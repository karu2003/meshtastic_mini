# Meshtastic-mini — Wio-E5-LE Mini (STM32WLE5JC)

**Meshtastic-compatible** LoRa mesh firmware for Seeed Studio Wio-E5-LE Mini. LoRa mesh only, config via Serial, no BLE/GPS/display.

## Platform

- **MCU:** STM32WLE5JC (Cortex-M4 + integrated sub-GHz radio)
- **Radio:** integrated SX1262-compatible, controlled via STM32WL HAL (SUBGHZSPI)
- **Interfaces:** UART (Serial) for config and application; no BLE/GPS/display
- **RF switch:** PA4 (RF_CTRL1), PA5 (RF_CTRL2). Default: RFO_HP (High Power PA)

## Build

### Prerequisites

- [arm-none-eabi-gcc](https://developer.arm.com/downloads/tools/gnu-rm)
- CMake 3.16+
- Git submodules initialized

### Quick start

```bash
git submodule update --init --recursive
./build.sh build    # → build/meshtastic_mini.elf
./build.sh flash    # Flash via OpenOCD / st-flash
```

### Manual build

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake
cmake --build .
```

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `WIO_E5_USE_LP` | OFF | Use RFO_LP PA instead of RFO_HP |
| `WIO_E5_NO_TCXO` | OFF | Disable TCXO (crystal-only boards) |
| `USE_STM32WL_RADIO` | ON | SubGHz driver |
| `USE_NANOPB` | ON | nanopb runtime |

## Radio link test

Two boards connected via USB — automatic bidirectional ping-pong:

```bash
pip install pyserial
python3 scripts/check_radio_link.py
```

Expected output:

```
[ttyUSB0] -> [ttyUSB1]: 'ping'
  OK: [ttyUSB1] received 'ping', [ttyUSB0] received 'pong'
[ttyUSB1] -> [ttyUSB0]: 'ping'
  OK: [ttyUSB0] received 'ping', [ttyUSB1] received 'pong'
PASS: radio link OK (both directions)
```

Options: `--timeout 15`, explicit ports: `python3 scripts/check_radio_link.py /dev/ttyUSB0 /dev/ttyUSB1`

## Serial interface

USART1: PB6 (TX), PB7 (RX), 115200 8N1.

On boot: `Meshtastic_mini started`, `mesh init done, loop`.

**Any text entered in the terminal is sent as LoRa payload.** For example, typing `hello` + Enter sends the bytes "hello" over LoRa to all nodes. This is not a command — it is data transmitted by radio.

Received packets appear as: `RX: <text>  RSSI: -XX dBm  SNR: X dB`. The receiver automatically replies with "pong".

Commands (reserved words):

| Command | Description |
|---------|-------------|
| `N1` … `N9` | Set node_id (e.g. N1 on first board, N2 on second) |
| `info` | Show frequency (MHz), SF, NodeId, last RSSI |
| `help` | List commands |

## SDR frequency

Default region EU868: **869.525 MHz**, BW 250 kHz, SF11.

## Protocol

### LoRa header (16 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 4 | To | Destination NodeID (0xFFFFFFFF = broadcast) |
| 0x04 | 4 | From | Sender NodeID |
| 0x08 | 4 | Packet ID | Unique packet ID |
| 0x0C | 1 | Flags | HopLimit, WantAck, ViaMQTT, HopStart |
| 0x0D | 1 | Channel | Channel index/hash |
| 0x0E | 1 | Next hop | For relaying |
| 0x0F | 1 | Relay | Relaying node |

### Flood routing

On receive: if hop_limit > 0 and packet not seen (by Packet ID + From), decrement hop_limit and rebroadcast.

### AES-128 encryption

STM32WLE5 hardware AES. Channel PSK 16 bytes → AES-128. Header is not encrypted; only payload.

## Project structure

```
Meshtastic_mini/
├── CMakeLists.txt
├── cmake/                  # Toolchain, HAL/CMSIS/nanopb cmake
├── scripts/                # check_radio_link.py, dual_serial_monitor.py
├── firmware/
│   ├── Core/               # main_loop, serial_io, led, system_clock
│   ├── Radio/              # radio_stm32wl, lora_meshtastic, radio_phy, rf_ctrl
│   ├── Mesh/               # mesh_packet, flood_router
│   ├── Serial/             # serial_framing
│   ├── Crypto/             # aes_meshtastic
│   └── Config/             # config_store
└── third_party/            # STM32CubeWL, nanopb, meshtastic_protobufs
```

## Links

- [Meshtastic LoRa Configuration](https://meshtastic.org/docs/configuration/radio/lora)
- [Meshtastic Protobufs](https://github.com/meshtastic/protobufs)
- [STM32WLE5 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0462-stm32wl-series-advanced-armbased-32bit-mcus-with-subghz-radio-stmicroelectronics.pdf)
- [Wio-E5-LE Mini](https://www.seeedstudio.com/Wio-E5-LE-mini-Dev-Board-STM32WLE5JC-p-5764.html)
- [Seeed LoRaWan-E5-Node](https://github.com/Seeed-Studio/LoRaWan-E5-Node)

## Removing Readout Protection

To allow flashing: set RDP level to AA, nBOOT0 = 0 via STM32CubeProgrammer.
