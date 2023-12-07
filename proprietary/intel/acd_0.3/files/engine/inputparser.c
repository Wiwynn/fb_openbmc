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

#include "inputparser.h"

#include "validator.h"

char* decodeState(int value)
{
    switch (value)
    {
        case STARTUP:
            return MD_STARTUP;
            break;
        case EVENT:
            return MD_EVENT;
            break;
        case OVERWRITTEN:
            return MD_OVERWRITTEN;
            break;
        case INVALID:
            return MD_INVALID;
            break;
        default:
            return MD_INVALID;
            break;
    }
}

void ReadLoops(cJSON* section, LoopOnFlags* loopOnFlags)
{
    loopOnFlags->loopOnCPU = false;
    loopOnFlags->loopOnCHA = false;
    loopOnFlags->loopOnCore = false;
    loopOnFlags->loopOnThread = false;
    loopOnFlags->loopOnCompute = false;
    loopOnFlags->loopOnIO = false;
    cJSON* LoopOn = cJSON_GetObjectItemCaseSensitive(section, "LoopOnCPU");
    if (LoopOn != NULL)
    {
        if (cJSON_IsBool(LoopOn) && cJSON_IsTrue(LoopOn))
        {
            loopOnFlags->loopOnCPU = true;
        }
    }
    LoopOn = cJSON_GetObjectItemCaseSensitive(section, "LoopOnCHA");
    if (LoopOn != NULL)
    {
        loopOnFlags->loopOnCHA = true;
    }
    LoopOn = cJSON_GetObjectItemCaseSensitive(section, "LoopOnCore");
    if (LoopOn != NULL)
    {
        loopOnFlags->loopOnCore = true;
    }
    LoopOn = cJSON_GetObjectItemCaseSensitive(section, "LoopOnThread");
    if (LoopOn != NULL)
    {
        loopOnFlags->loopOnThread = true;
    }
    LoopOn = cJSON_GetObjectItemCaseSensitive(section, "LoopOnCompute");
    if (LoopOn != NULL)
    {
        if (cJSON_IsBool(LoopOn) && cJSON_IsTrue(LoopOn))
        {
            loopOnFlags->loopOnCompute = true;
        }
    }
    LoopOn = cJSON_GetObjectItemCaseSensitive(section, "LoopOnIO");
    if (LoopOn != NULL)
    {
        if (cJSON_IsBool(LoopOn) && cJSON_IsTrue(LoopOn))
        {
            loopOnFlags->loopOnIO = true;
        }
    }
}

acdStatus ResetParams(cJSON* params, cJSON* paramsTracker)
{
    cJSON* it = NULL;
    cJSON_ArrayForEach(it, paramsTracker)
    {
        cJSON_ReplaceItemInArray(params, it->valueint,
                                 cJSON_CreateString(it->string));
    }
    return ACD_SUCCESS;
}

acdStatus UpdateParams(CPUInfo* cpuInfo, CmdInOut* cmdInOut,
                       LoggerStruct* loggerStruct, InputParserErrInfo* errInfo)
{
    for (int pos = 0; pos < cJSON_GetArraySize(cmdInOut->in.params); pos++)
    {
        cJSON* param = cJSON_GetArrayItem(cmdInOut->in.params, pos);
        if (cJSON_IsString(param))
        {
            cJSON* internalVar = cJSON_GetObjectItemCaseSensitive(
                cmdInOut->internalVarsTracker, param->valuestring);
            if (internalVar != NULL)
            {
                cJSON_AddItemToObject(cmdInOut->paramsTracker,
                                      param->valuestring,
                                      cJSON_CreateNumber(pos));
                cJSON_ReplaceItemInArray(
                    cmdInOut->in.params, pos,
                    cJSON_CreateNumber(
                        (double)strtoull(internalVar->valuestring, NULL, 16)));
            }
            else
            {
                int mismatchtarget = 1;
                int mismatchcha = 1;
                int mismatchcorethread = 1;
                int mismatchcore = 1;
                int mismatchDomainID = 1;
                int mismatchPCU = 1;
                int mismatchIoDomain = 1;
                int mismatchComputeDomain = 1;
                strcmp_s(TARGET, strnlen_s(TARGET, sizeof(TARGET)),
                         param->valuestring, &mismatchtarget);
                strcmp_s(CHA, strnlen_s(CHA, sizeof(CHA)), param->valuestring,
                         &mismatchcha);
                strcmp_s(CORETHREAD, strnlen_s(CORETHREAD, sizeof(CORETHREAD)),
                         param->valuestring, &mismatchcorethread);
                strcmp_s(CORE, strnlen_s(CORE, sizeof(CORE)),
                         param->valuestring, &mismatchcore);
                strcmp_s(DOMAINID, strnlen_s(DOMAINID, sizeof(DOMAINID)),
                         param->valuestring, &mismatchDomainID);
                strcmp_s(PCU_D9_TO_D5,
                         strnlen_s(PCU_D9_TO_D5, sizeof(PCU_D9_TO_D5)),
                         param->valuestring, &mismatchPCU);
                // IO and Compute string comparisons aren't exact; Use substring
                // search
                mismatchIoDomain = strprefix_s(
                    param->valuestring, strnlen_s(IO_DOMAIN, sizeof(IO_DOMAIN)),
                    IO_DOMAIN);
                mismatchComputeDomain = strprefix_s(
                    param->valuestring,
                    strnlen_s(COMPUTE_DOMAIN, sizeof(COMPUTE_DOMAIN)),
                    COMPUTE_DOMAIN);

                if (mismatchtarget == 0)
                {
                    cJSON_AddItemToObject(cmdInOut->paramsTracker, TARGET,
                                          cJSON_CreateNumber(pos));
                    cJSON_ReplaceItemInArray(
                        cmdInOut->in.params, pos,
                        cJSON_CreateNumber(cpuInfo->clientAddr));
                }
                else if (mismatchcha == 0)
                {
                    cJSON_AddItemToObject(cmdInOut->paramsTracker, CHA,
                                          cJSON_CreateNumber(pos));
                    cJSON_ReplaceItemInArray(
                        cmdInOut->in.params, pos,
                        cJSON_CreateNumber(loggerStruct->contextLogger.cha));
                }
                else if (mismatchcorethread == 0)
                {
                    cJSON_AddItemToObject(cmdInOut->paramsTracker, CORETHREAD,
                                          cJSON_CreateNumber(pos));
                    cJSON_ReplaceItemInArray(
                        cmdInOut->in.params, pos,
                        cJSON_CreateNumber(
                            ((loggerStruct->contextLogger.core * 2) +
                             loggerStruct->contextLogger.thread)));
                }
                else if (mismatchcore == 0)
                {
                    cJSON_AddItemToObject(cmdInOut->paramsTracker, CORE,
                                          cJSON_CreateNumber(pos));
                    cJSON_ReplaceItemInArray(
                        cmdInOut->in.params, pos,
                        cJSON_CreateNumber(loggerStruct->contextLogger.core));
                }
                else if (mismatchDomainID == 0)
                {
                    cJSON_AddItemToObject(cmdInOut->paramsTracker, DOMAINID,
                                          cJSON_CreateNumber(pos));
                    cJSON_ReplaceItemInArray(
                        cmdInOut->in.params, pos,
                        cJSON_CreateNumber(
                            loggerStruct->contextLogger.domainID));
                }
                else if (mismatchPCU == 0)
                {
                    cJSON_AddItemToObject(cmdInOut->paramsTracker, PCU_D9_TO_D5,
                                          cJSON_CreateNumber(pos));
                    int device = UpdateGnrPcuDeviceNum(
                        &cpuInfo->dieMaskInfo,
                        loggerStruct->contextLogger.domainID);
                    cJSON_ReplaceItemInArray(cmdInOut->in.params, pos,
                                             cJSON_CreateNumber(device));
                }
                else if (mismatchIoDomain == EOK)
                {
                    uint32_t ioDomain = 255;
                    uint32_t mappedDie = 255;
                    cJSON_AddItemToObject(cmdInOut->paramsTracker,
                                          param->valuestring,
                                          cJSON_CreateNumber(pos));
                    // Parse digits
                    parseExplicitDomain("io", param->valuestring, &ioDomain);
                    mappedDie = mapDieNumToBitGNR(
                        cpuInfo->dieMaskInfo.dieMask, ioDomain,
                        cpuInfo->dieMaskInfo.io.range);
                    if (mappedDie == DIE_MAP_ERROR_VALUE)
                    {
                        // Die doesn't exist!
                        return ACD_FAILURE_UPDATE_PARAMS;
                    }
                    cJSON_ReplaceItemInArray(cmdInOut->in.params, pos,
                                             cJSON_CreateNumber(mappedDie));
                    // Update Logger
                    loggerStruct->contextLogger.io = ioDomain;
                    loggerStruct->contextLogger.domainID = mappedDie;
                }
                else if (mismatchComputeDomain == EOK)
                {
                    uint32_t computeDomain = 255;
                    uint32_t mappedDie = 255;

                    cJSON_AddItemToObject(cmdInOut->paramsTracker,
                                          param->valuestring,
                                          cJSON_CreateNumber(pos));
                    // Parse digits
                    parseExplicitDomain("compute", param->valuestring,
                                        &computeDomain);
                    mappedDie = mapDieNumToBitGNR(
                        cpuInfo->dieMaskInfo.dieMask, computeDomain,
                        cpuInfo->dieMaskInfo.compute.range);
                    if (mappedDie == DIE_MAP_ERROR_VALUE)
                    {
                        // Die doesn't exist!
                        return ACD_FAILURE_UPDATE_PARAMS;
                    }
                    cJSON_ReplaceItemInArray(cmdInOut->in.params, pos,
                                             cJSON_CreateNumber(mappedDie));
                    // Update Logger
                    loggerStruct->contextLogger.compute = computeDomain;
                    loggerStruct->contextLogger.domainID = mappedDie;
                }
                else if (IsValidHexString(param->valuestring))
                {
                    cJSON_ReplaceItemInArray(
                        cmdInOut->in.params, pos,
                        cJSON_CreateNumber(
                            (double)strtoull(param->valuestring, NULL, 16)));
                }
                else
                {
                    cmdInOut->out.stringVal = param->valuestring;
                }
            }
        }
    }
    return ACD_SUCCESS;
}

void ParseSection_FailThreshold(cJSON* section, uint32_t* failThresh)
{
    cJSON* FailThreshold_json =
        cJSON_GetObjectItemCaseSensitive(section->child, "FailThreshold");
    if (FailThreshold_json != NULL)
    {
        *failThresh = FailThreshold_json->valueint;
    }
    else
    {
        // If the key doesn't exist, failThreshold is disabled; 0 = disabled;
        *failThresh = 0;
    }
}
