cmake_minimum_required(VERSION 3.16)
project(meshtastic_mini LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)

# All dependencies via submodules (third_party/). Init: git submodule update --init --recursive

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(FIRMWARE_DIR ${PROJECT_ROOT}/firmware)
set(CORE_DIR    ${FIRMWARE_DIR}/Core)
set(RADIO_DIR   ${FIRMWARE_DIR}/Radio)
set(MESH_DIR    ${FIRMWARE_DIR}/Mesh)
set(SERIAL_DIR  ${FIRMWARE_DIR}/Serial)
set(CRYPTO_DIR  ${FIRMWARE_DIR}/Crypto)
set(CONFIG_DIR  ${FIRMWARE_DIR}/Config)
set(THIRD_PARTY ${PROJECT_ROOT}/third_party)

# Single target: STM32WLE5JC. Use: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -B build .
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
  message(FATAL_ERROR "This project builds only for STM32WLE5JC. Run: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -B build .")
endif()

option(USE_STM32WL_RADIO "Use STM32WL SubGHz HAL from CubeWL submodule" ON)
option(WIO_E5_RF_SWAP "Swap PA4/PA5 polarity for RF switch (try if no TX/RX)" OFF)
# Default OFF: use RFO_HP (same as radio_pair — proven to work on Wio-E5/Wio-E5-LE).
# Set -DWIO_E5_USE_LP=ON only if your board routes RFO_LP to the antenna.
option(WIO_E5_USE_LP "Use RFO_LP 14dBm instead of RFO_HP" OFF)
# Default OFF: enable TCXO (same as radio_pair — Wio-E5/Wio-E5-LE have TCXO on DIO3).
# Set -DWIO_E5_NO_TCXO=ON only if your board truly has no TCXO.
option(WIO_E5_NO_TCXO "Disable TCXO (for boards without TCXO)" OFF)
option(USE_NANOPB       "Use nanopb runtime from submodule" ON)
option(BUILD_AS_LIBRARY "Build only static library (no executable)" OFF)

# Firmware sources
set(FIRMWARE_SOURCES
  ${CORE_DIR}/main.c
  ${CORE_DIR}/main_loop.c
  ${CORE_DIR}/led.c
  ${CORE_DIR}/serial_io.c
  ${CORE_DIR}/system_clock_ll.c
  ${CORE_DIR}/stm32wlxx_it.c
  ${CORE_DIR}/syscalls_stub.c
  ${RADIO_DIR}/radio_phy.c
  ${RADIO_DIR}/rf_ctrl.c
  ${RADIO_DIR}/lora_meshtastic.c
  ${RADIO_DIR}/radio_stm32wl.c
  ${MESH_DIR}/mesh_packet.c
  ${MESH_DIR}/flood_router.c
  ${SERIAL_DIR}/serial_framing.c
  ${CRYPTO_DIR}/aes_meshtastic.c
  ${CONFIG_DIR}/config_store.c
)

# ---- STM32WLE5JC (single target) ----
include(${PROJECT_ROOT}/cmake/STM32CubeWL.cmake)
list(FIND CUBE_HAL_DEFINITIONS USE_HAL_DRIVER _idx_hal)
list(FIND CUBE_HAL_DEFINITIONS STM32WLE5xx _idx_chip)
if(_idx_hal EQUAL -1 OR _idx_chip EQUAL -1)
  message(FATAL_ERROR "USE_HAL_DRIVER and STM32WLE5xx required in CUBE_HAL_DEFINITIONS")
endif()
set(STM32_LINKER_SCRIPT ${PROJECT_ROOT}/platform/stm32wle5/STM32WLE5JC.ld)

if(USE_NANOPB)
  include(${PROJECT_ROOT}/cmake/NanopbRuntime.cmake)
endif()

add_library(cube_hal STATIC ${CUBE_HAL_SRCS})
target_include_directories(cube_hal PUBLIC
    ${CUBE_HAL_DRIVER}/Inc
    ${CUBE_CMSIS_DEVICE}/Include
    ${CUBE_CMSIS_CORE}/Include
  ${CUBE_HAL_CONF_DIR}
)
target_compile_definitions(cube_hal PUBLIC ${CUBE_HAL_DEFINITIONS})
target_compile_options(cube_hal PRIVATE -mcpu=cortex-m4 -mthumb -fdata-sections -ffunction-sections)

if(USE_STM32WL_RADIO)
  target_compile_definitions(cube_hal PUBLIC USE_STM32WL_RADIO)
endif()

set(CUBE_SYSTEM_C ${CUBE_CMSIS_DEVICE}/Source/Templates/system_stm32wlxx.c)
if(NOT EXISTS "${CUBE_SYSTEM_C}")
  message(FATAL_ERROR "system_stm32wlxx.c not found at ${CUBE_SYSTEM_C}")
endif()

if(NOT BUILD_AS_LIBRARY)
  add_executable(meshtastic_mini.elf ${FIRMWARE_SOURCES} ${CUBE_STARTUP_ASM} ${CUBE_SYSTEM_C})
  target_link_libraries(meshtastic_mini.elf PRIVATE cube_hal)
  if(USE_NANOPB)
    target_sources(meshtastic_mini.elf PRIVATE ${NANOPB_RUNTIME_SRCS})
    target_include_directories(meshtastic_mini.elf PRIVATE ${NANOPB_INCLUDE_DIR})
  endif()
  target_include_directories(meshtastic_mini.elf PRIVATE
    ${FIRMWARE_DIR} ${CORE_DIR} ${RADIO_DIR} ${MESH_DIR} ${SERIAL_DIR} ${CRYPTO_DIR} ${CONFIG_DIR}
    ${CUBE_CMSIS_DEVICE}/Include ${CUBE_CMSIS_CORE}/Include ${CUBE_HAL_DRIVER}/Inc ${CUBE_HAL_CONF_DIR}
  )
  target_compile_definitions(meshtastic_mini.elf PRIVATE ${CUBE_HAL_DEFINITIONS})
  if(USE_STM32WL_RADIO)
    target_compile_definitions(meshtastic_mini.elf PRIVATE USE_STM32WL_RADIO)
  endif()
  if(WIO_E5_RF_SWAP)
    target_compile_definitions(meshtastic_mini.elf PRIVATE WIO_E5_RF_SWAP=1)
  endif()
  if(WIO_E5_USE_LP)
    target_compile_definitions(meshtastic_mini.elf PRIVATE WIO_E5_USE_LP=1)
  endif()
  if(WIO_E5_NO_TCXO)
    target_compile_definitions(meshtastic_mini.elf PRIVATE WIO_E5_NO_TCXO=1)
  endif()
  target_compile_options(meshtastic_mini.elf PRIVATE -mcpu=cortex-m4 -mthumb -fdata-sections -ffunction-sections)
  target_link_options(meshtastic_mini.elf PRIVATE
    -mcpu=cortex-m4 -mthumb -Wl,--gc-sections -specs=nano.specs -specs=nosys.specs
    -T "${STM32_LINKER_SCRIPT}"
  )
else()
  add_library(meshtastic_mini STATIC ${FIRMWARE_SOURCES})
  target_link_libraries(meshtastic_mini PUBLIC cube_hal)
  if(USE_NANOPB)
    target_sources(meshtastic_mini PRIVATE ${NANOPB_RUNTIME_SRCS})
    target_include_directories(meshtastic_mini PUBLIC ${NANOPB_INCLUDE_DIR})
  endif()
  target_include_directories(meshtastic_mini PUBLIC
    ${FIRMWARE_DIR} ${CORE_DIR} ${RADIO_DIR} ${MESH_DIR} ${SERIAL_DIR} ${CRYPTO_DIR} ${CONFIG_DIR}
    ${CUBE_CMSIS_DEVICE}/Include ${CUBE_CMSIS_CORE}/Include ${CUBE_HAL_DRIVER}/Inc ${CUBE_HAL_CONF_DIR}
  )
  target_compile_definitions(meshtastic_mini PUBLIC ${CUBE_HAL_DEFINITIONS})
  if(USE_STM32WL_RADIO)
    target_compile_definitions(meshtastic_mini PUBLIC USE_STM32WL_RADIO)
  endif()
  if(WIO_E5_RF_SWAP)
    target_compile_definitions(meshtastic_mini PRIVATE WIO_E5_RF_SWAP=1)
  endif()
  if(WIO_E5_USE_LP)
    target_compile_definitions(meshtastic_mini PRIVATE WIO_E5_USE_LP=1)
  endif()
  if(WIO_E5_NO_TCXO)
    target_compile_definitions(meshtastic_mini PRIVATE WIO_E5_NO_TCXO=1)
  endif()
  target_compile_options(meshtastic_mini PRIVATE -mcpu=cortex-m4 -mthumb -fdata-sections -ffunction-sections)
endif()
