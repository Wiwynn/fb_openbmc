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
 * @file aries_link.h
 * @brief Definition of Link level functions for the SDK.
 */
#pragma once
#ifndef ASTERA_ARIES_SDK_LINK_H_
#define ASTERA_ARIES_SDK_LINK_H_

#include "aries_globals.h"
#include "aries_error.h"
#include "aries_api_types.h"
#include "astera_log.h"
#include "aries_api.h"
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
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

AriesErrorType ariesSetPcieReset(AriesLinkType* link, uint8_t reset);

AriesErrorType ariesCheckLinkHealth(AriesLinkType* link);

AriesErrorType ariesLinkGetCurrentWidth(AriesLinkType* link, int* currentWidth);

AriesErrorType ariesGetLinkRecoveryCount(AriesLinkType* link,
                                         int* recoveryCount);

AriesErrorType ariesClearLinkRecoveryCount(AriesLinkType* link);

AriesErrorType ariesGetLinkState(AriesLinkType* link);

AriesErrorType ariesGetLinkStateDetailed(AriesLinkType* link);

AriesErrorType ariesLTSSMLoggerInit(AriesLinkType* link, uint8_t oneBatchModeEn,
                                    AriesLTSSMVerbosityType verbosity);

AriesErrorType ariesLTSSMLoggerPrintEn(AriesLinkType* link, uint8_t printEn);

AriesErrorType ariesLTSSMLoggerClear(AriesLinkType* link);

AriesErrorType ariesLTSSMLoggerReadEntry(AriesLinkType* link,
                                         AriesLTSSMLoggerEnumType logType,
                                         int* offset,
                                         AriesLTSSMEntryType* entry);

AriesErrorType ariesLinkDumpDebugInfo(AriesLinkType* link);

AriesErrorType ariesLinkPrintMicroLogs(AriesLinkType* link,
                                       const char* basepath,
                                       const char* filename);

AriesErrorType ariesLinkPrintDetailedState(AriesLinkType* link,
                                           const char* basepath,
                                           const char* filename);

AriesErrorType ariesPrintLog(AriesLinkType* link, AriesLTSSMLoggerEnumType log,
                             FILE* fp);

AriesErrorType ariesLinkPresetRequestListSet(AriesLinkType* link,
                                             AriesPseudoPortType port,
                                             AriesMaxDataRateType rate,
                                             uint16_t laneMask,
                                             uint16_t presetOneHotList);

#ifdef __cplusplus
}
#endif

#endif /* ASTERA_ARIES_SDK_LINK_H_ */
