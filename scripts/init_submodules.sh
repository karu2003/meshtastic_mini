#!/bin/sh
# Init submodules (required before ARM build).
set -e
cd "${0%/*}/.."
git submodule update --init --recursive
echo "Submodules ready: third_party/STM32CubeWL, third_party/nanopb, third_party/meshtastic_protobufs"
