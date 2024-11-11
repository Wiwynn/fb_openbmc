#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>
#include <openbmc/kv.h>
#include "pal.h"
#include "pal_health.h"

int
pal_get_fru_health(uint8_t fru, uint8_t *value) {
  char val[MAX_VALUE_LEN] = {0};
  char key[MAX_KEY_LEN] = {0};
  int ret = ERR_NOT_READY;

  switch(fru) {
    case FRU_SLOT1:
    case FRU_SLOT2:
    case FRU_SLOT3:
    case FRU_SLOT4:
      /*it's supported*/
      break;
    default:
      return ERR_NOT_READY;
  }

  // get sel val
  snprintf(key, MAX_KEY_LEN, "slot%d_sel_error", fru);
  ret = pal_get_key_value(key, val);
  if ( ret < 0 ) return ret;

  // store it
  *value = atoi(val);

  // reset
  memset(val, 0, MAX_VALUE_LEN);

  // get snr val
  snprintf(key, MAX_KEY_LEN, "slot%d_sensor_health", fru);
  ret = pal_get_key_value(key, val);
  if ( ret < 0 ) return ret;

  // is fru FRU_BAD?
  *value = *value & atoi(val);
  return PAL_EOK;
}

int
pal_set_sensor_health(uint8_t fru, uint8_t value) {
  char val[MAX_VALUE_LEN] = {0};
  char key[MAX_KEY_LEN] = {0};

  switch(fru) {
    case FRU_SLOT1:
    case FRU_SLOT2:
    case FRU_SLOT3:
    case FRU_SLOT4:
      /*it's supported*/
      break;
    default:
      return ERR_NOT_READY;
  }

  snprintf(key, MAX_KEY_LEN, "slot%d_sensor_health", fru);
  snprintf(val, MAX_VALUE_LEN, (value > 0) ? "1": "0");

  return pal_set_key_value(key, val);
}

int
pal_bic_self_test(uint8_t fru) {
  uint8_t result[SIZE_SELF_TEST_RESULT] = {0};
  uint8_t root = fru;
  int ret;
  uint8_t status;

  ret = pal_is_fru_prsnt(fru, &status);

  // The FRU is not present or we can not confirm it, skip running the self-test against its bic.
  if (ret < 0 || status == 0) {
    return 0;
  }

  // Check the health of sb bic and it relies on NONE_INTF
  pal_get_root_fru(fru, &root);
  return bic_get_self_test_result(root, (uint8_t *)&result, NONE_INTF);
}

int
pal_is_bic_ready(uint8_t fru, uint8_t *status) {
  return fby35_common_is_bic_ready(fru, status);
}
