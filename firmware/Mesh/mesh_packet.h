/**
 * Mesh packet: 16-byte Meshtastic header + payload.
 * Broadcast = 0xFFFFFFFF, NodeID = 32 bit (typically lower 4 bytes of MAC or random).
 */

#ifndef MESH_PACKET_H
#define MESH_PACKET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MESH_HEADER_SIZE  16
#define MESH_BROADCAST_ID 0xFFFFFFFFU

typedef struct __attribute__((packed)) {
    uint32_t to_id;      /* 0x00: destination, BROADCAST = 0xFFFFFFFF */
    uint32_t from_id;    /* 0x04: sender */
    uint32_t packet_id;  /* 0x08: unique ID from sender */
    uint8_t  flags;     /* 0x0C: HopLimit (low bits), WantAck, ViaMQTT, HopStart */
    uint8_t  channel;   /* 0x0D: channel index/hash */
    uint8_t  next_hop;  /* 0x0E */
    uint8_t  relay;     /* 0x0F */
} mesh_lora_header_t;

/* Parse/build header from/to LoRa buffer */
void mesh_header_from_buf(mesh_lora_header_t *h, const uint8_t *buf);
void mesh_header_to_buf(const mesh_lora_header_t *h, uint8_t *buf);

/* Hop limit from flags (typically low 4 bits) */
#define MESH_HOP_LIMIT_MASK  0x0F
static inline uint8_t mesh_hop_limit(uint8_t flags) {
    return flags & MESH_HOP_LIMIT_MASK;
}

#ifdef __cplusplus
}
#endif

#endif /* MESH_PACKET_H */
