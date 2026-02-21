/**
 * Simple deduplication: ring buffer of recent (from_id, packet_id).
 * In practice better to limit by time or count.
 */

#include "flood_router.h"
#include <string.h>

#define SEEN_SIZE 32

typedef struct { uint32_t from; uint32_t id; } seen_t;
static seen_t seen_buf[SEEN_SIZE];
static uint8_t seen_head;

void flood_seen(uint32_t from_id, uint32_t packet_id) {
    seen_buf[seen_head].from = from_id;
    seen_buf[seen_head].id   = packet_id;
    seen_head = (seen_head + 1) % SEEN_SIZE;
}

bool flood_was_seen(uint32_t from_id, uint32_t packet_id) {
    for (int i = 0; i < SEEN_SIZE; i++) {
        if (seen_buf[i].from == from_id && seen_buf[i].id == packet_id)
            return true;
    }
    return false;
}

bool flood_should_forward(const uint8_t *lora_packet, uint16_t len) {
    if (!lora_packet || len < MESH_HEADER_SIZE) return false;
    mesh_lora_header_t h;
    mesh_header_from_buf(&h, lora_packet);
    if (flood_was_seen(h.from_id, h.packet_id)) return false;
    uint8_t hop = mesh_hop_limit(h.flags);
    if (hop == 0) return false;
    return true;
}

void flood_prepare_forward(uint8_t *lora_packet, uint16_t len, uint8_t my_node_id) {
    if (!lora_packet || len < MESH_HEADER_SIZE) return;
    mesh_lora_header_t h;
    mesh_header_from_buf(&h, lora_packet);
    uint8_t hop = mesh_hop_limit(h.flags);
    if (hop > 0) hop--;
    h.flags = (h.flags & ~MESH_HOP_LIMIT_MASK) | (hop & MESH_HOP_LIMIT_MASK);
    h.relay = my_node_id; /* or next_hop depending on semantics */
    mesh_header_to_buf(&h, lora_packet);
}
