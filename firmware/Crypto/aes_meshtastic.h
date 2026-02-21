/**
 * AES-128 for channel payload (STM32WLE5 hardware AES).
 * Packet header is not encrypted; only payload after 16-byte header is encrypted.
 */

#ifndef AES_MESHTASTIC_H
#define AES_MESHTASTIC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MESH_AES_KEY_LEN 16

/**
 * Set channel key (PSK, 16 bytes for AES-128).
 */
void aes_set_channel_key(const uint8_t *key);

/**
 * Decrypt payload in place. len must be multiple of 16 (AES block).
 * IV/mode per Meshtastic spec (see firmware or docs).
 */
bool aes_decrypt_payload(uint8_t *payload, uint16_t len);

/**
 * Encrypt payload in place.
 */
bool aes_encrypt_payload(uint8_t *payload, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* AES_MESHTASTIC_H */
