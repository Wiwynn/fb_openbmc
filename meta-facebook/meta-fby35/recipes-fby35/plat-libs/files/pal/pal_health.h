#ifndef __PAL_HEALTH_H__
#define __PAL_HEALTH_H__

int pal_set_sensor_health(uint8_t fru, uint8_t value);
int pal_get_fru_health(uint8_t fru, uint8_t *value);
int pal_bic_self_test(uint8_t fru);
int pal_is_bic_ready(uint8_t fru, uint8_t *status);

#endif

