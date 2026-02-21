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

# Target: host (cmake .. && make) or ARM (cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DBUILD_FIRMWARE_ARM=ON ..)
option(BUILD_FIRMWARE_ARM "Build for ARM (STM32WLE5); OFF = host build" OFF)
option(USE_STM32WL_RADIO "Use STM32WL SubGHz HAL from CubeWL submodule" ON)
option(USE_NANOPB       "Use nanopb runtime from submodule" ON)
option(BUILD_AS_LIBRARY "Build only static library (no executable)" OFF)

# Firmware sources
set(FIRMWARE_SOURCES
  ${CORE_DIR}/main.c
  ${CORE_DIR}/main_loop.c
  ${CORE_DIR}/led.c
  ${CORE_DIR}/debug_uart.c
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

# ---- ARM (STM32) build ----
# For ARM pass: -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DBUILD_FIRMWARE_ARM=ON
if(BUILD_FIRMWARE_ARM)
  # Submodule: STM32CubeWL (HAL, CMSIS, startup)
  include(${PROJECT_ROOT}/cmake/STM32CubeWL.cmake)
  # Use platform linker script (single 64K RAM, 256K Flash) like working Wio-E5 example
  set(STM32_LINKER_SCRIPT ${PROJECT_ROOT}/platform/stm32wle5/STM32WLE5JC.ld)

  # Submodule: nanopb runtime (optional)
  if(USE_NANOPB)
    include(${PROJECT_ROOT}/cmake/NanopbRuntime.cmake)
  endif()

  # HAL library from CubeWL (stm32wlxx_hal_conf.h in config/)
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

  # system_stm32wlxx.c from CMSIS Device (called from startup)
  set(CUBE_SYSTEM_C ${CUBE_CMSIS_DEVICE}/Source/Templates/system_stm32wlxx.c)
  if(NOT EXISTS "${CUBE_SYSTEM_C}")
    message(FATAL_ERROR "system_stm32wlxx.c not found at ${CUBE_SYSTEM_C}")
  endif()

  # Firmware executable
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
    )
    target_compile_definitions(meshtastic_mini PUBLIC ${CUBE_HAL_DEFINITIONS})
    if(USE_STM32WL_RADIO)
      target_compile_definitions(meshtastic_mini PUBLIC USE_STM32WL_RADIO)
    endif()
    target_compile_options(meshtastic_mini PRIVATE -mcpu=cortex-m4 -mthumb -fdata-sections -ffunction-sections)
  endif()
  return()
endif()

# ---- Host build (no submodules, firmware only) ----
if(BUILD_AS_LIBRARY)
  add_library(meshtastic_mini STATIC ${FIRMWARE_SOURCES})
  target_include_directories(meshtastic_mini PUBLIC
    ${FIRMWARE_DIR} ${CORE_DIR} ${RADIO_DIR} ${MESH_DIR} ${SERIAL_DIR} ${CRYPTO_DIR} ${CONFIG_DIR})
  return()
endif()

add_executable(meshtastic_mini ${FIRMWARE_SOURCES})
target_include_directories(meshtastic_mini PRIVATE
  ${FIRMWARE_DIR} ${CORE_DIR} ${RADIO_DIR} ${MESH_DIR} ${SERIAL_DIR} ${CRYPTO_DIR} ${CONFIG_DIR})
