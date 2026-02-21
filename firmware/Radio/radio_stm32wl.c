/**
 * Radio implementation for STM32WLE5 via SubGHz HAL (STM32CubeWL).
 * Full LoRa init: Standby, PacketType, RFFrequency, ModulationParams,
 * PacketParams, DIO IRQ, then SetRx. TX via WriteBuffer + SetTx; RX via
 * IRQ callback flag and rx_poll read + SetRx restart.
 */
#include "radio_phy.h"
#include "serial_io.h"
#include <string.h>

/* SX1262: version string (0x0137, discrete only); REG_OCP (0x08E7) R/W for SPI write-read test */
#define SX1262_REG_VERSION  0x0137U
#define SX1262_VERSION_LEN  16
#define SPI_TEST_PATTERN    0xA5U   /* write then read back; neither 0x00 nor 0xFF */

#if defined(USE_STM32WL_RADIO) && defined(STM32WLE5xx)
#include "stm32wlxx_hal.h"
#include "stm32wlxx_hal_subghz.h"
#include "stm32wlxx_ll_rcc.h"
#include "stm32wlxx_ll_pwr.h"
#include "stm32wlxx_ll_exti.h"
#include "rf_ctrl.h"

/* Wait for radio busy to clear (PWR SR2 RFBUSYS), as radio_pair subghz_wait_busy() */
static void subghz_wait_busy(void) {
    for (volatile uint32_t i = 0; i < 1000000; i++) {
        if (!LL_PWR_IsActiveFlag_RFBUSYS())
            return;
    }
}
/* SX1262: 32 MHz crystal */
#define XTAL_FREQ_HZ  32000000u

/* Meshtastic-compatible LoRa: preamble 16 symbols, sync word 0x2B (REG_LR_SYNCWORD = 0x0740) */
#define MESHTASTIC_LORA_PREAMBLE_LEN  16
#define REG_LR_SYNCWORD               0x0740U
#define REG_OCP                       0x08E7U   /* SX1262 over-current protection */
#define REG_TX_CLAMP                  0x08D8U   /* TX clamp (ST workaround for RFO_HP) */

/* Default EU868 for first listen until lora_set_region_preset is called */
#define DEFAULT_FREQ_HZ  868100000u
#define DEFAULT_SF       11
#define DEFAULT_BW_HZ    250000u
#define DEFAULT_CR       5  /* 4/5 */

static SUBGHZ_HandleTypeDef hsubghz;
static int16_t last_rssi;
static int8_t  last_snr;
static uint8_t rx_buf[256];
static uint16_t rx_len;
static volatile bool rx_pending;  /* set in IRQ callback, consumed in rx_poll */
static volatile bool tx_done_flag; /* set in TxCplt IRQ callback */

static uint32_t freq_to_rf_reg(uint32_t freq_hz) {
    return (uint32_t)(((uint64_t)freq_hz << 25) / XTAL_FREQ_HZ);
}

/* Convert BW (Hz) to SX1262 LoRa BW register value (SX1262 DS Table 13-47) */
static uint8_t bw_to_param(uint32_t bw_hz) {
    if (bw_hz <= 7810)   return 0x00;
    if (bw_hz <= 10420)  return 0x08;
    if (bw_hz <= 15630)  return 0x01;
    if (bw_hz <= 20830)  return 0x09;
    if (bw_hz <= 31250)  return 0x02;
    if (bw_hz <= 41670)  return 0x0A;
    if (bw_hz <= 62500)  return 0x03;
    if (bw_hz <= 125000) return 0x04;
    if (bw_hz <= 250000) return 0x05;
    return 0x06;
}

/* CR 5->1, 6->2, 7->3, 8->4 (4/5 .. 4/8) */
static uint8_t cr_to_param(uint8_t cr) {
    if (cr <= 5) return 1;
    if (cr <= 6) return 2;
    if (cr <= 7) return 3;
    return 4;
}

static void radio_apply_lora_params(uint32_t freq_hz, uint8_t sf, uint32_t bw_hz, uint8_t cr) {
    uint8_t buf[8];

    buf[0] = 0x00;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_STANDBY, buf, 1);
    subghz_wait_busy();

    buf[0] = 0x01;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_REGULATORMODE, buf, 1);
    subghz_wait_busy();

    /*
     * TCXO auto-detect: try TCXO first, check for XOSC error, fall back to crystal.
     * LoRa-E5 modules have TCXO on DIO3; some boards use a plain crystal instead.
     */
#if !defined(WIO_E5_NO_TCXO)
    /* Clear previous errors so we read fresh status */
    { uint8_t clr[2] = {0, 0}; HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_CLR_ERROR, clr, 2); }
    subghz_wait_busy();

    buf[0] = 0x01; buf[1] = 0x00; buf[2] = 0x02; buf[3] = 0x80;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_TCXOMODE, buf, 4);
    subghz_wait_busy();

    /* Check XOSC error (bit 5) — if set, TCXO did not start → fall back to crystal */
    {
        uint8_t err[2] = {0, 0};
        HAL_SUBGHZ_ExecGetCmd(&hsubghz, RADIO_GET_ERROR, err, 2);
        uint16_t dev_err = (uint16_t)((err[0] << 8) | err[1]);
        if (dev_err & 0x0020U) {
            LL_RCC_RF_EnableReset();
            for (volatile int i = 0; i < 1000; i++) { (void)i; }
            LL_RCC_RF_DisableReset();
            HAL_Delay(5);
            subghz_wait_busy();
            buf[0] = 0x00;
            HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_STANDBY, buf, 1);
            subghz_wait_busy();
            buf[0] = 0x01;
            HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_REGULATORMODE, buf, 1);
            subghz_wait_busy();
        }
    }
#endif

    buf[0] = 0x7F;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_CALIBRATE, buf, 1);
    subghz_wait_busy();
    HAL_Delay(10);

    /* CalibrateImage for 863–870 MHz band (critical for RX sensitivity) */
    buf[0] = 0xD7;
    buf[1] = 0xDB;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_CALIBRATEIMAGE, buf, 2);

    /* Fallback to STDBY_RC on RX/TX timeout */
    buf[0] = 0x20;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_TXFALLBACKMODE, buf, 1);

    /* RADIO_SET_PACKETTYPE = LoRa (0x01) */
    buf[0] = 0x01;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACKETTYPE, buf, 1);

    /* RADIO_SET_RFFREQUENCY: 4 bytes big-endian */
    uint32_t rf = freq_to_rf_reg(freq_hz);
    buf[0] = (uint8_t)(rf >> 24);
    buf[1] = (uint8_t)(rf >> 16);
    buf[2] = (uint8_t)(rf >> 8);
    buf[3] = (uint8_t)(rf);
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RFFREQUENCY, buf, 4);

    /* RADIO_SET_MODULATIONPARAMS LoRa: SF, BW, CR, LDRO (4 bytes) */
    buf[0] = sf;
    buf[1] = bw_to_param(bw_hz);
    buf[2] = cr_to_param(cr);
    buf[3] = (sf >= 11 && bw_hz <= 125000) ? 1 : 0;  /* LowDataRateOptimize */
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_MODULATIONPARAMS, buf, 4);

    /* RADIO_SET_PACKETPARAMS LoRa: PreambleLen(2), HeaderType(0=explicit), PayloadLen(0xFF), CrcOn(1), InvertIQ(0) */
    buf[0] = (uint8_t)(MESHTASTIC_LORA_PREAMBLE_LEN >> 8);
    buf[1] = (uint8_t)(MESHTASTIC_LORA_PREAMBLE_LEN);
    buf[2] = 0x00;    /* explicit header */
    buf[3] = 0xFF;    /* max payload for RX */
    buf[4] = 0x01;    /* CRC on */
    buf[5] = 0x00;    /* normal IQ */
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACKETPARAMS, buf, 6);

    /* LoRa sync word for Meshtastic compatibility: 0x2B (second byte 0x44 recommended for SX126x) */
    uint8_t sync[] = { 0x2B, 0x44 };
    HAL_SUBGHZ_WriteRegisters(&hsubghz, REG_LR_SYNCWORD, sync, 2);

    /* RADIO_SET_BUFFERBASEADDRESS: TxBase=0, RxBase=128 (as radio_pair) */
    buf[0] = 0x00;
    buf[1] = 0x80;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_BUFFERBASEADDRESS, buf, 2);

    /* RF switch: DIO2 (0x01) + PA4/PA5 from rf_ctrl — same as radio_pair */
    buf[0] = 0x01;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RFSWITCHMODE, buf, 1);

    /* PA: RFO_HP 14 dBm (0x02,0x02) — matches radio_pair. For LP-only board use WIO_E5_USE_LP=1. */
#if defined(WIO_E5_USE_LP)
    buf[0] = 0x04; buf[1] = 0x00; buf[2] = 0x01; buf[3] = 0x01;  /* RFO_LP 14 dBm */
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACONFIG, buf, 4);
    { uint8_t ocp = 0x18; HAL_SUBGHZ_WriteRegisters(&hsubghz, REG_OCP, &ocp, 1); }
    buf[0] = 0x0E; buf[1] = 0x04;  /* TX power LP 14 dBm */
#else
    buf[0] = 0x02; buf[1] = 0x02; buf[2] = 0x00; buf[3] = 0x01;  /* RFO_HP 14 dBm */
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACONFIG, buf, 4);
    /* ST workaround: RFO HP resistance to antenna mismatch (radio_driver.c) */
    { uint8_t v; HAL_SUBGHZ_ReadRegister(&hsubghz, REG_TX_CLAMP, &v); v |= (0x0FU << 1); HAL_SUBGHZ_WriteRegisters(&hsubghz, REG_TX_CLAMP, &v, 1); }
    { uint8_t ocp = 0x38; HAL_SUBGHZ_WriteRegisters(&hsubghz, REG_OCP, &ocp, 1); }
    buf[0] = 14; buf[1] = 0x04;   /* TX power HP 14 dBm */
#endif
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_TXPARAMS, buf, 2);

    /* RADIO_CFG_DIOIRQ: TX_DONE(0) | RX_DONE(1) | TIMEOUT(9) on DIO1 */
    buf[0] = 0x02;
    buf[1] = 0x03;    /* IrqMask: 0x0203 */
    buf[2] = 0x02;
    buf[3] = 0x03;    /* Dio1Mask: 0x0203 */
    buf[4] = 0x00;
    buf[5] = 0x00;
    buf[6] = 0x00;
    buf[7] = 0x00;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_CFG_DIOIRQ, buf, 8);
}

/* Apply PA config and SetTxParams in STDBY — same as radio_pair before each TX */
static void apply_pa_tx_params(void) {
    uint8_t buf[4];
#if defined(WIO_E5_USE_LP)
    buf[0] = 0x04; buf[1] = 0x00; buf[2] = 0x01; buf[3] = 0x01;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACONFIG, buf, 4);
    { uint8_t ocp = 0x18; HAL_SUBGHZ_WriteRegisters(&hsubghz, REG_OCP, &ocp, 1); }
    buf[0] = 0x0E; buf[1] = 0x04;
#else
    buf[0] = 0x02; buf[1] = 0x02; buf[2] = 0x00; buf[3] = 0x01;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACONFIG, buf, 4);
    { uint8_t v; HAL_SUBGHZ_ReadRegister(&hsubghz, REG_TX_CLAMP, &v); v |= (0x0FU << 1); HAL_SUBGHZ_WriteRegisters(&hsubghz, REG_TX_CLAMP, &v, 1); }
    { uint8_t ocp = 0x38; HAL_SUBGHZ_WriteRegisters(&hsubghz, REG_OCP, &ocp, 1); }
    buf[0] = 14; buf[1] = 0x04;
#endif
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_TXPARAMS, buf, 2);
}

/* Init SUBGHZSPI before radio reset — same order as radio_pair: subghz_spi_init() then subghz_reset() */
static void subghz_spi_init_before_reset(void) {
    __HAL_RCC_SUBGHZSPI_CLK_ENABLE();
    __DSB();
    /* SPI: master, CPOL=0 CPHA=0, software NSS, 8-bit, prescaler 2 (f/2) — match radio_pair + HAL */
    CLEAR_BIT(SUBGHZSPI->CR1, SPI_CR1_SPE);
    SUBGHZSPI->CR1 = SPI_CR1_MSTR | SPI_CR1_SSI | SPI_CR1_SSM | SUBGHZSPI_BAUDRATEPRESCALER_2;
    SUBGHZSPI->CR2 = SPI_CR2_FRXTH | SPI_CR2_DS_0 | SPI_CR2_DS_1 | SPI_CR2_DS_2;
    SET_BIT(SUBGHZSPI->CR1, SPI_CR1_SPE);
    LL_PWR_UnselectSUBGHZSPI_NSS();
}

static bool stm32wl_radio_init(void) {
    rf_ctrl_init();
    memset(&hsubghz, 0, sizeof(hsubghz));
    rx_pending = false;
    rx_len = 0;
    last_rssi = 0;
    last_snr = 0;

    /*
     * Init sequence matches radio_pair exactly:
     *   1) SPI init  2) radio reset  3) standby  4) DC-DC  5) TCXO  6) calibrate
     * We skip HAL_SUBGHZ_Init() because it sets DeepSleep=ENABLE and adds
     * extra SPI traffic between reset and TCXO, which causes XOSC start error.
     */
    hsubghz.Init.BaudratePrescaler = SUBGHZSPI_BAUDRATEPRESCALER_2;
    subghz_spi_init_before_reset();

    LL_RCC_RF_EnableReset();
    for (volatile int i = 0; i < 1000; i++) { (void)i; }
    LL_RCC_RF_DisableReset();
    HAL_Delay(5);
    subghz_wait_busy();

    /* Manual HAL state setup (replaces HAL_SUBGHZ_Init) */
    HAL_NVIC_SetPriority(SUBGHZ_Radio_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
    LL_EXTI_EnableRisingTrig_32_63(LL_EXTI_LINE_44);
    LL_EXTI_EnableIT_32_63(LL_EXTI_LINE_44);
    LL_PWR_SetRadioBusyTrigger(LL_PWR_RADIO_BUSY_TRIGGER_WU_IT);
    LL_PWR_ClearFlag_RFBUSY();
    hsubghz.DeepSleep = 0;
    hsubghz.ErrorCode = HAL_SUBGHZ_ERROR_NONE;
    hsubghz.State = HAL_SUBGHZ_STATE_READY;

    rf_ctrl_set_off();
    radio_apply_lora_params(DEFAULT_FREQ_HZ, DEFAULT_SF, DEFAULT_BW_HZ, DEFAULT_CR);

    { uint8_t clr[2] = { 0xFF, 0xFF }; HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_CLR_IRQSTATUS, clr, 2); }
    subghz_wait_busy();
    rf_ctrl_set_rx();
    uint8_t rx_params[3] = { 0xFF, 0xFF, 0xFF };
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RX, rx_params, 3);
    HAL_Delay(5);
    return true;
}

static bool stm32wl_radio_set_freq(uint32_t freq_hz) {
    uint8_t buf[4];
    uint8_t standby[] = { 0x00 };
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_STANDBY, standby, 1);
    uint32_t rf = freq_to_rf_reg(freq_hz);
    buf[0] = (uint8_t)(rf >> 24);
    buf[1] = (uint8_t)(rf >> 16);
    buf[2] = (uint8_t)(rf >> 8);
    buf[3] = (uint8_t)(rf);
    if (HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RFFREQUENCY, buf, 4) != HAL_OK)
        return false;
    subghz_wait_busy();
    rf_ctrl_set_rx();
    uint8_t rx_params[3] = { 0xFF, 0xFF, 0xFF };
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RX, rx_params, 3);
    return true;
}

static bool stm32wl_radio_set_lora(uint8_t sf, uint32_t bw_hz, uint8_t cr) {
    uint8_t buf[8];
    uint8_t standby[] = { 0x00 };
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_STANDBY, standby, 1);
    buf[0] = sf;
    buf[1] = bw_to_param(bw_hz);
    buf[2] = cr_to_param(cr);
    buf[3] = (sf >= 11 && bw_hz <= 125000) ? 1 : 0;
    if (HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_MODULATIONPARAMS, buf, 4) != HAL_OK)
        return false;
    buf[0] = (uint8_t)(MESHTASTIC_LORA_PREAMBLE_LEN >> 8);
    buf[1] = (uint8_t)(MESHTASTIC_LORA_PREAMBLE_LEN);
    buf[2] = 0x00;
    buf[3] = 0xFF;
    buf[4] = 0x01;
    buf[5] = 0x00;
    if (HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACKETPARAMS, buf, 6) != HAL_OK)
        return false;
    uint8_t sync[] = { 0x2B, 0x44 };
    HAL_SUBGHZ_WriteRegisters(&hsubghz, REG_LR_SYNCWORD, sync, 2);
    subghz_wait_busy();
    rf_ctrl_set_rx();
    uint8_t rx_params[3] = { 0xFF, 0xFF, 0xFF };
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RX, rx_params, 3);
    return true;
}

static bool stm32wl_radio_tx(const uint8_t *data, uint16_t len) {
    if (!data || len > 256) return false;

    /* Disable radio IRQ for the entire TX cycle to prevent HAL reentrancy:
     * the IRQ handler calls HAL SPI functions which conflict with our calls. */
    HAL_NVIC_DisableIRQ(SUBGHZ_Radio_IRQn);

    uint8_t standby[] = { 0x00 };
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_STANDBY, standby, 1);
    subghz_wait_busy();

    apply_pa_tx_params();
    subghz_wait_busy();

    rf_ctrl_set_off();

    { uint8_t clr[2] = { 0xFF, 0xFF }; HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_CLR_IRQSTATUS, clr, 2); }

    uint8_t pkt[6] = {
        (uint8_t)(MESHTASTIC_LORA_PREAMBLE_LEN >> 8),
        (uint8_t)(MESHTASTIC_LORA_PREAMBLE_LEN),
        0x00,       /* explicit header */
        (uint8_t)len,
        0x01,       /* CRC on */
        0x00,       /* normal IQ */
    };
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACKETPARAMS, pkt, 6);

    if (HAL_SUBGHZ_WriteBuffer(&hsubghz, 0, (uint8_t *)data, len) != HAL_OK) {
        HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
        return false;
    }
    subghz_wait_busy();

    rf_ctrl_set_tx();
    subghz_wait_busy();

    uint8_t tx_to[3] = { 0x02, 0xEE, 0x00 };
    if (HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_TX, tx_to, 3) != HAL_OK) {
        rf_ctrl_set_rx();
        HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
        return false;
    }

    /* Poll GetIrqStatus directly — safe because IRQ handler is disabled */
    uint32_t t0 = HAL_GetTick();
    uint16_t final_irq = 0;
    bool tx_ok = false;
    while ((HAL_GetTick() - t0) < 4000u) {
        uint8_t irq[2];
        if (HAL_SUBGHZ_ExecGetCmd(&hsubghz, RADIO_GET_IRQSTATUS, irq, 2) == HAL_OK) {
            final_irq = (uint16_t)((irq[0] << 8) | irq[1]);
            if (final_irq & 0x0201u) {  /* TX_DONE(bit0) or TIMEOUT(bit9) */
                tx_ok = (final_irq & 0x01u) != 0;
                break;
            }
        }
    }
    /* Return to RX */
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_STANDBY, standby, 1);
    subghz_wait_busy();
    rf_ctrl_set_off();
    { uint8_t clr[2] = { 0xFF, 0xFF }; HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_CLR_IRQSTATUS, clr, 2); }
    pkt[3] = 0xFF;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACKETPARAMS, pkt, 6);
    subghz_wait_busy();
    rf_ctrl_set_rx();
    { uint8_t rx_p[3] = { 0xFF, 0xFF, 0xFF }; HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RX, rx_p, 3); }

    /* Clear any pending NVIC IRQ accumulated during TX, then re-enable for RX */
    NVIC_ClearPendingIRQ(SUBGHZ_Radio_IRQn);
    HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
    return tx_ok;
}

static uint16_t stm32wl_radio_rx_poll(uint8_t *buf, uint16_t max_len) {
    if (!buf || max_len == 0) return 0;
    if (!rx_pending) return 0;

    /* Disable radio IRQ while we access HAL SPI to prevent reentrancy */
    HAL_NVIC_DisableIRQ(SUBGHZ_Radio_IRQn);
    rx_pending = false;

    uint8_t status[2];
    if (HAL_SUBGHZ_ExecGetCmd(&hsubghz, RADIO_GET_RXBUFFERSTATUS, status, 2) != HAL_OK) {
        NVIC_ClearPendingIRQ(SUBGHZ_Radio_IRQn);
        HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
        return 0;
    }
    uint8_t payload_len = status[0];
    uint8_t offset = status[1];
    if (payload_len == 0 || payload_len > max_len || payload_len > sizeof(rx_buf)) {
        subghz_wait_busy();
        rf_ctrl_set_rx();
        { uint8_t rx_p[3] = { 0xFF, 0xFF, 0xFF }; HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RX, rx_p, 3); }
        NVIC_ClearPendingIRQ(SUBGHZ_Radio_IRQn);
        HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
        return 0;
    }
    if (HAL_SUBGHZ_ReadBuffer(&hsubghz, offset, rx_buf, payload_len) != HAL_OK) {
        subghz_wait_busy();
        rf_ctrl_set_rx();
        { uint8_t rx_p[3] = { 0xFF, 0xFF, 0xFF }; HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RX, rx_p, 3); }
        NVIC_ClearPendingIRQ(SUBGHZ_Radio_IRQn);
        HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
        return 0;
    }
    memcpy(buf, rx_buf, payload_len);
    rx_len = payload_len;

    { uint8_t clr[2] = { 0xFF, 0xFF }; HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_CLR_IRQSTATUS, clr, 2); }

    uint8_t pkt_status[3];
    if (HAL_SUBGHZ_ExecGetCmd(&hsubghz, RADIO_GET_PACKETSTATUS, pkt_status, 3) == HAL_OK) {
        last_rssi = (int16_t)(-(int8_t)pkt_status[1] / 2);
        last_snr = (int8_t)pkt_status[2] / 4;
    }

    subghz_wait_busy();
    rf_ctrl_set_rx();
    { uint8_t rx_p[3] = { 0xFF, 0xFF, 0xFF }; HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RX, rx_p, 3); }

    NVIC_ClearPendingIRQ(SUBGHZ_Radio_IRQn);
    HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
    return payload_len;
}

static void stm32wl_radio_get_rssi_snr(int16_t *rssi, int8_t *snr) {
    if (rssi) *rssi = last_rssi;
    if (snr)  *snr  = last_snr;
}

static const radio_phy_ops_t stm32wl_ops = {
    .init = stm32wl_radio_init,
    .set_freq = stm32wl_radio_set_freq,
    .set_lora = stm32wl_radio_set_lora,
    .tx = stm32wl_radio_tx,
    .rx_poll = stm32wl_radio_rx_poll,
    .get_last_rssi_snr = stm32wl_radio_get_rssi_snr,
};

void radio_stm32wl_register(void) {
    radio_phy_set_ops(&stm32wl_ops);
}

/* ----- MspInit: enable SUBGHZSPI clock and Radio IRQ (order as in radio_pair) ----- */
void HAL_SUBGHZ_MspInit(SUBGHZ_HandleTypeDef *hsubghz_handle) {
    (void)hsubghz_handle;
    __HAL_RCC_SUBGHZSPI_CLK_ENABLE();
    __DSB();  /* as in radio_pair subghz_spi_init() */
    HAL_NVIC_SetPriority(SUBGHZ_Radio_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
}

void HAL_SUBGHZ_MspDeInit(SUBGHZ_HandleTypeDef *hsubghz_handle) {
    (void)hsubghz_handle;
    HAL_NVIC_DisableIRQ(SUBGHZ_Radio_IRQn);
    __HAL_RCC_SUBGHZSPI_CLK_DISABLE();
}

/* Called from HAL when RX complete IRQ is detected */
void HAL_SUBGHZ_RxCpltCallback(SUBGHZ_HandleTypeDef *h) {
    (void)h;
    rx_pending = true;
}

void HAL_SUBGHZ_TxCpltCallback(SUBGHZ_HandleTypeDef *h) {
    (void)h;
    tx_done_flag = true;
}

void HAL_SUBGHZ_RxTxTimeoutCallback(SUBGHZ_HandleTypeDef *h) {
    (void)h;
    /* tx_done_flag stays false — polling loop will report TIMEOUT */
}

void SUBGHZ_Radio_IRQHandler(void) {
    HAL_SUBGHZ_IRQHandler(&hsubghz);
}

#else
void radio_stm32wl_register(void) { (void)0; }
#endif
