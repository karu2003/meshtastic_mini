# AES-128 encryption (Meshtastic-compatible)

- **Channel key (PSK):** 0 (off), 16 (AES-128) or 32 (AES-256) bytes. For this mini implementation **16 bytes (AES-128)** is enough.
- **STM32WLE5:** has hardware AES (128/256). Use HAL (e.g. `HAL_CRYP_*`) or LL drivers.
- **What is encrypted:** only the packet **payload** (after the 16-byte LoRa header). The header is sent in the clear so relays can forward without decrypting.
- **Mode and IV:** exact algorithm (ECB/CBC, where to get IV — nonce from header, packet counter, etc.) should be taken from official Meshtastic firmware or [Encryption](https://meshtastic.org/docs/overview/encryption/) docs. Once defined, implement in `Crypto/aes_meshtastic.c` on top of HAL.

In Meshtastic the primary channel often uses key `0x01` (base64 "AQ=="), i.e. one byte; for AES-128 the key is padded to 16 bytes (as in reference firmware).
