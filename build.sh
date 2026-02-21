#!/usr/bin/env bash
# build.sh — build and flash for STM32WLE5JC (single target).
# Usage: ./build.sh <build|flash|clean>

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="${BUILD_DIR:-build}"
TOOLCHAIN_FILE="$SCRIPT_DIR/cmake/arm-none-eabi.cmake"

usage() {
  echo "Usage: $0 <command>"
  echo "Commands:"
  echo "  build   Configure and build (STM32WLE5JC)."
  echo "  flash   Flash meshtastic_mini.elf to the device (openocd or st-flash)."
  echo "  clean   Remove build directory."
  exit 1
}

cmd_build() {
  if [ ! -d "third_party/STM32CubeWL/Drivers/STM32WLxx_HAL_Driver/Src" ]; then
    echo "Run first: git submodule update --init --recursive"
    exit 1
  fi

  mkdir -p "$BUILD_DIR"
  ( cd "$BUILD_DIR"; cmake .. -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"; cmake --build . )
  echo "Build done: $BUILD_DIR/meshtastic_mini.elf"
}

cmd_flash() {
  ELF="$SCRIPT_DIR/$BUILD_DIR/meshtastic_mini.elf"
  if [ ! -f "$ELF" ]; then
    echo "No $ELF. Run: $0 build"
    exit 1
  fi

  if command -v openocd &>/dev/null; then
    echo "Flashing with OpenOCD..."
    openocd -f interface/stlink.cfg -f target/stm32wlx.cfg \
      -c "program $ELF verify reset exit"
  elif command -v st-flash &>/dev/null; then
    echo "Flashing with st-flash..."
    st-flash write "$ELF" 0x08000000
  else
    echo "Install openocd or stlink (st-flash) to flash."
    exit 1
  fi
  echo "Flash done."
}

cmd_clean() {
  if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
    echo "Removed $BUILD_DIR"
  fi
  echo "Clean done."
}

case "${1:-build}" in
  build)  cmd_build ;;
  flash)  cmd_flash ;;
  clean)  cmd_clean ;;
  -h|--help) usage ;;
  *)
    echo "Unknown command: $1"
    usage
    ;;
esac
