/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2021 Intel Corporation.
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("License"). Unless the License provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit
 * this software or the related documents without Intel's prior written
 * permission.
 *
 * This software and the related documents are provided as is, with no express
 * or implied warranties, other than those that are expressly stated in the
 * License.
 *
 ******************************************************************************/

#ifndef CRASHDUMP_VALIDATOR_H
#define CRASHDUMP_VALIDATOR_H

#include "../engine/crashdump.h"

typedef struct
{
    bool validateInput;
} ValidatorParams;

bool IsMaxTimeValid(uint32_t maxTime, const ValidatorParams* ValidatorParams);
bool IsCrashDump_DiscoveryParamsValid(cJSON* params,
                                      const ValidatorParams* ValidatorParams);
bool IsCrashDump_GetFrameParamsValid(cJSON* params,
                                     const ValidatorParams* ValidatorParams);
bool IsGetCPUIDParamsValid(cJSON* params,
                           const ValidatorParams* ValidatorParams);
bool IsPingParamsValid(cJSON* params, const ValidatorParams* ValidatorParams);
bool IsRdPciConfigLocalParamsValid(cJSON* params,
                                   const ValidatorParams* ValidatorParams);
bool IsRdEndPointConfigMmioParamsValid(cJSON* params,
                                       const ValidatorParams* ValidatorParams);
bool IsRdEndPointConfigPciLocalParamsValid(
    cJSON* params, const ValidatorParams* ValidatorParams);
bool IsWrEndPointConfigPciLocalParamsValid(
    cJSON* params, const ValidatorParams* ValidatorParams);
bool IsRdIAMSRParamsValid(cJSON* params,
                          const ValidatorParams* ValidatorParams);
bool IsRdPkgConfigParamsValid(cJSON* params,
                              const ValidatorParams* ValidatorParams);
bool IsRdPkgConfigCoreParamsValid(cJSON* params,
                                  const ValidatorParams* ValidatorParams);
bool IsRdPostEnumBusParamsValid(cJSON* params,
                                const ValidatorParams* ValidatorParams);
bool IsRdChaCountParamsValid(cJSON* params,
                             const ValidatorParams* ValidatorParams);
bool IsValidHexString(char* str);
bool IsTelemetry_DiscoveryParamsValid(cJSON* params,
                                      const ValidatorParams* ValidatorParams);
bool IsRdAndConcatenateParamsValid(cJSON* params,
                                   const ValidatorParams* ValidatorParams);
bool IsRdGlobalVarsValid(cJSON* params, const ValidatorParams* ValidatorParams);
bool IsSaveStrVarsValid(cJSON* params, const ValidatorParams* ValidatorParams);

#endif // CRASHDUMP_VALIDATOR_H
