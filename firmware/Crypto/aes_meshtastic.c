/**
 * AES-128-CTR for Meshtastic payloads via STM32WLE5 hardware AES.
 *
 * CTR mode: AES-ECB encrypts the nonce to produce keystream, XOR with data.
 * Nonce layout (16 bytes, matches Meshtastic CryptoEngine::initNonce):
 *   [0..3]   packetId    (LE 32-bit, low half of 64-bit packetId)
 *   [4..7]   0           (high half, always 0 for 32-bit IDs)
 *   [8..11]  fromNode    (LE 32-bit)
 *   [12..14] 0
 *   [15]     block counter (0, 1, 2, ...)
 */
#include "aes_meshtastic.h"
#include <string.h>

#if defined(USE_HAL_DRIVER)
#include "stm32wlxx_hal.h"
#endif

static uint8_t channel_key[MESH_AES_KEY_LEN];

#if defined(USE_HAL_DRIVER) && defined(HAL_CRYP_MODULE_ENABLED)
#include "stm32wlxx_hal_cryp.h"
#include "stm32wlxx_hal_rcc.h"

static CRYP_HandleTypeDef hcryp;
static uint32_t hw_key[4];
static bool cryp_ready;

void HAL_CRYP_MspInit(CRYP_HandleTypeDef *h) {
    (void)h;
    __HAL_RCC_AES_CLK_ENABLE();
}

static bool ensure_cryp(void) {
    if (cryp_ready) return true;
    memset(&hcryp, 0, sizeof(hcryp));
    hcryp.Instance             = AES;
    hcryp.Init.DataType        = CRYP_DATATYPE_32B;
    hcryp.Init.KeySize         = CRYP_KEYSIZE_128B;
    hcryp.Init.pKey            = hw_key;
    hcryp.Init.pInitVect       = NULL;
    hcryp.Init.Algorithm       = CRYP_AES_ECB;
    hcryp.Init.DataWidthUnit   = CRYP_DATAWIDTHUNIT_WORD;
    hcryp.Init.HeaderWidthUnit = CRYP_HEADERWIDTHUNIT_WORD;
    hcryp.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ALWAYS;
    hcryp.Init.Header          = NULL;
    hcryp.Init.HeaderSize      = 0;
    hcryp.Init.B0              = NULL;
    if (HAL_CRYP_Init(&hcryp) != HAL_OK) return false;
    cryp_ready = true;
    return true;
}

static void bytes_to_be_words(const uint8_t b[16], uint32_t w[4]) {
    for (int i = 0; i < 4; i++)
        w[i] = ((uint32_t)b[i*4] << 24) | ((uint32_t)b[i*4+1] << 16) |
               ((uint32_t)b[i*4+2] << 8) | b[i*4+3];
}

static void be_words_to_bytes(const uint32_t w[4], uint8_t b[16]) {
    for (int i = 0; i < 4; i++) {
        b[i*4]   = (uint8_t)(w[i] >> 24);
        b[i*4+1] = (uint8_t)(w[i] >> 16);
        b[i*4+2] = (uint8_t)(w[i] >> 8);
        b[i*4+3] = (uint8_t)(w[i]);
    }
}

static bool aes_ecb_block(const uint8_t in[16], uint8_t out[16]) {
    if (!ensure_cryp()) return false;
    uint32_t in_w[4], out_w[4];
    bytes_to_be_words(in, in_w);
    if (HAL_CRYP_Encrypt(&hcryp, in_w, 4, out_w, 100) != HAL_OK)
        return false;
    be_words_to_bytes(out_w, out);
    return true;
}

void aes_ctr_crypt(uint8_t *payload, uint16_t len,
                   uint32_t packet_id, uint32_t from_node)
{
    if (len == 0) return;

    uint8_t nonce[16];
    memset(nonce, 0, 16);
    memcpy(nonce,     &packet_id,  4);
    memcpy(nonce + 8, &from_node,  4);

    uint8_t keystream[16];
    for (uint16_t off = 0; off < len; off += 16) {
        if (!aes_ecb_block(nonce, keystream)) return;
        nonce[15]++;
        uint16_t n = (len - off > 16) ? 16 : (len - off);
        for (uint16_t j = 0; j < n; j++)
            payload[off + j] ^= keystream[j];
    }
}

#else /* no HAL */

void aes_ctr_crypt(uint8_t *payload, uint16_t len,
                   uint32_t packet_id, uint32_t from_node)
{
    (void)payload; (void)len; (void)packet_id; (void)from_node;
}

#endif

void aes_set_channel_key(const uint8_t *key) {
    if (!key) return;
    memcpy(channel_key, key, MESH_AES_KEY_LEN);
#if defined(USE_HAL_DRIVER) && defined(HAL_CRYP_MODULE_ENABLED)
    bytes_to_be_words(channel_key, hw_key);
    cryp_ready = false;
#endif
}
