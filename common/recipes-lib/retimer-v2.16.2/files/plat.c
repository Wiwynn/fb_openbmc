/* this file provides platform specific implementation */

#include <stdio.h>
#include <stdint.h>

#include "plat.h"
#include "aries_api_types.h"

uint8_t bus = 0; //0'base
uint8_t addr = 0;  //8 bits AL
uint8_t width = 0;
AriesDevicePartType type = ARIES_PTX16;

void __attribute__((weak))
plat_rt_preinit(void *args) {
  struct retimer_config *config = (struct retimer_config *)args;

  bus = config->retimer_bus;
  addr = config->retimer_addr;
  width = config->retimer_width;

  printf("setup retimer_bus:%u retimer_addr:0x%02X \n", bus, addr);
}
