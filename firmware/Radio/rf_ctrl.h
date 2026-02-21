/**
 * RF front-end control: PA4=RF_CTRL1, PA5=RF_CTRL2.
 * Switch antenna/path for RX vs TX. Adjust levels in rf_ctrl.c per board schematic.
 */

#ifndef FIRMWARE_RADIO_RF_CTRL_H
#define FIRMWARE_RADIO_RF_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

void rf_ctrl_init(void);
void rf_ctrl_set_off(void);   /* PA4=0 PA5=0 (as radio_pair standby) */
void rf_ctrl_set_rx(void);
void rf_ctrl_set_tx(void);

#ifdef __cplusplus
}
#endif

#endif /* FIRMWARE_RADIO_RF_CTRL_H */
