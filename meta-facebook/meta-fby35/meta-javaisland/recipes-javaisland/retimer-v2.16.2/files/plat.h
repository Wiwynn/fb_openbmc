#include "aries_api_types.h"

#define SMBUS_BLOCK_READ_UNSUPPORTED 1

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t bus; //0'base
extern uint8_t addr;  //8 bits AL
extern uint8_t width;
extern AriesDevicePartType type;

void plat_rt_preinit(void *args);

struct retimer_config {
  uint8_t slot_id;
  AriesDevicePartType type;
  uint8_t retimer_bus; //0'base
  uint8_t retimer_addr;  //8 bits
  uint8_t retimer_width;
};

#ifdef __cplusplus
} // extern "C"
#endif