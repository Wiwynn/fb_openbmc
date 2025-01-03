#include <syslog.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <plat.h>
#include "aries_api_types.h"
#include "aries_error.h"
#include <facebook/bic_xfer.h>

uint8_t slot_id = 1;
AriesDevicePartType type = ARIES_PTX08;
uint8_t bus = 0; //0'base
uint8_t addr = 0;  //8 bits AL
uint8_t width = 0;

void plat_rt_preinit(void *args) {
  struct retimer_config *config = (struct retimer_config *)args;

  slot_id = config->slot_id;
  type = config->type;
  bus = config->retimer_bus;
  addr = config->retimer_addr;
  width = config->retimer_width;

  printf("setup slot_id:%u bus:%u addr:0x%02X \n", slot_id, bus, addr);
}

int asteraI2COpenConnection(int i2cBus, int slaveAddress)
{
  (void)i2cBus;  /* unused */
  (void)slaveAddress;  /* unused */

  return 0;
}

AriesErrorType asteraI2CWriteBlockData(int handle, uint8_t cmdCode, uint8_t numBytes,
                            uint8_t* buf)
{
  (void)handle; /* unused */

  int ret = 0;
  uint8_t tlen = 0;
  uint8_t rlen = 0;
  uint8_t tbuf[64] = {0};
  uint8_t rbuf[64] = {0};

  if (numBytes > 32) {
    syslog(LOG_WARNING,"asteraI2CWriteBlockData write length > 32, numBytes: %u ", numBytes);
    numBytes = 32;
  }

  tbuf[0] = (bus << 1) + 1;
  tbuf[1] = addr;
  tbuf[2] = 0x00; //read cnt
  tbuf[3] = cmdCode;
  tbuf[4] = numBytes;
  memcpy(&tbuf[5], buf, numBytes);
  tlen = 5 + numBytes;

  ret = bic_data_wrapper(slot_id, NETFN_APP_REQ, CMD_APP_MASTER_WRITE_READ, tbuf, tlen, rbuf, &rlen);

  if (ret != 0) {
    printf("asteraI2CWriteBlockData fail \n");
  }

  return ret;
}

int asteraI2CReadBlockData(int handle, uint8_t cmdCode, uint8_t numBytes,
                           uint8_t* buf)
{
  (void)handle; /* unused */

  int ret = 0;
  uint8_t tlen = 0;
  uint8_t rlen = 0;
  uint8_t tbuf[64] = {0};
  uint8_t rbuf[64] = {0};

  tbuf[0] = (bus << 1) + 1;
  tbuf[1] = addr;
  tbuf[2] = numBytes; //read cnt
  tbuf[3] = cmdCode;
  tlen = 4;
  ret = bic_data_wrapper(slot_id, NETFN_APP_REQ, CMD_APP_MASTER_WRITE_READ, tbuf, tlen, rbuf, &rlen);
  if (ret == 0) {
    memcpy(buf, rbuf, numBytes);
    return numBytes;
  } else {
    printf("bic_data_wrapper fail ret:%d \n", ret);
  }
  
  return ret;
}

int asteraI2CWriteReadBlockData(int handle, uint8_t cmdCode, uint8_t numBytes,
                                uint8_t* buf)
{
  (void)handle; /* unused */
  (void)cmdCode; /* unused */
  (void)numBytes; /* unused */
  (void)buf; /* unused */

  return -1;
}