# Serial API (config via application)

Device has no BLE: config and packet exchange only over UART (USB CDC or LPUART).

## Framing (Meshtastic-style)

Each packet is preceded by **4 bytes**:

| Byte | Value     | Description        |
|------|-----------|--------------------|
| 0    | 0x94      | START1             |
| 1    | 0xC3      | START2             |
| 2    | len >> 8  | Payload length MSB |
| 3    | len & 0xFF| Payload length LSB |

- **Payload:** up to **512** bytes (protobuf ToRadio or FromRadio).
- If length > 512 → treat as corrupted, resync on 0x94.
- Stream is assumed reliable (no CRC in this layer); on byte loss the receiver resyncs on START1.

## Direction

- **To device:** stream of **ToRadio** packets (commands, MeshPacket to send OTA, config request).
- **From device:** stream of **FromRadio** packets (config replies, received MeshPacket, my_info, node_info, logs, etc.).

## ToRadio (client → device)

Main oneof variants:

- `packet` — MeshPacket to send on the mesh.
- `want_config_id` — request full config; device sends FromRadio stream and ends with `end_config` with this id.
- AdminMessage (get/set config, get/set channel, set_owner, etc.) — for config over Serial.

## FromRadio (device → client)

Main variants:

- `packet` — received MeshPacket (for the app).
- `my_info` — local node info (MyNodeInfo).
- `node_info` — node info from NodeDB.
- `config` — current config (in reply to get).
- `log_record` — log (optional).
- `end_config` — end of config dump after `want_config_id`.

## Typical flow on connect

1. Client connects over Serial.
2. Client sends **ToRadio** with `want_config_id` (e.g. 1).
3. Device sends a series of **FromRadio**: my_info, node_info for all nodes, config, channels, etc., then **FromRadio.end_config** with the same id.
4. Client can then send ToRadio.packet (text/data) and receive FromRadio.packet for incoming messages.

## Config without BLE

- All config (frequency, SF/BW/CR, channels, keys, names) via **AdminMessage** in ToRadio.
- Device stores config in flash and reboots if needed.
- See [Client API](https://meshtastic.org/docs/development/device/client-api/) and [meshtastic/protobufs](https://github.com/meshtastic/protobufs) (admin.proto, config.proto).
