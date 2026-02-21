/**
 * AES-128-CTR for Meshtastic channel payload (STM32WLE5 hardware AES).
 * Nonce = packetId(8 LE) + fromNode(4 LE) + blockCounter(4).
 * Header is NOT encrypted; only payload after 16-byte header.
 */

#ifndef AES_MESHTASTIC_H
#define AES_MESHTASTIC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MESH_AES_KEY_LEN 16

void aes_set_channel_key(const uint8_t *key);

/**
 * AES-128-CTR encrypt/decrypt in place (same operation for CTR mode).
 * Meshtastic nonce: packetId(8 LE) + fromNode(4 LE) + counter(4).
 */
void aes_ctr_crypt(uint8_t *payload, uint16_t len,
                   uint32_t packet_id, uint32_t from_node);

#ifdef __cplusplus
}
#endif

#endif /* AES_MESHTASTIC_H */
