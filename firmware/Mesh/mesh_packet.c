#include "mesh_packet.h"

void mesh_header_from_buf(mesh_lora_header_t *h, const uint8_t *buf) {
    if (!h || !buf) return;
    h->to_id     = (uint32_t)buf[0] | ((uint32_t)buf[1]<<8) | ((uint32_t)buf[2]<<16) | ((uint32_t)buf[3]<<24);
    h->from_id   = (uint32_t)buf[4] | ((uint32_t)buf[5]<<8) | ((uint32_t)buf[6]<<16) | ((uint32_t)buf[7]<<24);
    h->packet_id = (uint32_t)buf[8] | ((uint32_t)buf[9]<<8) | ((uint32_t)buf[10]<<16) | ((uint32_t)buf[11]<<24);
    h->flags    = buf[12];
    h->channel  = buf[13];
    h->next_hop = buf[14];
    h->relay    = buf[15];
}

void mesh_header_to_buf(const mesh_lora_header_t *h, uint8_t *buf) {
    if (!h || !buf) return;
    buf[0]= (h->to_id)     & 0xFF; buf[1]= (h->to_id>>8)&0xFF; buf[2]= (h->to_id>>16)&0xFF; buf[3]= (h->to_id>>24)&0xFF;
    buf[4]= (h->from_id)   & 0xFF; buf[5]= (h->from_id>>8)&0xFF; buf[6]= (h->from_id>>16)&0xFF; buf[7]= (h->from_id>>24)&0xFF;
    buf[8]= (h->packet_id) & 0xFF; buf[9]= (h->packet_id>>8)&0xFF; buf[10]=(h->packet_id>>16)&0xFF; buf[11]=(h->packet_id>>24)&0xFF;
    buf[12]= h->flags; buf[13]= h->channel; buf[14]= h->next_hop; buf[15]= h->relay;
}
