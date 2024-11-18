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
 * @file aries_api.h
 * @brief Definition of public functions for the SDK.
 */
#pragma once
#ifndef ASTERA_ARIES_SDK_API_H_
#define ASTERA_ARIES_SDK_API_H_

#include "aries_globals.h"
#include "aries_error.h"
#include "aries_api_types.h"
#include "astera_log.h"
#include "aries_i2c.h"
#include "aries_link.h"
#include "aries_misc.h"

#ifdef ARIES_MPW
#include "aries_mpw_reg_defines.h"
#else
#include "aries_a0_reg_defines.h"
#endif

#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARIES_SDK_VERSION "2.16.2"

const char* ariesGetSDKVersion(void);

AriesErrorType ariesInitDevice(AriesDeviceType* device, uint8_t recoveryAddr);

AriesErrorType ariesDeviceRevisionGet(AriesDeviceType* device);

AriesErrorType ariesCreateLinkStructs(AriesDeviceType* device,
                                      AriesLinkType* links);

AriesErrorType ariesFWStatusCheck(AriesDeviceType* device);

AriesErrorType ariesComplianceFWGet(AriesDeviceType* device);

AriesErrorType ariesSetEEPROMPageSize(AriesDeviceType* device, int pageSize);

AriesErrorType ariesSetBifurcationMode(AriesDeviceType* device,
                                       AriesBifurcationType bifur);

AriesErrorType ariesGetBifurcationMode(AriesDeviceType* device,
                                       AriesBifurcationType* bifur);

AriesErrorType ariesSetHwReset(AriesDeviceType* device, uint8_t reset);

AriesErrorType ariesSetMMI2CMasterReset(AriesDeviceType* device, uint8_t reset);

AriesErrorType ariesSetI2CMasterReset(AriesDeviceType* device, uint8_t reset);

AriesErrorType ariesUpdateFirmware(AriesDeviceType* device,
                                   const char* filename,
                                   AriesFWImageFormatType fileType);

AriesErrorType ariesUpdateFirmwareViaBuffer(AriesDeviceType* device,
                                            uint8_t* image);

AriesErrorType ariesFirmwareUpdateProgress(AriesDeviceType* device,
                                           uint8_t* percentComplete);

AriesErrorType ariesWriteEEPROMImage(AriesDeviceType* device, uint8_t* values,
                                     bool legacyMode);

AriesErrorType ariesVerifyEEPROMImage(AriesDeviceType* device, uint8_t* values,
                                      bool legacyMode);

AriesErrorType ariesVerifyEEPROMImageViaChecksum(AriesDeviceType* device,
                                                 uint8_t* image);

AriesErrorType ariesCheckEEPROMCrc(AriesDeviceType* device, uint8_t* image);

AriesErrorType ariesCheckEEPROMImageCrcBytes(AriesDeviceType* device,
                                             uint8_t* crcBytes,
                                             uint8_t* numCrcBytes);

AriesErrorType ariesReadEEPROMByte(AriesDeviceType* device, int addr,
                                   uint8_t* value);

AriesErrorType ariesWriteEEPROMByte(AriesDeviceType* device, int addr,
                                    uint8_t* value);

AriesErrorType ariesCheckConnectionHealth(AriesDeviceType* device,
                                          uint8_t slaveAddress);

AriesErrorType ariesCheckDeviceHealth(AriesDeviceType* device);

AriesErrorType ariesGetMaxTemp(AriesDeviceType* device);

AriesErrorType ariesGetCurrentTemp(AriesDeviceType* device);

AriesErrorType ariesSetMaxDataRate(AriesDeviceType* device,
                                   AriesMaxDataRateType rate);

AriesErrorType ariesSetGPIO(AriesDeviceType* device, int gpioNum, bool value);

AriesErrorType ariesGetGPIO(AriesDeviceType* device, int gpioNum, bool* value);

AriesErrorType ariesToggleGPIO(AriesDeviceType* device, int gpioNum);

AriesErrorType ariesSetGPIODirection(AriesDeviceType* device, int gpioNum,
                                     bool value);

AriesErrorType ariesGetGPIODirection(AriesDeviceType* device, int gpioNum,
                                     bool* value);

AriesErrorType ariesTestModeEnable(AriesDeviceType* device);

AriesErrorType ariesTestModeDisable(AriesDeviceType* device);

AriesErrorType ariesTestModeRateChange(AriesDeviceType* device,
                                       AriesMaxDataRateType rate);

AriesErrorType ariesTestModeTxConfig(AriesDeviceType* device,
                                     AriesPRBSPatternType pattern, int preset,
                                     bool enable);

AriesErrorType ariesTestModeTxConfigLane(AriesDeviceType* device,
                                         AriesPRBSPatternType pattern,
                                         int preset, int side, int lane,
                                         bool enable);

AriesErrorType ariesTestModeRxConfig(AriesDeviceType* device,
                                     AriesPRBSPatternType pattern, bool enable);

AriesErrorType ariesTestModeRxEcountRead(AriesDeviceType* device, int* ecount);

AriesErrorType ariesTestModeRxEcountClear(AriesDeviceType* device);

AriesErrorType ariesTestModeRxEcountClearLane(AriesDeviceType* device, int side,
                                              int lane);

AriesErrorType ariesTestModeRxFomRead(AriesDeviceType* device, int* fom);

AriesErrorType ariesTestModeRxValidRead(AriesDeviceType* device);

AriesErrorType ariesTestModeTxErrorInject(AriesDeviceType* device);

AriesErrorType ariesFWReloadOnNextSBR(AriesDeviceType* device);

AriesErrorType ariesOvertempStatusGet(AriesDeviceType* device, bool* value);

AriesErrorType ariesGetI2CBusClearEventStatus(AriesDeviceType* device,
                                              int* status);

AriesErrorType ariesClearI2CBusClearEventStatus(AriesDeviceType* device);

#ifdef __cplusplus
}
#endif

#endif /* ASTERA_ARIES_SDK_API_H_ */
