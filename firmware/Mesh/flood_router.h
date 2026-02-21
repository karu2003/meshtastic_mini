/**
 * Flood routing: relay by hop_limit and deduplication by (from_id, packet_id).
 */

#ifndef FLOOD_ROUTER_H
#define FLOOD_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "mesh_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Call on raw LoRa packet receive (header + payload).
 * - If packet already seen — returns false (do not relay).
 * - If hop_limit == 0 — returns false.
 * - Otherwise registers packet, decrements hop_limit and returns true (may relay).
 */
bool flood_should_forward(const uint8_t *lora_packet, uint16_t len);

/* Remember (from_id, packet_id) for deduplication. Call on successful receive. */
void flood_seen(uint32_t from_id, uint32_t packet_id);

/* Check if this packet was already seen */
bool flood_was_seen(uint32_t from_id, uint32_t packet_id);

/* Prepare packet for relay: decrement hop_limit in buffer, update relay. */
void flood_prepare_forward(uint8_t *lora_packet, uint16_t len, uint8_t my_node_id);

#ifdef __cplusplus
}
#endif

#endif /* FLOOD_ROUTER_H */
