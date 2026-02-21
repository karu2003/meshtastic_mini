# Meshtastic-mini — Wio-E5-LE Mini (STM32WLE5JC)

**Meshtastic-compatible** protocol implementation for Seeed Studio Wio-E5-LE Mini: LoRa mesh only, config via Serial, no BLE/GPS/display.

## Platform

- **MCU:** STM32WLE5JC (Cortex-M4 + integrated sub-GHz LoRa, Semtech-compatible)
- **Radio:** integrated in the chip (not a separate SX1262); controlled via STM32WL HAL or RadioLib wrapper for STM32WL
- **Interfaces:** UART (Serial) for config and application; no BLE/GPS/display

## Scope

### 1. Physical layer (LoRa)

- **Meshtastic parameters:** SF, BW, CR, frequency — as in [LoRa Configuration](https://meshtastic.org/docs/configuration/radio/lora) and [LoRa Design Guide](https://meshtastic.org/docs/development/reference/lora-design/).
- **Regions/frequencies:** EU_868, US_915, EU_433, LORA_24, etc. — one region per build or selectable via Serial.
- **Modem presets:** LONG_FAST (default), SHORT_FAST, MEDIUM_SLOW, etc. — define SF/BW/CR combination.
- **Implementation:** either **STM32WL HAL** (SubGHz) or **RadioLib** with STM32WL board (ModuleSTM32WL). Both are viable.

### 2. Packet format (Protobuf + nanopb)

- **Over-the-air:** LoRa payload carries **MeshPacket** (or a simplified subset). Some fields are in the **16-byte LoRa header** (see below).
- **Serial (app ↔ device):** stream of **ToRadio** / **FromRadio** with **4-byte framing**: `0x94`, `0xC3`, `length_MSB`, `length_LSB` (length ≤ 512).
- **Proto repo:** [meshtastic/protobufs](https://github.com/meshtastic/protobufs). For embedded use **nanopb** (protobufs include `nanopb.proto` and options).
- **Minimum for compatibility:** MeshPacket, Data, User, portnums, channel, config (only needed fields); AdminMessage for get/set config over Serial.

### 3. 16-byte LoRa header (Meshtastic)

Enables relaying without decoding protobuf and saves airtime:

| Offset | Size | Field       | Description                                    |
|--------|------|-------------|------------------------------------------------|
| 0x00   | 4    | To (dest)   | Destination NodeID (0xFFFFFFFF = broadcast)    |
| 0x04   | 4    | From        | Sender NodeID                                 |
| 0x08   | 4    | Packet ID   | Unique packet ID from sender                  |
| 0x0C   | 1    | Flags       | HopLimit, WantAck, ViaMQTT, HopStart          |
| 0x0D   | 1    | Channel     | Channel index/hash (for decryption)            |
| 0x0E   | 1    | Next hop    | For relaying                                  |
| 0x0F   | 1    | Relay       | Relaying node                                 |

Payload follows: when encryption is on — encrypted block; otherwise protobuf (e.g. Data with portnum + payload).

### 4. Flood routing (managed flood)

- On receive: if **hop_limit > 0** and packet **not seen** (by Packet ID + From), decrement hop_limit and **rebroadcast**.
- Optional: short random delay before relay and cancel relay if the same packet is heard from another node in that time (deduplication).
- Need: store recent (From, Packet ID), hop limit (e.g. 3), timeout to “forget” IDs.

### 5. AES-128 encryption

- **STM32WLE5** has hardware **AES** (128/256); suitable for AES-128.
- Meshtastic: channel **PSK** is 0 / 16 / 32 bytes. 16 bytes → AES-128. Packet **header is not encrypted**; only payload is encrypted.
- Mode: as in Meshtastic (typically AES with shared channel key; exact mode/IV — see reference firmware or docs).

### 6. Config via Serial (no BLE/GPS/display)

- **Transport:** single UART (e.g. USB-CDC or LPUART).
- **Framing:** as in Meshtastic Client API — each packet: 4 bytes `0x94 0xC3 len_MSB len_LSB` + body (ToRadio from client, FromRadio from device).
- **Flow:** app (Python, CLI, custom) connects over Serial, sends **ToRadio** (e.g. `packet` with MeshPacket, `want_config_id`, admin get/set config). Device replies with **FromRadio** (my_info, node_info, config, packet, end_config, etc.).
- **Config storage:** in flash (or EEPROM) — region, modem preset, channels, keys, short_name/long_name, node number, etc.

### 7. Out of scope

- BLE, GPS, display — not used.
- Config **only** via Serial (and saved settings in flash when needed).

---

## Project structure

```
Meshtastic_mini/
├── README.md
├── CMakeLists.txt            # Single build system — CMake
├── .gitmodules                # Submodules: STM32CubeWL, nanopb, meshtastic_protobufs
├── config/
│   └── stm32wlxx_hal_conf.h  # HAL config (from CubeWL template)
├── cmake/
│   ├── arm-none-eabi.cmake   # ARM toolchain
│   ├── STM32CubeWL.cmake     # Paths to HAL/CMSIS/startup/linker from submodule
│   └── NanopbRuntime.cmake   # nanopb runtime from submodule
├── scripts/
│   └── init_submodules.sh   # git submodule update --init --recursive
├── third_party/              # Submodules (do not edit)
│   ├── STM32CubeWL/
│   ├── nanopb/
│   └── meshtastic_protobufs/
├── docs/
│   ├── SERIAL_API.md
│   └── CRYPTO.md
└── firmware/
    ├── Core/
    ├── Radio/
    ├── Mesh/
    ├── Serial/
    ├── Crypto/
    ├── Config/
    └── Protobuf/
```

---

## Links

- [Meshtastic LoRa Configuration](https://meshtastic.org/docs/configuration/radio/lora)
- [Meshtastic Client API (Serial/TCP/BLE)](https://meshtastic.org/docs/development/device/client-api/)
- [Meshtastic Protobufs](https://github.com/meshtastic/protobufs)
- [Why Meshtastic Uses Managed Flood Routing](https://meshtastic.org/blog/why-meshtastic-uses-managed-flood-routing/)
- [Meshtastic Encryption](https://meshtastic.org/docs/overview/encryption/)
- [STM32WLE5 Reference](https://www.st.com/resource/en/reference_manual/rm0462-stm32wl-series-advanced-armbased-32bit-mcus-with-subghz-radio-stmicroelectronics.pdf) — SubGHz and AES
- [Wio-E5-LE Mini](https://www.seeedstudio.com/Wio-E5-LE-mini-Dev-Board-STM32WLE5JC-p-5764.html)

---

## Build (CMake only)

All dependencies are **git submodules**. Build is CMake-only.

### Submodules

Before first ARM build, init submodules:

```bash
git submodule update --init --recursive
# or
./scripts/init_submodules.sh
```

Submodules:
- **third_party/STM32CubeWL** — HAL, CMSIS, SubGHz, startup and linker script for STM32WL.
- **third_party/nanopb** — Protobuf runtime (pb_encode/pb_decode).
- **third_party/meshtastic_protobufs** — Meshtastic .proto (for codegen and reference).

### build.sh script

From project root:

```bash
./build.sh build    # ARM build (default; requires submodules)
./build.sh flash    # Flash meshtastic_mini.elf (openocd or st-flash)
./build.sh clean    # Remove build and build-arm
BUILD_HOST=1 ./build.sh build   # Host build (no submodules)
```

### Host build (compile check)

No submodules; firmware only (radio and UART are stubs):

```bash
mkdir build && cd build
cmake ..
cmake --build .
# or: BUILD_HOST=1 ./build.sh build
```

Produces executable `meshtastic_mini`.

### STM32WLE5 (ARM) build

1. Install [arm-none-eabi-gcc](https://developer.arm.com/downloads/tools/gnu-rm).
2. Init submodules (see above).
3. Build with toolchain and ARM flag:

```bash
mkdir build-arm && cd build-arm
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake -DBUILD_FIRMWARE_ARM=ON
cmake --build .
```

Result: `meshtastic_mini.elf`. Linker script and startup come from STM32CubeWL submodule (override with `-DSTM32_LINKER_SCRIPT=...` if needed).

CMake options:
- `BUILD_FIRMWARE_ARM=ON` — ARM build (required for .elf).
- `USE_STM32WL_RADIO=ON` — Enable SubGHz driver from CubeWL (default ON for ARM).
- `USE_NANOPB=ON` — Use nanopb runtime from submodule (default ON for ARM).
- `BUILD_AS_LIBRARY=ON` — Build static library only (no main, no linker).

### Radio layer

- **Host:** stub in `radio_phy.c`.
- **ARM with CubeWL submodule:** `radio_stm32wl.c` and HAL SubGHz are built in; see [AN5406](https://www.st.com/resource/en/application_note/an5406-how-to-build-a-lora-application-with-stm32cubewl-stmicroelectronics.pdf) for frequency/LoRa setup.

---

## How to verify it works

### 1. Host build (no hardware)

Confirms that the firmware compiles and links:

```bash
./build.sh clean
BUILD_HOST=1 ./build.sh build
./build/meshtastic_mini   # runs forever; radio/UART are stubs — no real I/O
```

Exit with Ctrl+C. If the binary runs without crashing, the host path is OK.

### 2. ARM build

Confirms that the STM32 target builds (submodules required):

```bash
./build.sh clean
git submodule update --init --recursive
./build.sh build
```

Check that `build-arm/meshtastic_mini.elf` exists and has a reasonable size (tens of KB). You can inspect it:

```bash
arm-none-eabi-size build-arm/meshtastic_mini.elf
```

### 3. Flash to the board (Wio-E5-LE Mini)

With OpenOCD or st-link and the board connected via ST-Link (or built-in debugger):

```bash
./build.sh flash
```

After flashing, the MCU resets and runs `main()` → `mesh_mini_init()` → `mesh_mini_loop()`. The radio and Serial are driven by HAL (SubGHz and UART); without a full Meshtastic stack you will not see app-level traffic yet, but the device should start and sit in the loop.

### 4. Serial (when UART is wired)

- Connect the board’s UART (e.g. USB-CDC if exposed) to the PC.
- Open a terminal at the same baud rate as in your `uart_tx`/UART init (e.g. 115200).
- The firmware uses the **Meshtastic Serial framing** (0x94 0xC3 + length + ToRadio/FromRadio protobuf). Raw text will not be parsed.
- To really “see it work” you need a client that sends ToRadio and decodes FromRadio (e.g. [Meshtastic Python](https://github.com/meshtastic/Meshtastic-python) with a serial port, or a small test script that builds a ToRadio packet and prints FromRadio).

### 5. LoRa (two boards)

With two Wio-E5 boards running this firmware (same region/preset and channel key):

- Implement or complete in `radio_stm32wl.c` the actual SubGHz TX/RX (SetRFFrequency, SetModemParams, etc. per AN5406).
- Send a MeshPacket over Serial on one node; the other should receive it on LoRa and, if Serial is connected, you should see a FromRadio on the second node’s serial port.

Until the HAL calls in `radio_stm32wl.c` are fully wired and the ToRadio/FromRadio path is implemented (nanopb encode/decode), “working” means: **builds, flashes, runs main loop**; full Meshtastic compatibility requires finishing the radio init, Serial↔ToRadio/FromRadio, and optional AES.

---

Next steps: LoRa TX/RX → Serial framing and one packet type → flood and AES.
