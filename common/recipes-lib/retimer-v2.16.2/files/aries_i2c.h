/*
 * Copyright 2020 Astera Labs, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file aries_i2c.h
 * @brief Definition of I2C/SMBus types for the SDK.
 */
#pragma once
#ifndef ASTERA_ARIES_SDK_I2C_H_
#define ASTERA_ARIES_SDK_I2C_H_

#include "aries_globals.h"
#include "aries_error.h"
#include "aries_api_types.h"
#include "astera_log.h"
#include "aries_misc.h"

#ifdef ARIES_MPW
#include "aries_mpw_reg_defines.h"
#else
#include "aries_a0_reg_defines.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHECK_SUCCESS(rc)                                                      \
    {                                                                          \
        if (rc != ARIES_SUCCESS)                                               \
        {                                                                      \
            ASTERA_ERROR("Unexpected return code: %d", rc);                    \
            return rc;                                                         \
        }                                                                      \
    }

int asteraI2COpenConnection(int i2cBus, int slaveAddress);

AriesErrorType asteraI2CWriteBlockData(int handle, uint8_t cmdCode, uint8_t numBytes,
                            uint8_t* buf);

AriesErrorType asteraI2CReadBlockData(int handle, uint8_t cmdCode, uint8_t numBytes,
                           uint8_t* buf);

AriesErrorType asteraI2CWriteReadBlockData(int handle, uint8_t cmdCode, uint8_t numBytes,
                                uint8_t* buf);

AriesErrorType asteraI2CBlock(int handle);

AriesErrorType asteraI2CUnblock(int handle);

AriesErrorType ariesRunArp(int handle, uint8_t new7bitSmbusAddr);

AriesErrorType ariesWriteBlockData(AriesI2CDriverType* i2cDriver,
                                   uint32_t address, uint8_t numBytes,
                                   uint8_t* values);

AriesErrorType ariesWriteByteData(AriesI2CDriverType* i2cDriver,
                                  uint32_t address, uint8_t* value);

AriesErrorType ariesReadBlockData(AriesI2CDriverType* i2cDriver,
                                  uint32_t address, uint8_t numBytes,
                                  uint8_t* values);

AriesErrorType ariesReadByteData(AriesI2CDriverType* i2cDriver,
                                 uint32_t address, uint8_t* value);

AriesErrorType ariesWriteReadBlockData(AriesI2CDriverType* i2cDriver,
                                       uint32_t address, uint8_t numBytes,
                                       uint8_t* values);

AriesErrorType ariesReadBlockDataMainMicroIndirectA0(
    AriesI2CDriverType* i2cDriver, uint32_t microIndStructOffset,
    uint32_t address, uint8_t numBytes, uint8_t* values);

AriesErrorType ariesReadBlockDataMainMicroIndirectMPW(
    AriesI2CDriverType* i2cDriver, uint32_t microIndStructOffset,
    uint32_t address, uint8_t numBytes, uint8_t* values);

AriesErrorType ariesWriteBlockDataMainMicroIndirectA0(
    AriesI2CDriverType* i2cDriver, uint32_t microIndStructOffset,
    uint32_t address, uint8_t numBytes, uint8_t* values);

AriesErrorType ariesWriteBlockDataMainMicroIndirectMPW(
    AriesI2CDriverType* i2cDriver, uint32_t microIndStructOffset,
    uint32_t address, uint8_t numBytes, uint8_t* values);

AriesErrorType ariesReadByteDataMainMicroIndirect(AriesI2CDriverType* i2cDriver,
                                                  uint32_t address,
                                                  uint8_t* values);

AriesErrorType
    ariesReadBlockDataMainMicroIndirect(AriesI2CDriverType* i2cDriver,
                                        uint32_t address, uint8_t numBytes,
                                        uint8_t* values);

AriesErrorType
    ariesWriteByteDataMainMicroIndirect(AriesI2CDriverType* i2cDriver,
                                        uint32_t address, uint8_t* value);

AriesErrorType
    ariesWriteBlockDataMainMicroIndirect(AriesI2CDriverType* i2cDriver,
                                         uint32_t address, uint8_t numBytes,
                                         uint8_t* values);

AriesErrorType ariesReadByteDataPathMicroIndirect(AriesI2CDriverType* i2cDriver,
                                                  uint8_t pathID,
                                                  uint32_t address,
                                                  uint8_t* value);

AriesErrorType
    ariesReadBlockDataPathMicroIndirect(AriesI2CDriverType* i2cDriver,
                                        uint8_t pathID, uint32_t address,
                                        uint8_t numBytes, uint8_t* values);

AriesErrorType
    ariesWriteByteDataPathMicroIndirect(AriesI2CDriverType* i2cDriver,
                                        uint8_t pathID, uint32_t address,
                                        uint8_t* value);

AriesErrorType
    ariesWriteBlockDataPathMicroIndirect(AriesI2CDriverType* i2cDriver,
                                         uint8_t pathID, uint32_t address,
                                         uint8_t numBytes, uint8_t* values);

AriesErrorType ariesReadWordPmaIndirect(AriesI2CDriverType* i2cDriver, int side,
                                        int quadSlice, uint16_t address,
                                        uint8_t* values);

AriesErrorType ariesWriteWordPmaIndirect(AriesI2CDriverType* i2cDriver,
                                         int side, int quadSlice,
                                         uint16_t address, uint8_t* values);

AriesErrorType ariesReadWordPmaLaneIndirect(AriesI2CDriverType* i2cDriver,
                                            int side, int quadSlice, int lane,
                                            uint16_t regOffset,
                                            uint8_t* values);

AriesErrorType ariesWriteWordPmaLaneIndirect(AriesI2CDriverType* i2cDriver,
                                             int side, int quadSlice, int lane,
                                             uint16_t regOffset,
                                             uint8_t* values);

AriesErrorType ariesReadWordPmaMainMicroIndirect(AriesI2CDriverType* i2cDriver,
                                                 int side, int quadSlice,
                                                 uint16_t pmaAddr,
                                                 uint8_t* data);

AriesErrorType ariesWriteWordPmaMainMicroIndirect(AriesI2CDriverType* i2cDriver,
                                                  int side, int quadSlice,
                                                  uint16_t pmaAddr,
                                                  uint8_t* data);

AriesErrorType
    ariesReadWordPmaLaneMainMicroIndirect(AriesI2CDriverType* i2cDriver,
                                          int side, int quadSlice, int lane,
                                          uint16_t regOffset, uint8_t* values);

AriesErrorType
    ariesWriteWordPmaLaneMainMicroIndirect(AriesI2CDriverType* i2cDriver,
                                           int side, int quadSlice, int lane,
                                           uint16_t regOffset, uint8_t* values);

AriesErrorType ariesReadWriteWordPmaLaneMainMicroIndirect(
    AriesI2CDriverType* i2cDriver, int side, int quadSlice, int quadSliceLane,
    uint16_t pmaAddr, int offset, uint16_t value, int width);

AriesErrorType ariesReadRetimerRegister(AriesI2CDriverType* i2cDriver, int side,
                                        int lane, uint16_t baseAddr,
                                        uint8_t numBytes, uint8_t* data);

AriesErrorType ariesWriteRetimerRegister(AriesI2CDriverType* i2cDriver,
                                         int side, int lane, uint16_t baseAddr,
                                         uint8_t numBytes, uint8_t* data);

AriesErrorType ariesReadWideRegister(AriesI2CDriverType* i2cDriver,
                                     uint32_t address, uint8_t width,
                                     uint8_t* values);

AriesErrorType ariesWriteWideRegister(AriesI2CDriverType* i2cDriver,
                                      uint32_t address, uint8_t width,
                                      uint8_t* values);

AriesErrorType ariesLock(AriesI2CDriverType* i2cDriver);

AriesErrorType ariesUnlock(AriesI2CDriverType* i2cDriver);

#ifdef __cplusplus
}
#endif

#endif /* ASTERA_ARIES_SDK_I2C_H_ */
