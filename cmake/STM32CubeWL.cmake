# Include STM32CubeWL from submodule third_party/STM32CubeWL.
# Call from project root: include(cmake/STM32CubeWL.cmake)
# Requires: git submodule update --init --recursive

set(CUBE_ROOT ${CMAKE_SOURCE_DIR}/third_party/STM32CubeWL)
if(NOT EXISTS ${CUBE_ROOT}/Drivers)
  message(FATAL_ERROR "STM32CubeWL submodule not found. Run: git submodule update --init --recursive")
endif()

set(CUBE_HAL_DRIVER   ${CUBE_ROOT}/Drivers/STM32WLxx_HAL_Driver)
set(CUBE_CMSIS        ${CUBE_ROOT}/Drivers/CMSIS)
set(CUBE_CMSIS_DEVICE ${CUBE_CMSIS}/Device/ST/STM32WLxx)
set(CUBE_CMSIS_CORE   ${CUBE_CMSIS})

# HAL sources (minimal set for SubGHz, system, UART debug)
set(CUBE_HAL_SRCS
  ${CUBE_HAL_DRIVER}/Src/stm32wlxx_hal.c
  ${CUBE_HAL_DRIVER}/Src/stm32wlxx_hal_subghz.c
  ${CUBE_HAL_DRIVER}/Src/stm32wlxx_hal_cortex.c
  ${CUBE_HAL_DRIVER}/Src/stm32wlxx_hal_dma.c
  ${CUBE_HAL_DRIVER}/Src/stm32wlxx_hal_gpio.c
  ${CUBE_HAL_DRIVER}/Src/stm32wlxx_hal_rcc.c
  ${CUBE_HAL_DRIVER}/Src/stm32wlxx_hal_rcc_ex.c
  ${CUBE_HAL_DRIVER}/Src/stm32wlxx_hal_pwr.c
  ${CUBE_HAL_DRIVER}/Src/stm32wlxx_hal_pwr_ex.c
  ${CUBE_HAL_DRIVER}/Src/stm32wlxx_hal_uart.c
  ${CUBE_HAL_DRIVER}/Src/stm32wlxx_hal_uart_ex.c
)

# Startup and linker from CMSIS Device (cmsis_device_wl submodule). Wio-E5 = STM32WLE5xx.
set(CUBE_GCC_TEMPLATE ${CUBE_CMSIS_DEVICE}/Source/Templates/gcc)
set(CUBE_STARTUP_ASM ${CUBE_GCC_TEMPLATE}/startup_stm32wle5xx.s)
set(CUBE_LD_SCRIPT_DEFAULT "${CUBE_CMSIS_DEVICE}/Source/Templates/gcc/linker/STM32WLE5XX_FLASH.ld")
if(NOT EXISTS "${CUBE_STARTUP_ASM}")
  message(WARNING "STM32CubeWL: startup not found at ${CUBE_STARTUP_ASM}")
endif()
if(NOT EXISTS "${CUBE_LD_SCRIPT_DEFAULT}")
  message(WARNING "STM32CubeWL: linker script not found at ${CUBE_LD_SCRIPT_DEFAULT}")
endif()
set(STM32_LINKER_SCRIPT ${CUBE_LD_SCRIPT_DEFAULT} CACHE PATH "STM32 linker script")

# Definitions for Wio-E5 (STM32WLE5JC)
set(CUBE_HAL_DEFINITIONS STM32WLE5xx USE_HAL_DRIVER)
# Path to stm32wlxx_hal_conf.h (copy of HAL template in config/)
set(CUBE_HAL_CONF_DIR ${CMAKE_SOURCE_DIR}/config)
