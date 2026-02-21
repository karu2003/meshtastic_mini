# Radio exchange between two devices

## CPU ↔ radio interface (STM32WLE5)

On STM32WLE5 the LoRa radio is **integrated in the chip** (SX1262-compatible block). The link between the CPU and the radio is **internal SUBGHZSPI**: an on-chip SPI controller (no external SPI pins). All commands and payloads are sent via HAL (`HAL_SUBGHZ_ExecSetCmd`, `WriteBuffer`, `ReadBuffer`). NSS is controlled by the PWR unit; RX/TX completion is signalled by **SUBGHZ_Radio_IRQn**.

Init sequence: SUBGHZSPI clock (APB3), radio reset pulse (RCC RF reset, 5 ms delay), SPI baudrate prescaler 2 (f/2), NSS/busy via HAL. RF switch aligned with radio_pair: PA4/PA5 — TX = PA4=0 PA5=1, RX = PA4=1 PA5=0; DIO2 used as RF switch (0x01).

## Meshtastic compatibility

- **Packet header:** 16-byte Meshtastic header (to_id, from_id, packet_id, flags, channel, next_hop, relay) — same layout as Meshtastic.
- **LoRa physical layer:** Preamble 16 symbols, LoRa sync word **0x2B** (Meshtastic network), SF/BW/CR and region presets as in Meshtastic docs. Same frequency and modem preset on both devices for interoperability.

---

The main serial port is **USART1 (PB6 TX, PB7 RX), 115200 8N1**. All traffic goes over it: commands, sending text over the air, and printing received LoRa packets.

## Exchange flow

```
  [PC]                     [Device A]                     [Device B]                     [PC]
   |       Serial               |         LoRa (radio)           |       Serial               |
   |  "0: hello" + Enter  -->   |  packet (broadcast, "hello") -->|  receives                 |
   |                            |                                |  RX: hello                |  <--  "RX: hello"
   |                            |                                |  sends "pong" -->         |
   |                            |  <--  receives "pong"          |  (to_id = A)              |
   |  "RX: pong"            <-- |  RX: pong                      |                          |
```

1. **User** types `0: hello` in the monitor → the line is sent to device A over serial.
2. **Device A** builds a LoRa packet: header (from_id=A, to_id=broadcast, packet_id, …) + payload `"hello"`, calls `lora_tx()`.
3. **Device B** gets the packet in `lora_rx_poll()`, checks `to_id == broadcast || to_id == node_id`, prints `RX: hello` on serial, then sends a reply packet with payload `"pong"` and `to_id = from_id` (i.e. to A).
4. **Device A** receives the "pong" packet, prints `RX: pong` on serial. Incoming "pong" is not replied to over the air (to avoid a loop).

Result: **A→B** and **B→A** exchange over the radio, visible on the PC via the main serial.

## Automatic link check

Run the script to verify the radio link in both directions (exit code 0 = pass, 1 = fail):

```bash
pip install pyserial
python3 scripts/check_radio_link.py
# or: python3 scripts/check_radio_link.py /dev/ttyUSB0 /dev/ttyUSB1
# optional: --timeout 8  (seconds to wait),  -q  (only print PASS/FAIL)
```

The script sets N1/N2 on the two ports, sends "hello" from device 0 and checks for "RX: hello" on device 1 and "RX: pong" on device 0, then sends "test" from device 1 and checks the reverse.

## Manual verification steps

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Two devices on /dev/ttyUSB0 and /dev/ttyUSB1; run `python3 scripts/dual_serial_monitor.py` | On both ports: `Meshtastic_mini started`, `mesh init done, loop` |
| 2 | Type `0: N1` and `1: N2` | On port 0 and 1 respectively: `node_id set` |
| 3 | Type `0: hello` | On port 1: `RX: hello`; then on port 0: `RX: pong` |
| 4 | Type `1: test` | On port 0: `RX: test`; then on port 1: `RX: pong` |

If step 3 or 4 fails, there is no LoRa RX/TX (check SubGHz HAL: frequency, RX mode, and that `rx_buf` / `rx_ready` in `radio_stm32wl.c` are set on receive).

## Over-the-air packet format

- **16 bytes** — Meshtastic header (to_id, from_id, packet_id, flags, channel, next_hop, relay).
- **Then** — payload (text, no null terminator; length = string length).

Serial commands:

- **N1** / **N2** — set this device’s `node_id` to 1 or 2 (use different ids for the two devices).
- Any other line — send it over LoRa as payload (broadcast).
