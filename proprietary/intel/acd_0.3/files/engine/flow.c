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

#include "flow.h"

acdStatus ProcessPECICmds(ENTRY* entry, CPUInfo* cpuInfo, cJSON* peciCmds,
                          CmdInOut* cmdInOut, InputParserErrInfo* errInfo,
                          LoggerStruct* loggerStruct, cJSON* outRoot,
                          RunTimeInfo* runTimeInfo)
{
    cJSON* cmdGroup = NULL;
    int failCount = 0;
    acdStatus paramParseResult = ACD_SUCCESS;

    cJSON_ArrayForEach(cmdGroup, peciCmds)
    {
        if (cmdGroup->child == NULL)
        {
            continue;
        }
        entry->key = cmdGroup->child->string;
        errInfo->cmdGroup = entry->key;
        errInfo->cmdGroupPos = 0;
        executionStatus sectionExecStatus, globalExecStatus;
        cJSON* paramsGroup =
            cJSON_GetObjectItemCaseSensitive(cmdGroup, cmdGroup->child->string);
        cJSON* params = NULL;
        cJSON_ArrayForEach(params, paramsGroup)
        {
            if (cmdGroup->child == NULL)
            {
                continue;
            }
            int repeats = 1;
            loggerStruct->nameProcessing.logRegister = true;
            cmdInOut->in.params =
                cJSON_GetObjectItemCaseSensitive(params, "Params");
            cmdInOut->in.outputPath =
                cJSON_GetObjectItemCaseSensitive(params, "Output");
            ParseNameSection(cmdInOut, loggerStruct);
            cmdInOut->in.repeats =
                cJSON_GetObjectItemCaseSensitive(params, "Repeat");
            if (cmdInOut->in.repeats != NULL)
            {
                repeats = cmdInOut->in.repeats->valueint;
            }
            cmdInOut->internalVarName = NULL;
            cJSON* var =
                cJSON_GetObjectItemCaseSensitive(params, "InternalVar");
            if (var != NULL)
            {
                cmdInOut->internalVarName = var->valuestring;
            }
            cmdInOut->out.ret = PECI_CC_INVALID_REQ;
            cmdInOut->out.printString = false;
            cmdInOut->paramsTracker = cJSON_CreateObject();
            paramParseResult =
                UpdateParams(cpuInfo, cmdInOut, loggerStruct, errInfo);
            if (paramParseResult != ACD_SUCCESS)
            {
                // Do not execute a failure
                continue;
            }
            for (int n = 1; n <= repeats; n++)
            {
                sectionExecStatus = checkMaxTimeElapsed(
                    runTimeInfo->maxSectionTime, runTimeInfo->sectionRunTime);
                globalExecStatus = checkMaxTimeElapsed(
                    runTimeInfo->maxGlobalTime, runTimeInfo->globalRunTime);
                cmdInOut->runTimeInfo = runTimeInfo;
                if (globalExecStatus == EXECUTION_ABORTED)
                {
                    loggerStruct->contextLogger.skipFlag = true;
                    cmdInOut->runTimeInfo->maxGlobalRunTimeReached = true;
                }
                if (sectionExecStatus == EXECUTION_ABORTED)
                {
                    loggerStruct->contextLogger.skipFlag = true;
                    cmdInOut->runTimeInfo->maxSectionRunTimeReached = true;
                }
                if (!loggerStruct->contextLogger.skipFlag)
                {
                    acdStatus status = Execute(entry, cmdInOut, cpuInfo);
                    if (status != ACD_SUCCESS)
                    {
                        failCount++;
                    }
                }
                if (loggerStruct->nameProcessing.logRegister)
                {
                    loggerStruct->contextLogger.repeats = n;
                    Logger(cmdInOut, outRoot, loggerStruct);
                }
                if (cmdInOut->out.ret != PECI_CC_SUCCESS ||
                    PECI_CC_UA(cmdInOut->out.cc))
                {
                    if (loggerStruct->contextLogger.skipOnFailFromInputFile)
                    {
                        loggerStruct->contextLogger.skipFlag = true;
                    }
                }
            }
            ResetParams(cmdInOut->in.params, cmdInOut->paramsTracker);
            cJSON_Delete(cmdInOut->paramsTracker);
            errInfo->cmdGroupPos++;
        }
    }
    return failCount;
}

acdStatus fillNewSection(cJSON* root, PlatformInfo* platformInfo,
                         CPUInfo* cpuInfo, uint8_t cpu, uint8_t domain,
                         uint8_t logicalDieNum, RunTimeInfo* runTimeInfo,
                         uint8_t sectionIndex, bool enablePostReset)
{
    bool validateInput = readInputFileFlag(cpuInfo[0].inputFile.bufferPtr, true,
                                           VALIDATE_ENABLE_KEY);
    cJSON* sections = cJSON_GetObjectItemCaseSensitive(
        cpuInfo[0].inputFile.bufferPtr, "Sections");
    cJSON* section = NULL;
    uint32_t failCount = 0;
    uint32_t failThreshold = 0;
    runTimeInfo->maxGlobalRunTimeReached = false;
    section = cJSON_GetArrayItem(sections, sectionIndex);
    if (section == NULL)
    {
        return ACD_SUCCESS;
    }
    ENTRY entry;
    LoggerStruct loggerStruct;
    BuildCmdsTable(&entry);
    uint8_t threadsPerCore = 1;
    runTimeInfo->maxSectionTime = 0xFFFFFFFF;
    runTimeInfo->maxSectionRunTimeReached = false;
    runTimeInfo->sectionMaxPrint = true;
    runTimeInfo->globalMaxPrint = true;
    loggerStruct.contextLogger.skipOnFailFromInputFile = false;
    InputParserErrInfo errInfo = {};
    CmdInOut cmdInOut;
    ValidatorParams validParams;
    validParams.validateInput = validateInput;
    cmdInOut.validatorParams = &validParams;
    LoopOnFlags loopOnFlags;
    cmdInOut.platformInfo = platformInfo;
    cmdInOut.root = root;
    cmdInOut.logger = &loggerStruct;
    cmdInOut.out.ret = PECI_CC_INVALID_REQ;
    cmdInOut.out.size = 0;
    cmdInOut.out.stringVal = "";
    RunTimeInfo emptyRunTimeInfo = {.maxSectionTime = 0xFFFFFFFF,
                                    .maxGlobalRunTimeReached = false,
                                    .maxSectionRunTimeReached = false,
                                    .sectionMaxPrint = true,
                                    .globalMaxPrint = true};
    cmdInOut.runTimeInfo = &emptyRunTimeInfo;
    CmdInOut preReqCmdInOut;
    preReqCmdInOut.validatorParams = cmdInOut.validatorParams;
    preReqCmdInOut.platformInfo = platformInfo;
    preReqCmdInOut.out.ret = PECI_CC_INVALID_REQ;
    preReqCmdInOut.logger = &loggerStruct;
    acdStatus status = ACD_SUCCESS;

    if (section->child == NULL)
    {
        return ACD_FAILURE;
    }

    cJSON* sectionEnable =
        cJSON_GetObjectItemCaseSensitive(section->child, "RecordEnable");

    if (enablePostReset)
    {
        // Only fill section if "PostResetData" is enabled
        cJSON* obj = cJSON_GetObjectItem(section->child, POST_RESET_DATA);
        if (obj == NULL || cJSON_IsFalse(obj))
        {
            return ACD_SUCCESS;
        }
    }

    if (sectionEnable != NULL)
    {
        if (!cJSON_IsTrue(sectionEnable))
        {
            if (getPath(section->child, &loggerStruct) == ACD_SUCCESS)
            {
                loggerStruct.contextLogger.cpu = cpu;
                loggerStruct.contextLogger.compute = logicalDieNum;
                loggerStruct.contextLogger.io = logicalDieNum;
                ParsePath(&loggerStruct);
                logRecordDisabled(&cmdInOut, root, &loggerStruct);
            }
            return ACD_SUCCESS;
        }
    }
    else
    {
        return ACD_SUCCESS;
    }

    ReadLoops(section->child, &loopOnFlags);
    loggerStruct.contextLogger.domainID = domain;
    GenerateVersion(section->child, &loggerStruct.contextLogger.version);
    getPath(section->child, &loggerStruct);
    ParsePath(&loggerStruct);
    cJSON* maxTimeJson =
        cJSON_GetObjectItemCaseSensitive(section->child, "MaxTimeSec");
    if (maxTimeJson != NULL)
    {
        runTimeInfo->maxSectionTime = maxTimeJson->valueint;
    }
    cJSON* skipOnErrorJson =
        cJSON_GetObjectItemCaseSensitive(section->child, "SkipOnFail");
    if (skipOnErrorJson != NULL)
    {
        loggerStruct.contextLogger.skipOnFailFromInputFile =
            cJSON_IsTrue(skipOnErrorJson);
    }
    loggerStruct.contextLogger.skipCrashCores = false;
    cJSON* skipCrashedCores =
        cJSON_GetObjectItemCaseSensitive(section->child, "SkipCrashedCores");
    if (skipCrashedCores != NULL)
    {
        loggerStruct.contextLogger.skipCrashCores =
            cJSON_IsTrue(skipCrashedCores);
    }
    cJSON* preReq = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(section->child, "PreReq"), "Commands");
    cJSON* peciCmds =
        cJSON_GetObjectItemCaseSensitive(section->child, "Commands");

    ParseSection_FailThreshold(section, &failThreshold);

    CRASHDUMP_PRINT(ERR, stderr, "Logging %s on PECI address %d domain %d\n",
                    section->child->string, cpuInfo->clientAddr, domain);
    loggerStruct.contextLogger.currentSectionName = section->child->string;
    loggerStruct.contextLogger.cpu = cpu;
    loggerStruct.contextLogger.compute = logicalDieNum;
    loggerStruct.contextLogger.io = logicalDieNum;
    uint64_t effectiveCoreMask = 0;
    uint64_t effectiveChaMask = 0;
    size_t effectiveChaCount = 0;
    uint64_t effectiveCrashCoreMask = 0;
    if (cpuInfo->dieMaskInfo.dieMaskSupported)
    {
        // Check to see this is a compute-die
        if CHECK_BIT (cpuInfo->dieMaskInfo.compute.range, domain)
        {
            effectiveCoreMask = cpuInfo->computeDies[logicalDieNum].coreMask;
            effectiveChaMask = cpuInfo->computeDies[logicalDieNum].chaMask;
            effectiveChaCount = cpuInfo->computeDies[logicalDieNum].chaCount;
            effectiveCrashCoreMask =
                cpuInfo->computeDies[logicalDieNum].crashedCoreMask;
        }
    }
    else
    {
        effectiveCoreMask = cpuInfo->coreMask;
        effectiveChaCount = cpuInfo->chaCount;
        effectiveCrashCoreMask = cpuInfo->crashedCoreMask;
    }
    logVersion(&cmdInOut, root, &loggerStruct);
    loggerStruct.contextLogger.skipFlag = false;
    preReqCmdInOut.internalVarsTracker = cJSON_CreateObject();
    ProcessPECICmds(&entry, cpuInfo, preReq, &preReqCmdInOut, &errInfo,
                    &loggerStruct, root, runTimeInfo);
    cmdInOut.internalVarsTracker = preReqCmdInOut.internalVarsTracker;
    if (loopOnFlags.loopOnCHA)
    {
        for (size_t cha = 0; cha < effectiveChaCount; cha++)
        {
            loggerStruct.contextLogger.cha = (uint8_t)cha;
            failCount +=
                ProcessPECICmds(&entry, cpuInfo, peciCmds, &cmdInOut, &errInfo,
                                &loggerStruct, root, runTimeInfo);
            loggerStruct.contextLogger.skipFlag = false;
        }
    }
    else if (loopOnFlags.loopOnCore)
    {
        for (uint8_t u8CoreNum = 0; u8CoreNum < MAX_CORE_MASK; u8CoreNum++)
        {
            if (!CHECK_BIT(effectiveCoreMask, u8CoreNum))
            {
                continue;
            }
            if (loggerStruct.contextLogger.skipCrashCores &&
                CHECK_BIT(effectiveCrashCoreMask, u8CoreNum))
            {
                continue;
            }
            loggerStruct.contextLogger.core = u8CoreNum;
            if (loopOnFlags.loopOnThread)
            {
                threadsPerCore = 2;
            }
            for (uint8_t threadNum = 0; threadNum < threadsPerCore; threadNum++)
            {
                loggerStruct.contextLogger.thread = threadNum;
                failCount +=
                    ProcessPECICmds(&entry, cpuInfo, peciCmds, &cmdInOut,
                                    &errInfo, &loggerStruct, root, runTimeInfo);
            }
            loggerStruct.contextLogger.skipFlag = false;
        }
    }
    else
    {
        if (!cmdInOut.platformInfo->loopOverOneCpu)
        {
            failCount +=
                ProcessPECICmds(&entry, cpuInfo, peciCmds, &cmdInOut, &errInfo,
                                &loggerStruct, root, runTimeInfo);
            loggerStruct.contextLogger.skipFlag = false;
        }
        if (!loopOnFlags.loopOnCPU &&
            cmdInOut.platformInfo->loopOverOneCpu == false)
        {
            cmdInOut.platformInfo->loopOverOneCpu = true;
        }
    }
    loggerStruct.nameProcessing.extraLevel = false;
    if (GenerateJsonPath(&cmdInOut, root, &loggerStruct, true) == ACD_SUCCESS)
    {
        char sectionTimeString[64];
        cd_snprintf_s(sectionTimeString, sizeof(sectionTimeString), "_time_%s",
                      loggerStruct.contextLogger.currentSectionName);

        logSectionRunTime(loggerStruct.nameProcessing.jsonOutput,
                          &runTimeInfo->sectionRunTime, sectionTimeString);
    }
    // Log section failCount if failthreshold is enabled
    if (failThreshold)
    {
        logSectionFailCount(loggerStruct.nameProcessing.jsonOutput, failCount,
                            section->child->string);
    }

    cJSON_Delete(cmdInOut.internalVarsTracker);
    hdestroy();
    // Section failure check; if FailThreshold is enabled (!0)
    if (failThreshold && (failCount >= failThreshold))
    {
        return ACD_FAILURE;
    }
    else
    {
        return ACD_SUCCESS;
    }
}
