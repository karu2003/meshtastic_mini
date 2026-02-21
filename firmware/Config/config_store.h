/**
 * Config storage in flash: region, modem preset, channels (keys), node id, short/long name.
 * Load/save on boot and on AdminMessage (set_config etc.).
 */

#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t node_id;           /* our NodeID (lower 32 bits or random) */
    uint8_t  region;            /* lora_region_t */
    uint8_t  modem_preset;       /* lora_modem_preset_t */
    uint8_t  channel_psk[16];   /* default channel AES-128 key */
    char     short_name[4];     /* 2–3 chars + null */
    char     long_name[32];
} device_config_t;

bool config_load(device_config_t *cfg);
bool config_save(const device_config_t *cfg);

/* Default values (Meshtastic-compatible) */
void config_set_defaults(device_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_STORE_H */
