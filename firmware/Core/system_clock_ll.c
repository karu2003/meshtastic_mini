/**
 * System clock 48 MHz via MSI, using ST LL (no HAL).
 * Call before HAL_Init() so SysTick is configured for 48 MHz.
 */
#if defined(USE_HAL_DRIVER)

#include "stm32wlxx_ll_rcc.h"
#include "stm32wlxx_ll_pwr.h"
#include "stm32wlxx_ll_system.h"

extern void SystemCoreClockUpdate(void);

void SystemClock_Config_LL(void)
{
    /* 1. Flash latency for 48 MHz (2 wait states) */
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2) {}

    /* 2. Voltage scaling Range 1 for 48 MHz */
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    while (LL_PWR_IsActiveFlag_VOS() != 0) {}

    /* 3. Enable MSI and wait ready */
    LL_RCC_MSI_Enable();
    while (!LL_RCC_MSI_IsReady()) {}

    /* 4. MSI range 11 = 48 MHz */
    LL_RCC_MSI_EnableRangeSelection();
    LL_RCC_MSI_SetRange(LL_RCC_MSIRANGE_11);
    LL_RCC_MSI_SetCalibTrimming(0);

    /* 5. MSI as system clock */
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_MSI);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_MSI) {}

    /* 6. AHB/APB dividers */
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAHB3Prescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);

    /* 7. Update SystemCoreClock so HAL_Init() configures SysTick for 48 MHz */
    SystemCoreClockUpdate();
}

#endif
