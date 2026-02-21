# Connecting Meshtastic_mini to Other Meshtastic Nodes

This document describes how to achieve interoperability between Meshtastic_mini (STM32WLE5) and other devices running Meshtastic: phones with the Meshtastic app, ESP32-based nodes, or any node using the same over-the-air (OTA) format.

---

## 1. Overview

Meshtastic_mini implements a **Meshtastic-compatible** radio link:

- **Physical layer:** LoRa with Meshtastic sync word (0x2B), preamble, and region/preset tables.
- **Packet structure:** 16-byte Meshtastic header (to_id, from_id, packet_id, flags, channel, next_hop, relay) + encrypted payload.
- **Payload:** AES-128-CTR encrypted; plaintext is a Protocol Buffers **Data** message (portnum + payload), matching Meshtastic text messages (portnum = 1).
- **Channel key:** Default is the standard Meshtastic **LongFast** primary channel PSK (AES-128).

If another node uses the **same region, modem preset, channel key, and frequency**, it can send and receive text with Meshtastic_mini.

---

## 2. Requirements for Interoperability

| Requirement | Meshtastic_mini default | Other node must match |
|-------------|-------------------------|------------------------|
| **Region** | EU 868 | Same region (EU 868, US 915, etc.) |
| **Frequency** | 869.525 MHz (EU 868 slot 0) | Same frequency for the channel |
| **Modem preset** | Long Fast | Long Fast (SF11, BW 250 kHz, CR 4/5) |
| **Channel key (PSK)** | LongFast default (see below) | Same 16-byte key on the same channel |
| **Sync word** | 0x2B (Meshtastic) | 0x2B |
| **Packet format** | 16-byte header + AES-CTR(protobuf Data) | Same |

---

## 3. Channel Key (Default LongFast PSK)

Meshtastic_mini uses the standard Meshtastic **LongFast** primary channel key by default. It is stored in firmware and loaded at boot (`config_store.c`):

```
Hex (16 bytes):
d4 f1 bb 3a 20 29 05 13 f0 08 8a a3 5a d3 4a 1c
```

To connect with standard Meshtastic devices:

1. On the **other node or Meshtastic app:** ensure the **primary channel** is set to **Long Fast** and uses the **default channel key** (or the same key as above if you changed it in firmware).
2. Do **not** use a custom channel key on the other device unless you also set that same key in Meshtastic_mini (e.g. via config storage or recompile).

---

## 4. Supported Regions and Modem Presets

Meshtastic_mini supports the same region and preset concepts as Meshtastic. Default at boot is **EU 868** and **Long Fast**.

| Region (index) | Default frequency (Hz) |
|----------------|-------------------------|
| EU 868 | 869 525 000 |
| US 915 | 906 875 000 |
| EU 433 | 433 875 000 |
| LoRa 2.4G | 2 403 000 000 |

| Modem preset | SF | BW | CR |
|--------------|----|----|-----|
| Short Fast | 7 | 250 kHz | 4/5 |
| Short Slow | 8 | 250 kHz | 4/5 |
| Medium Fast | 9 | 250 kHz | 4/5 |
| Medium Slow | 10 | 250 kHz | 4/5 |
| **Long Fast (default)** | **11** | **250 kHz** | **4/5** |
| Long Slow | 12 | 125 kHz | 4/8 |
| Very Long Slow | 12 | 125 kHz | 4/8 |

Other nodes must use the **same region and same preset** (and thus same frequency and LoRa parameters) to communicate.

---

## 5. Over-the-Air Packet Format

Each LoRa packet has:

1. **Header (16 bytes, unencrypted)**  
   - to_id, from_id, packet_id (32-bit LE)  
   - flags (hop limit in low bits, etc.), channel, next_hop, relay  

2. **Payload (encrypted)**  
   - AES-128-CTR with nonce: `packet_id (8 bytes LE) + from_id (4 bytes LE) + block_counter (4 bytes)`.  
   - Plaintext: Protobuf **Data** message — field 1: `portnum` (varint), field 2: `payload` (length-delimited bytes).  
   - Text messages use **portnum = 1** (TEXT_MESSAGE_APP).

See [CRYPTO.md](CRYPTO.md) for encryption details and [RADIO_EXCHANGE.md](RADIO_EXCHANGE.md) for the radio and serial flow.

---

## 6. Step-by-Step: Connecting to Another Meshtastic Node

1. **Flash Meshtastic_mini** on your STM32WLE5 board (Wio-E5 / Wio-E5-LE). No config change needed for default Long Fast + EU 868.

2. **Set node ID** over serial (e.g. `N1` or `N2`) so this device has a distinct ID. Other nodes will see packets from this ID.

3. **On the other device (phone app or another node):**  
   - Set **region** to **EU 868** (or the region you use).  
   - Set the **primary channel** to **Long Fast**.  
   - Use the **default Long Fast channel key** (or the same 16-byte key as in Meshtastic_mini).

4. **Antenna and placement:** Ensure both devices have antennas and are within LoRa range (hundreds of metres to a few km depending on environment).

5. **Test:**  
   - From Meshtastic_mini: type a line over serial (e.g. `hello`); it is sent as a broadcast. The other node should show the message.  
   - From the other node: send a text message; Meshtastic_mini should print `RX: <text>` on serial. Meshtastic_mini auto-replies with `pong` to any non-pong message.

6. **Optional:** Run the link test between two Meshtastic_mini devices first:
   ```bash
   python3 scripts/check_radio_link.py
   ```
   Then add a third device (phone or ESP32 node) with the same channel settings.

---

## 7. Limitations vs. Full Meshtastic

- **Node ID:** Meshtastic_mini uses a small numeric ID (e.g. 1–9 via serial). Full Meshtastic uses longer node numbers; the other side may show this node as an unknown/short ID.
- **Channels:** Only one channel key is used (primary Long Fast). No multi-channel or channel list management.
- **Services:** No Admin, Owner, or node list protocols; only **Data** messages (portnum 1 text) are implemented. Other portnums are not decoded.
- **Relay:** Flood forwarding with hop limit and deduplication is supported; packets can be relayed by Meshtastic_mini or by other nodes if they support the same header format.

---

## 8. Summary Checklist

- [ ] Same **region** (e.g. EU 868) on both sides  
- [ ] Same **modem preset** (Long Fast recommended)  
- [ ] Same **channel key** (default LongFast PSK or identical custom key)  
- [ ] Sync word **0x2B** (Meshtastic) — already set in firmware  
- [ ] Antennas connected and devices in range  
- [ ] Node IDs set on Meshtastic_mini (N1, N2, …) so traffic is identifiable  

With these aligned, Meshtastic_mini and other Meshtastic nodes can exchange text messages over the same channel.
