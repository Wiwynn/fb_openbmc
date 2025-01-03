/* this file provides platform specific definitions */

#include "aries_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t bus; //0'base
extern uint8_t addr;  //8 bits AL
extern uint8_t width;
extern AriesDevicePartType type;

void plat_rt_preinit(void *args);

struct retimer_config {
  AriesDevicePartType type;
  uint8_t retimer_bus; //0'base
  uint8_t retimer_addr;  //8 bits
  uint8_t retimer_width;
};

#ifdef __cplusplus
} // extern "C"
#endif