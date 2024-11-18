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
 * @file aries_margin.h
 * @brief Definition of receiver margining types for the SDK.
 */
#pragma once
#ifndef ASTERA_ARIES_SDK_MARGIN_H_
#define ASTERA_ARIES_SDK_MARGIN_H_

#include "aries_globals.h"
#include "aries_error.h"
#include "aries_api_types.h"
#include "astera_log.h"
#include "aries_api.h"
#include "aries_i2c.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// Constants describing margining actions
#define NUMTIMINGSTEPS 14
#define MAXTIMINGOFFSET 40
#define NUMVOLTAGESTEPS 108
#define MAXVOLTAGEOFFSET 20
#define SAMPLINGRATEVOLTAGE 31
#define SAMPLINGRATETIMING 31
#define MAXLANES 15
#define VOLTAGESUPPORTED true
#define INDUPDOWNVOLTAGE true
#define INDLEFTRIGHTTIMING true
#define SAMPLEREPORTINGMETHOD true
#define INDERRORSAMPLER true

AriesErrorType ariesMarginNoCommand(AriesRxMarginType* marginDevice);

AriesErrorType
    ariesMarginAccessRetimerRegister(AriesRxMarginType* marginDevice);

AriesErrorType
    ariesMarginReportMarginControlCapabilities(AriesRxMarginType* marginDevice,
                                               int* capabilities);

AriesErrorType ariesMarginReportNumVoltageSteps(AriesRxMarginType* marginDevice,
                                                int* numVoltageSteps);

AriesErrorType ariesMarginReportNumTimingSteps(AriesRxMarginType* marginDevice,
                                               int* numTimingSteps);

AriesErrorType ariesMarginReportMaxTimingOffset(AriesRxMarginType* marginDevice,
                                                int* maxTimingOffset);

AriesErrorType
    ariesMarginReportMaxVoltageOffset(AriesRxMarginType* marginDevice,
                                      int* maxVoltageOffset);

AriesErrorType
    ariesMarginReportSamplingRateVoltage(AriesRxMarginType* marginDevice,
                                         int* samplingRateVoltage);

AriesErrorType
    ariesMarginReportSamplingRateTiming(AriesRxMarginType* marginDevice,
                                        int* samplingRateTiming);

AriesErrorType ariesMarginReportSampleCount(AriesRxMarginType* marginDevice);

AriesErrorType ariesMarginReportMaxLanes(AriesRxMarginType* marginDevice,
                                         int* maxLanes);

AriesErrorType ariesMarginSetErrorCountLimit(AriesRxMarginType* marginDevice,
                                             int limit);

AriesErrorType ariesMarginGoToNormalSettings(AriesRxMarginType* marginDevice,
                                             AriesPseudoPortType port,
                                             int* lane, int laneCount);

AriesErrorType ariesMarginClearErrorLog(AriesRxMarginType* marginDevice,
                                        AriesPseudoPortType port,
                                        int laneCount);

AriesErrorType ariesMarginStepMarginToTimingOffset(
    AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane,
    int* direction, int* steps, int laneCount, float dwell);

AriesErrorType ariesMarginStepMarginToVoltageOffset(
    AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane,
    int* direction, int* steps, int laneCount, float dwell);

AriesErrorType ariesMarginVendorDefined(AriesRxMarginType* marginDevice);

AriesErrorType ariesMarginPmaRxMarginStop(AriesRxMarginType* marginDevice,
                                          AriesPseudoPortType port, int* lane,
                                          int laneCount);

AriesErrorType ariesMarginPmaRxMarginTiming(AriesRxMarginType* marginDevice,
                                            AriesPseudoPortType port, int* lane,
                                            int* direction, int* steps,
                                            int laneCount);

AriesErrorType ariesMarginPmaRxMarginVoltage(AriesRxMarginType* marginDevice,
                                             AriesPseudoPortType port,
                                             int* lane, int* direction,
                                             int* steps, int laneCount);

AriesErrorType ariesMarginPmaTimingVoltageOffset(
    AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane,
    int* timeDirection, int* timeSteps, int* voltageDirection,
    int* voltageSteps, int laneCount, float dwell);

AriesErrorType ariesMarginPmaRxMarginGetECount(AriesRxMarginType* marginDevice,
                                               AriesPseudoPortType port,
                                               int* lane, int laneCount);

AriesErrorType ariesMarginPmaRxReqAckHandshake(AriesRxMarginType* marginDevice,
                                               AriesPseudoPortType port,
                                               int* lane, int laneCount);

AriesErrorType ariesMarginDeterminePmaSideAndQs(AriesRxMarginType* marginDevice,
                                                AriesPseudoPortType port,
                                                int lane, int* side,
                                                int* quadSlice,
                                                int* quadSliceLane);

AriesErrorType ariesGetLaneRecoveryCount(AriesRxMarginType* marginDevice,
                                         int* lane, int laneCount,
                                         uint8_t* recoveryCount);

AriesErrorType ariesClearLaneRecoveryCount(AriesRxMarginType* marginDevice,
                                           int* lane, int laneCount);

AriesErrorType ariesCheckEye(AriesRxMarginType* marginDevice,
                             AriesPseudoPortType port, int* lane, int laneCount,
                             float dwell);

AriesErrorType ariesLogEye(AriesRxMarginType* marginDevice,
                           AriesPseudoPortType port, int width,
                           const char* filename, int startLane, float dwell);

AriesErrorType ariesLogEyeSerial(AriesRxMarginType* marginDevice,
                                 AriesPseudoPortType port, int width,
                                 const char* filename, int startLane,
                                 float dwell);

AriesErrorType ariesEyeDiagram(AriesRxMarginType* marginDevice,
                               AriesPseudoPortType port, int lane, int rate,
                               float dwell);

#ifdef __cplusplus
}
#endif

#endif /* ASTERA_ARIES_SDK_MARGIN_H_ */
