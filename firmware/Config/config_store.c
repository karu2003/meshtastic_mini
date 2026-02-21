#include "config_store.h"
#include <string.h>

void config_set_defaults(device_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->node_id = 0x12345678; /* replace with unique (MAC or random) */
    cfg->region = 0;           /* REGION_EU_868 */
    cfg->modem_preset = 4;     /* MODEM_LONG_FAST */
    cfg->channel_psk[0] = 0x01;
    /* short_name/long_name as desired */
}

bool config_load(device_config_t *cfg) {
    if (!cfg) return false;
    /* TODO: read from flash/EEPROM */
    config_set_defaults(cfg);
    return true;
}

bool config_save(const device_config_t *cfg) {
    (void)cfg;
    /* TODO: write to flash */
    return true;
}
