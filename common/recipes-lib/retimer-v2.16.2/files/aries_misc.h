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
 * @file aries_misc.h
 * @brief Definition of helper functions used by aries SDK.
 */
#pragma once
#ifndef ASTERA_ARIES_SDK_MISC_H_
#define ASTERA_ARIES_SDK_MISC_H_

#include "aries_globals.h"
#include "aries_error.h"
#include "aries_api_types.h"
#include "astera_log.h"
#include "aries_api.h"
#include "aries_i2c.h"

#ifdef ARIES_MPW
#include "aries_mpw_reg_defines.h"
#else
#include "aries_a0_reg_defines.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARIES_MAIN_MICRO_EXT_CSR_I2C_MST_INIT_CTRL_BIT_BANG_MODE_EN_GET(x)     \
    (((x)&0x04) >> 2)
#define ARIES_MAIN_MICRO_EXT_CSR_I2C_MST_INIT_CTRL_BIT_BANG_MODE_EN_SET(x)     \
    (((x) << 2) & 0x04)
#define ARIES_MAIN_MICRO_EXT_CSR_I2C_MST_INIT_CTRL_BIT_BANG_MODE_EN_MODIFY(r,  \
                                                                           x)  \
    ((((x) << 2) & 0x04) | ((r)&0xfb))

/* Bifurcation modes lookup */
extern AriesBifurcationParamsType bifurcationModes[36];

AriesErrorType ariesReadFwVersion(AriesI2CDriverType* i2cDriver, int offset,
                                  uint8_t* dataVal);

int ariesFirmwareIsAtLeast(AriesDeviceType* device, uint8_t major,
                           uint8_t minor, uint16_t build);

AriesErrorType ariesI2CMasterSetPage(AriesI2CDriverType* i2cDriver, int page);

AriesErrorType ariesI2CMasterSendByteBlockData(AriesI2CDriverType* i2cDriver,
                                               int address, int numBytes,
                                               uint8_t* value);

AriesErrorType ariesI2CMasterWriteCtrlReg(AriesI2CDriverType* i2cDriver,
                                          uint32_t address,
                                          uint8_t lengthDataBytes,
                                          uint8_t* values);

AriesErrorType ariesI2CMasterInit(AriesI2CDriverType* i2cDriver);

AriesErrorType ariesI2CMasterSendByte(AriesI2CDriverType* i2cDriver,
                                      uint8_t* value, int flag);

int ariesGetEEPROMImageEnd(uint8_t* data);

AriesErrorType ariesI2CMasterMultiBlockWrite(AriesDeviceType* device,
                                             uint16_t address, int numBytes,
                                             uint8_t* values);

AriesErrorType ariesI2CMasterRewriteAndVerifyByte(AriesI2CDriverType* i2cDriver,
                                                  int address, uint8_t* value);

AriesErrorType ariesI2CMasterSendAddress(AriesI2CDriverType* i2cDriver,
                                         int address);

AriesErrorType ariesI2CMasterReceiveByteBlock(AriesDeviceType* device,
                                              uint8_t* value);

AriesErrorType ariesI2CMasterGetChecksum(AriesDeviceType* device,
                                         uint16_t blockEnd, uint32_t* checksum);

AriesErrorType ariesI2CMasterSetFrequency(AriesI2CDriverType* i2cDriver,
                                          int frequencyHz);

AriesErrorType ariesI2CMasterReceiveByte(AriesI2CDriverType* i2cDriver,
                                         uint8_t* value);

AriesErrorType
    ariesI2CMasterReceiveContinuousByte(AriesI2CDriverType* i2cDriver,
                                        uint8_t* value);

AriesErrorType ariesGetTempCalibrationCodes(AriesDeviceType* device);

AriesErrorType ariesReadPmaTempMax(AriesDeviceType* device);

AriesErrorType ariesReadPmaAvgTemp(AriesDeviceType* device);

AriesErrorType ariesReadPmaAvgTempDirect(AriesDeviceType* device);

AriesErrorType ariesEnableThermalShutdown(AriesDeviceType* device);

AriesErrorType ariesDisableThermalShutdown(AriesDeviceType* device);

AriesErrorType ariesReadPmaTemp(AriesDeviceType* device, int side, int qs,
                                float* temperature_C);

AriesErrorType ariesGetPortOrientation(AriesDeviceType* device,
                                       int* orientation);

int ariesGetPmaNumber(int absLane);

int ariesGetPmaLane(int absLane);

int ariesGetPathID(int lane, int direction);

int ariesGetPathLaneID(int lane);

void ariesGetQSPathInfo(int lane, int direction, int* qs, int* qsPath,
                        int* qsPathLane);

int ariesGetStartLane(AriesLinkType* link);

AriesErrorType ariesDumpPMARegs(AriesDeviceType* device, uint16_t* pma_addr,
                                int len, char* filename);

AriesErrorType ariesGetLinkRxTerm(AriesLinkType* link, int side, int lane,
                                  int* term);

AriesErrorType ariesGetLinkId(AriesBifurcationType bifMode, int lane,
                              int* linkNum);

AriesErrorType ariesGetLinkCurrentSpeed(AriesLinkType* link, int lane,
                                        int direction, float* speed);

AriesErrorType ariesGetLaneNum(AriesLinkType* link, int lane, int* laneNum);

AriesErrorType ariesGetLogicalLaneNum(AriesLinkType* link, int lane,
                                      int direction, int* laneNum);

AriesErrorType ariesGetTxPre(AriesLinkType* link, int lane, int direction,
                             int* txPre);

AriesErrorType ariesGetTxCur(AriesLinkType* link, int lane, int direction,
                             int* txCur);

AriesErrorType ariesGetTxPst(AriesLinkType* link, int lane, int direction,
                             int* txPst);

AriesErrorType ariesGetRxPolarityCode(AriesLinkType* link, int lane,
                                      int direction, int pinSet, int* polarity);

AriesErrorType ariesGetRxAttCode(AriesLinkType* link, int side, int absLane,
                                 int* code);

AriesErrorType ariesGetRxCtleBoostCode(AriesLinkType* link, int side,
                                       int absLane, int* boostCode);

AriesErrorType ariesGetRxVgaCode(AriesLinkType* link, int side, int absLane,
                                 int* vgaCode);

float ariesGetRxBoostValueDb(int boostCode, float attValDb, int vgaCode);

AriesErrorType ariesGetRxCtlePoleCode(AriesLinkType* link, int side,
                                      int absLane, int* poleCode);

AriesErrorType ariesGetRxAdaptIq(AriesLinkType* link, int side, int absLane,
                                 int* iqValue);

AriesErrorType ariesGetRxAdaptIqBank(AriesLinkType* link, int side, int absLane,
                                     int bank, int* iqValue);

AriesErrorType ariesGetRxAdaptDoneBank(AriesLinkType* link, int side,
                                       int absLane, int bank, int* doneValue);

AriesErrorType ariesGetRxDfeCode(AriesLinkType* link, int side, int absLane,
                                 int tapNum, int* dfeCode);

AriesErrorType ariesGetLastEqSpeed(AriesLinkType* link, int lane, int direction,
                                   int* speed);

AriesErrorType ariesGetDeskewStatus(AriesLinkType* link, int lane,
                                    int direction, int* status);

AriesErrorType ariesGetDeskewClks(AriesLinkType* link, int lane, int direction,
                                  int* val);

AriesErrorType ariesGetLastEqReqPre(AriesLinkType* link, int lane,
                                    int direction, int* val);

AriesErrorType ariesGetLastEqReqCur(AriesLinkType* link, int lane,
                                    int direction, int* val);

AriesErrorType ariesGetLastEqReqPst(AriesLinkType* link, int lane,
                                    int direction, int* val);

AriesErrorType ariesGetLastEqReqPreset(AriesLinkType* link, int lane,
                                       int direction, int* val);

AriesErrorType ariesGetLastEqPresetReq(AriesLinkType* link, int lane,
                                       int direction, int reqNum, int* val);

AriesErrorType ariesGetLastEqPresetReq(AriesLinkType* link, int lane,
                                       int direction, int reqNum, int* val);

AriesErrorType ariesGetLastEqPresetReqFOM(AriesLinkType* link, int lane,
                                          int direction, int reqNum, int* val);

AriesErrorType ariesGetLastEqNumPresetReq(AriesLinkType* link, int lane,
                                          int direction, int* val);

AriesErrorType ariesGetLoggerFmtID(AriesLinkType* link,
                                   AriesLTSSMLoggerEnumType loggerType,
                                   int offset, int* fmtID);

AriesErrorType ariesGetLoggerWriteOffset(AriesLinkType* link,
                                         AriesLTSSMLoggerEnumType loggerType,
                                         int* writeOffset);

AriesErrorType ariesGetLoggerOneBatchModeEn(AriesLinkType* link,
                                            AriesLTSSMLoggerEnumType loggerType,
                                            int* oneBatchModeEn);

AriesErrorType ariesGetLoggerOneBatchWrEn(AriesLinkType* link,
                                          AriesLTSSMLoggerEnumType loggerType,
                                          int* oneBatchWrEn);

uint8_t ariesGetPecByte(uint8_t* polynomial, uint8_t length);

AriesErrorType ariesGetMinFoMVal(AriesDeviceType* device, int side, int pathID,
                                 int lane, int regOffset, uint8_t* data);

AriesErrorType ariesGetPinMap(AriesDeviceType* device);

// Read numBytes bytes of data starting at startAddr
AriesErrorType ariesEepromReadBlockData(AriesDeviceType* device,
                                        uint8_t* values, int startAddr,
                                        uint8_t numBytes);

// Read numBytes bytes from the EEPROM starting at startAddr and
// calculate a running checksum (e.g. add the bytes as you read them):
// uint8_t checksum = (checksum + new_byte) % 256
AriesErrorType ariesEepromCalcChecksum(AriesDeviceType* device, int startAddr,
                                       int numBytes, uint8_t* checksum);

void ariesSortArray(uint16_t* arr, int size);

uint16_t ariesGetMedian(uint16_t* arr, int size);

AriesErrorType ariesLoadBinFile(const char* filename, uint8_t* mem);

AriesErrorType ariesLoadIhxFile(const char* filename, uint8_t* mem);

AriesErrorType ariesParseIhxLine(char* line, uint8_t* bytes, int* addr,
                                 int* num, int* code);

AriesErrorType ariesI2cMasterSoftReset(AriesI2CDriverType* i2cDriver);

void ariesGetCrcBytesImage(uint8_t* image, uint8_t* crcBytes,
                           uint8_t* numCrcBytes);

AriesErrorType ariesEEPROMGetBlockLength(AriesI2CDriverType* i2cDriver,
                                         int blockStartAddr, int* blockLength);

AriesErrorType ariesEEPROMGetRandomByte(AriesI2CDriverType* i2cDriver, int addr,
                                        uint8_t* value);

AriesErrorType ariesGetEEPROMBlockCrcByte(AriesI2CDriverType* i2cDriver,
                                          int blockStartAddr, int blockLength,
                                          uint8_t* crcByte);

AriesErrorType ariesGetEEPROMBlockType(AriesI2CDriverType* i2cDriver,
                                       int blockStartAddr, uint8_t* blockType);

AriesErrorType ariesGetEEPROMFirstBlock(AriesI2CDriverType* i2cDriver,
                                        int* blockStartAddr);

AriesErrorType ariesGetPathFWState(AriesLinkType* link, int lane, int direction,
                                   int* state);

AriesErrorType ariesGetPathHWState(AriesLinkType* link, int lane, int direction,
                                   int* state);

AriesErrorType ariesSetPortOrientation(AriesDeviceType* device,
                                       uint8_t orientation);

AriesErrorType ariesPipeRxAdapt(AriesDeviceType* device, int side, int lane);

AriesErrorType ariesPipeFomGet(AriesDeviceType* device, int side, int lane,
                               int* fom);

AriesErrorType ariesPipeRxStandbySet(AriesDeviceType* device, int side,
                                     int lane, bool value);

AriesErrorType ariesPipeTxElecIdleSet(AriesDeviceType* device, int side,
                                      int lane, bool value);

AriesErrorType ariesPipeRxEqEval(AriesDeviceType* device, int side, int lane,
                                 bool value);

AriesErrorType ariesPipeRxEqInProgressSet(AriesDeviceType* device, int side,
                                          int lane, bool value);

AriesErrorType ariesPipePhyStatusClear(AriesDeviceType* device, int side,
                                       int lane);

AriesErrorType ariesPipePhyStatusGet(AriesDeviceType* device, int side,
                                     int lane, bool* value);

AriesErrorType ariesPipePhyStatusToggle(AriesDeviceType* device, int side,
                                        int lane);

AriesErrorType ariesPipePowerdownSet(AriesDeviceType* device, int side,
                                     int lane, int value);

AriesErrorType ariesPipePowerdownCheck(AriesDeviceType* device, int side,
                                       int lane, int value);

AriesErrorType ariesPipeRateChange(AriesDeviceType* device, int side, int lane,
                                   int rate);

AriesErrorType ariesPipeRateCheck(AriesDeviceType* device, int side, int lane,
                                  int rate);

AriesErrorType ariesPipeDeepmhasisSet(AriesDeviceType* device, int side,
                                      int lane, int de, int preset, int pre,
                                      int main, int pst);

AriesErrorType ariesPipeRxPolaritySet(AriesDeviceType* device, int side,
                                      int lane, int value);

AriesErrorType ariesPipeRxTermSet(AriesDeviceType* device, int side, int lane,
                                  bool value);

AriesErrorType ariesPipeBlkAlgnCtrlSet(AriesDeviceType* device, int side,
                                       int lane, bool value, bool enable);

AriesErrorType ariesPMABertPatChkSts(AriesDeviceType* device, int side,
                                     int lane, int* ecount);

AriesErrorType ariesPMABertPatChkToggleSync(AriesDeviceType* device, int side,
                                            int lane);

AriesErrorType ariesPMABertPatChkDetectCorrectPolarity(AriesDeviceType* device,
                                                       int side, int lane);

AriesErrorType ariesPMARxInvertSet(AriesDeviceType* device, int side, int lane,
                                   bool invert, bool override);

AriesErrorType ariesPMABertPatChkConfig(AriesDeviceType* device, int side,
                                        int lane, AriesPRBSPatternType mode);

AriesErrorType ariesPMABertPatGenConfig(AriesDeviceType* device, int side,
                                        int lane, AriesPRBSPatternType mode);

AriesErrorType ariesPMARxDataEnSet(AriesDeviceType* device, int side, int lane,
                                   bool value);

AriesErrorType ariesPMATxDataEnSet(AriesDeviceType* device, int side, int lane,
                                   bool value);

AriesErrorType ariesPMAPCSRxRecalBankOvrdSet(AriesDeviceType* device, int side,
                                             int lane, bool enable);

AriesErrorType ariesPMAPCSRxReqBlock(AriesDeviceType* device, int side,
                                     int lane, bool enable);

AriesErrorType ariesPMAVregVrefSet(AriesDeviceType* device, int side, int lane,
                                   int rate);

AriesErrorType ariesReadBlockDataForceError(AriesI2CDriverType* i2cDriver,
                                            uint32_t address, uint8_t numBytes,
                                            uint8_t* values);

float ariesTsenseADCToDegrees(AriesDeviceType* device, int adcCode,
                              uint8_t calCode);

#ifdef __cplusplus
}
#endif

#endif /* ASTERA_ARIES_SDK_MISC_H_ */
