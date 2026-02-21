#!/usr/bin/env bash
# build.sh — build, flash, and clean (CMake only).
# Usage: ./build.sh <build|flash|clean> [options]

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="${BUILD_DIR:-build-arm}"
BUILD_HOST=false
TOOLCHAIN_FILE="$SCRIPT_DIR/cmake/arm-none-eabi.cmake"

usage() {
  echo "Usage: $0 <command> [options]"
  echo "Commands:"
  echo "  build   Configure and build (default: ARM firmware). Use BUILD_HOST=1 for host."
  echo "  flash   Flash meshtastic_mini.elf to the device (openocd or st-flash)."
  echo "  clean   Remove build directories (build, build-arm)."
  echo "Options for build:"
  echo "  BUILD_HOST=1  Build for host instead of ARM (e.g. BUILD_HOST=1 $0 build)."
  exit 1
}

cmd_build() {
  if [ "$BUILD_HOST" = "1" ]; then
    BUILD_DIR="build"
    mkdir -p "$BUILD_DIR"
    ( cd "$BUILD_DIR"; cmake ..; cmake --build . )
    echo "Host build done: $BUILD_DIR/meshtastic_mini"
    return 0
  fi

  if [ ! -d "third_party/STM32CubeWL/Drivers/STM32WLxx_HAL_Driver/Src" ]; then
    echo "Run first: git submodule update --init --recursive"
    exit 1
  fi

  mkdir -p "$BUILD_DIR"
  ( cd "$BUILD_DIR"; cmake .. -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" -DBUILD_FIRMWARE_ARM=ON; cmake --build . )
  echo "ARM build done: $BUILD_DIR/meshtastic_mini.elf"
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
    echo "  openocd: -f interface/stlink.cfg -f target/stm32wlx.cfg -c 'program $ELF verify reset exit'"
    echo "  st-flash: st-flash write $ELF 0x08000000"
    exit 1
  fi
  echo "Flash done."
}

cmd_clean() {
  for d in build build-arm; do
    if [ -d "$d" ]; then
      rm -rf "$d"
      echo "Removed $d"
    fi
  done
  echo "Clean done."
}

case "${1:-build}" in
  build)
    BUILD_HOST="${BUILD_HOST:-0}"
    cmd_build
    ;;
  flash)
    cmd_flash
    ;;
  clean)
    cmd_clean
    ;;
  -h|--help)
    usage
    ;;
  *)
    echo "Unknown command: $1"
    usage
    ;;
esac
