/**
 * Config storage in last flash page (STM32WLE5: 256KB Flash, page 127 = 0x0803F800).
 * Layout: magic (4 bytes) + device_config_t. Load/save on boot and on set_config.
 */
#include "config_store.h"
#include <string.h>

#if defined(USE_HAL_DRIVER) && defined(HAL_FLASH_MODULE_ENABLED)
#include "stm32wlxx_hal.h"
#include "stm32wlxx_hal_flash.h"
#include "stm32wlxx_hal_flash_ex.h"

#define CONFIG_MAGIC        0x4D434647U  /* "MCFG" */
#define CONFIG_FLASH_PAGE   127
#define CONFIG_FLASH_ADDR   (FLASH_BASE + (CONFIG_FLASH_PAGE * FLASH_PAGE_SIZE))

static bool config_from_flash(device_config_t *cfg) {
    const uint32_t *p = (const uint32_t *)CONFIG_FLASH_ADDR;
    if (p[0] != CONFIG_MAGIC)
        return false;
    memcpy(cfg, p + 1, sizeof(device_config_t));
    return true;
}
#endif

/* Meshtastic default PSK for primary channel "LongFast" (AES-128) */
static const uint8_t meshtastic_default_psk[16] = {
    0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x05, 0x13,
    0xf0, 0x08, 0x8a, 0xa3, 0x5a, 0xd3, 0x4a, 0x1c
};

void config_set_defaults(device_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->node_id = 1;
    cfg->region = 0;           /* REGION_EU_868 */
    cfg->modem_preset = 4;     /* MODEM_LONG_FAST */
    memcpy(cfg->channel_psk, meshtastic_default_psk, 16);
}

bool config_load(device_config_t *cfg) {
    if (!cfg) return false;
#if defined(USE_HAL_DRIVER) && defined(HAL_FLASH_MODULE_ENABLED)
    if (config_from_flash(cfg))
        return true;
#endif
    config_set_defaults(cfg);
    return true;
}

bool config_save(const device_config_t *cfg) {
    if (!cfg) return false;
#if defined(USE_HAL_DRIVER) && defined(HAL_FLASH_MODULE_ENABLED)
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Page      = CONFIG_FLASH_PAGE,
        .NbPages   = 1,
    };
    uint32_t page_err = 0;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    if (HAL_FLASHEx_Erase(&erase, &page_err) != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }

    /* Program magic + config as doublewords (8 bytes). */
    uint32_t magic = CONFIG_MAGIC;
    uint8_t buf[4 + sizeof(device_config_t)];
    memcpy(buf, &magic, 4);
    memcpy(buf + 4, cfg, sizeof(device_config_t));

    const uint64_t *src = (const uint64_t *)buf;
    size_t n_dw = (4 + sizeof(device_config_t) + 7) / 8;
    uint32_t addr = CONFIG_FLASH_ADDR;

    for (size_t i = 0; i < n_dw; i++, addr += 8) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, src[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }

    HAL_FLASH_Lock();
#endif
    return true;
}
