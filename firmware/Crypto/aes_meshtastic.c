/**
 * Stub: AES-128 via STM32WLE5 HAL CRYP.
 * Mode and IV per Meshtastic spec (see docs/CRYPTO.md).
 */

#include "aes_meshtastic.h"
#include <string.h>
#include <stdbool.h>

static uint8_t channel_key[MESH_AES_KEY_LEN];

void aes_set_channel_key(const uint8_t *key) {
    if (key)
        memcpy(channel_key, key, MESH_AES_KEY_LEN);
}

bool aes_decrypt_payload(uint8_t *payload, uint16_t len) {
    (void)payload;
    (void)len;
    /* TODO: HAL_CRYP_AESECB_Decrypt or chosen mode (CBC + IV from packet) */
    return true;
}

bool aes_encrypt_payload(uint8_t *payload, uint16_t len) {
    (void)payload;
    (void)len;
    /* TODO: HAL_CRYP_AESECB_Encrypt or chosen mode */
    return true;
}
