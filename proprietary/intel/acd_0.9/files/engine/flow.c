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

#include "logger.h"
#include "modules.h"

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
                if (!CHECK_RANGE(repeats, REPEAT_LOWER_BOUND,
                                 REPEAT_UPPER_BOUND))
                {
                    repeats = clampInt(repeats, REPEAT_LOWER_BOUND,
                                       REPEAT_UPPER_BOUND);
                    char sBound[5];
                    cd_snprintf_s(sBound, sizeof(sBound), "%d", repeats);
                    cmdInOut->out.stringVal = sBound;
                    cmdInOut->out.printString = true;
                    if (GenerateJsonPath(cmdInOut, outRoot, loggerStruct,
                                         false) == ACD_SUCCESS)
                    {
                        LogProcessValue(
                            "_repeat_overridden", cmdInOut, loggerStruct,
                            loggerStruct->nameProcessing.jsonOutput);
                    }
                    cmdInOut->out.stringVal = "";
                }
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
            // Reset RmSection
            ResetRemoteModuleStructures(cmdInOut);
            paramParseResult =
                UpdateParams(cpuInfo, cmdInOut, loggerStruct, errInfo);
            if (paramParseResult == ACD_SUCCESS)
            {
                // Only execute on valid params
                for (int n = 1; n <= repeats; n++)
                {
                    sectionExecStatus =
                        checkMaxTimeElapsed(runTimeInfo->maxSectionTime,
                                            runTimeInfo->sectionRunTime);
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
                        LogRegister(cmdInOut, outRoot, loggerStruct);
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
            }
            ResetParams(cmdInOut->in.params, cmdInOut->paramsTracker);
            cJSON_Delete(cmdInOut->paramsTracker);
            errInfo->cmdGroupPos++;
        }
    }
    return failCount;
}
char* generateConsolePrefix(CPUInfo* cpuInfo, PlatformInfo* platformInfo,
                            uint8_t cpu, uint8_t domain, uint8_t logicalDieNum)
{
    static char prefix_str[CPU_DOMAIN_STR_LEN];

    if (cpuInfo->dieMaskInfo.dieMaskSupported)
    {
        // If we aren't looping IO/Compute then just show cpu
        if (platformInfo->noDomainLoop)
        {
            cd_snprintf_s(prefix_str, CPU_DOMAIN_STR_LEN, "[cpu%d]", cpu);
        }
        else if (isDomainCompute(&cpuInfo->dieMaskInfo, domain))
        {
            cd_snprintf_s(prefix_str, CPU_DOMAIN_STR_LEN, "[cpu%d.compute%d]",
                          cpu, logicalDieNum);
        }
        else
        {
            cd_snprintf_s(prefix_str, CPU_DOMAIN_STR_LEN, "[cpu%d.io%d]", cpu,
                          logicalDieNum);
        }
    }
    else
    {
        // SPR-Domains are not valid
        cd_snprintf_s(prefix_str, CPU_DOMAIN_STR_LEN, "[cpu%d]", cpu);
    }

    return prefix_str;
}
acdStatus fillNewSection(cJSON* root, PlatformInfo* platformInfo,
                         CPUInfo* cpuInfo, uint8_t cpu, uint8_t domain,
                         uint8_t logicalDieNum, RunTimeInfo* runTimeInfo,
                         uint8_t sectionIndex, const char* triggerType)
{
#ifdef USE_TWO_BY_ONE_S_CONFIGURATION
    /* For debug */
    /*
    CRASHDUMP_PRINT(INFO, stderr, "Addr: 0x%x SetDevName: %s\n",
                    cpuInfo->clientAddr, cpuInfo->devName);
    */
    peci_SetDevName(cpuInfo->devName);
#endif
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
    memset_s(&loggerStruct, sizeof(LoggerStruct), 0, sizeof(LoggerStruct));
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
    char* logPreFixStr;

    // Remote Section info
    RemoteModuleInfo rmModuleInfo;
    RemoteModuleConfig rmModuleConfig = {.apiVersion = -1, .executionCount = 0};
    RemoteModuleResponse rmModuleResponse = {.nodeLevel[0] = NULL,
                                             .nodeLevel[1] = NULL,
                                             .nodeLevel[2] = NULL,
                                             .nodeLevel[3] = NULL,
                                             .jsonString = NULL};
    RemoteModuleSectionInfo rmSectionInfo = {.isRemoteSection = false,
                                             .instanceLoops = 1,
                                             .modulePath = NULL,
                                             .rmModuleInfo = &rmModuleInfo,
                                             .rmModuleConfig = &rmModuleConfig,
                                             .rmModuleResponse =
                                                 &rmModuleResponse};

    cmdInOut.rmSectionInfo = &rmSectionInfo;
    preReqCmdInOut.rmSectionInfo = &rmSectionInfo;

    cJSON* rmPath =
        cJSON_GetObjectItemCaseSensitive(section->child, "RemoteModulePath");
    if (rmPath != NULL)
    {
        rmSectionInfo.isRemoteSection = true;
        rmSectionInfo.modulePath = rmPath->valuestring;
        cJSON* rmInstanceLoops =
            cJSON_GetObjectItemCaseSensitive(section->child, "LoopOnInstance");
        if (rmInstanceLoops != NULL)
        {
            rmSectionInfo.instanceLoops = rmInstanceLoops->valueint;
        }
    }

    if (section->child == NULL)
    {
        return ACD_FAILURE;
    }

    // Config Logger for KEY_VALUE
    LogConfig(&loggerStruct, KEY_VALUE);

    // Generate prefix for output messages
    logPreFixStr = generateConsolePrefix(cpuInfo, platformInfo, cpu, domain,
                                         logicalDieNum);
    cJSON* sectionEnable =
        cJSON_GetObjectItemCaseSensitive(section->child, "RecordEnable");

    if (sectionEnable != NULL)
    {
        if (!cJSON_IsTrue(sectionEnable))
        {
            if (getPath(section->child, &loggerStruct) == ACD_SUCCESS)
            {
                loggerStruct.contextLogger.cpu = cpu;
                loggerStruct.contextLogger.compute = logicalDieNum;
                loggerStruct.contextLogger.io = logicalDieNum;
                loggerStruct.contextLogger.isDomainCompute =
                    isDomainCompute(&cpuInfo->dieMaskInfo, domain);
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

    if (isCrashlogFlow(triggerType))
    {
        TriggerString split = {{0}, {0}};
        splitTriggerString(triggerType, &split);
        char flag[MAX_TRIGGER_STR_LEN];
        concatStrings(flag, sizeof(flag), split.modifier, "Data");
        cJSON* obj = cJSON_GetObjectItem(section->child, flag);
        if (obj == NULL || cJSON_IsFalse(obj))
        {
            CRASHDUMP_PRINT(ERR, stderr, "%s Skipping %s\n", logPreFixStr,
                            section->child->string);
            return ACD_SUCCESS;
        }
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

    CRASHDUMP_PRINT(ERR, stderr, "%s Logging %s \n", logPreFixStr,
                    section->child->string);

    loggerStruct.contextLogger.currentSectionName = section->child->string;
    loggerStruct.contextLogger.cpu = cpu;
    loggerStruct.contextLogger.compute = logicalDieNum;
    loggerStruct.contextLogger.isDomainCompute =
        isDomainCompute(&cpuInfo->dieMaskInfo, domain);
    loggerStruct.contextLogger.io = logicalDieNum;
    uint64_t effectiveCoreMask = 0;
    size_t effectiveChaCount = 0;
    uint64_t effectiveCrashCoreMask = 0;
    if (cpuInfo->dieMaskInfo.dieMaskSupported)
    {
        // Check to see this is a compute-die
        if CHECK_BIT (cpuInfo->dieMaskInfo.compute.range, domain)
        {
            effectiveCoreMask = cpuInfo->computeDies[logicalDieNum].coreMask;
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

    cJSON* recordType =
        cJSON_GetObjectItemCaseSensitive(section->child, "RecordType");

    loggerStruct.contextLogger.skipFlag = false;
    preReqCmdInOut.internalVarsTracker = cJSON_CreateObject();
    ProcessPECICmds(&entry, cpuInfo, preReq, &preReqCmdInOut, &errInfo,
                    &loggerStruct, root, runTimeInfo);
    cmdInOut.internalVarsTracker = preReqCmdInOut.internalVarsTracker;
    if (loopOnFlags.loopOnCHA)
    {
        if (cpuInfo->dieMaskInfo.dieMaskSupported)
        {
            // Total of 60 instances of CHAs in domain 9 if available
            int domain9CHALow = 0;
            int domain9CHAHigh = 59;

            // Total of 60 instances of CHAs in domain 10 if available
            int domain10CHALow = 0;
            int domain10CHAHigh = 59;

            // Total of 8 instances of CHAs in domain 11 if available
            int domain11CHALow = 0;
            int domain11CHAHigh = 7;
            bool skipDomain = false;

            getRangeValues(section->child, "Domain9CHA", &domain9CHALow,
                           &domain9CHAHigh);
            getRangeValues(section->child, "Domain10CHA", &domain10CHALow,
                           &domain10CHAHigh);
            getRangeValues(section->child, "Domain11CHA", &domain11CHALow,
                           &domain11CHAHigh);
            int domain9CHACount = domain9CHAHigh - domain9CHALow + 1;
            int domain10CHACount = domain10CHAHigh - domain10CHALow + 1;

            uint8_t startInstanceNum = 0;
            uint8_t endInstanceNum = 0;
            uint8_t chaOffset = 0;

            if (domain == 9)
            {
                startInstanceNum = domain9CHALow;
                endInstanceNum = domain9CHAHigh;
                chaOffset = 0;
                if (cpuInfo->chaCount < domain9CHACount)
                {
                    endInstanceNum = cpuInfo->chaCount - 1;
                }
            }

            if (domain == 10 && cpuInfo->chaCount > domain9CHACount)
            {
                startInstanceNum = domain10CHALow;
                endInstanceNum = domain10CHAHigh;
                chaOffset = domain9CHACount;
                if (cpuInfo->chaCount - domain9CHACount < domain9CHACount)
                {
                    endInstanceNum = cpuInfo->chaCount - (domain9CHACount + 1);
                }
            }
            else if (domain == 10 && cpuInfo->chaCount <= domain9CHACount)
            {
                skipDomain = true;
            }

            if (domain == 11 &&
                cpuInfo->chaCount > (domain9CHACount + domain10CHACount))
            {
                startInstanceNum = domain11CHALow;
                endInstanceNum = cpuInfo->chaCount -
                                 (domain9CHACount + domain10CHACount + 1);
                chaOffset = domain9CHACount + domain10CHACount;
            }
            else if (domain == 11 &&
                     cpuInfo->chaCount <= (domain9CHACount + domain10CHACount))
            {
                skipDomain = true;
            }

            if (!skipDomain)
            {
                for (size_t cha = startInstanceNum; cha <= endInstanceNum;
                     cha++)
                {
                    loggerStruct.contextLogger.cha = (uint8_t)cha;
                    loggerStruct.contextLogger.chaOffset = (uint8_t)chaOffset;
                    failCount += ProcessPECICmds(
                        &entry, cpuInfo, peciCmds, &cmdInOut, &errInfo,
                        &loggerStruct, root, runTimeInfo);
                    loggerStruct.contextLogger.skipFlag = false;
                }
            }
        }
        else
        {
            for (size_t cha = 0; cha < effectiveChaCount; cha++)
            {
                loggerStruct.contextLogger.cha = (uint8_t)cha;
                loggerStruct.contextLogger.chaOffset = 0;
                failCount +=
                    ProcessPECICmds(&entry, cpuInfo, peciCmds, &cmdInOut,
                                    &errInfo, &loggerStruct, root, runTimeInfo);
                loggerStruct.contextLogger.skipFlag = false;
            }
        }
    }
    else if (loopOnFlags.loopOnModule)
    {
        if (cpuInfo->coreType == ACD_ATOM_CORE)
        {
            uint8_t coreIdx = 0;
            for (uint8_t u8ModuleNum = 0; u8ModuleNum < MAX_MODULE_MASK;
                 u8ModuleNum++)
            {
                if (!CHECK_BIT(effectiveCoreMask, u8ModuleNum))
                {
                    continue;
                }
                loggerStruct.contextLogger.module = u8ModuleNum;
                for (uint8_t u8CoreNum = 0; u8CoreNum < MAX_CORE_PER_MODULE;
                     u8CoreNum++)
                {
                    loggerStruct.contextLogger.core = u8CoreNum;
                    loggerStruct.contextLogger.logicalCore = coreIdx;
                    failCount += ProcessPECICmds(
                        &entry, cpuInfo, peciCmds, &cmdInOut, &errInfo,
                        &loggerStruct, root, runTimeInfo);
                    coreIdx++;
                }
                loggerStruct.contextLogger.skipFlag = false;
            }
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
            /*
                Both of these variables need to be set as a side effect of
                supporting both SRF/SPR:
                core is used for OutputPath creation.
                logicalCore is used for "Core" peci parameter.
            */
            loggerStruct.contextLogger.core = u8CoreNum;
            loggerStruct.contextLogger.logicalCore = u8CoreNum;
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

        bool sectionNotBigOrAtomCore = isSectionNotBigOrAtomCore(recordType);
        if (sectionNotBigOrAtomCore)
        {
            logSectionRunTime(loggerStruct.nameProcessing.jsonOutput,
                              &runTimeInfo->sectionRunTime, sectionTimeString);
        }
        else
        {
            cJSON* bigOrAtomCore = getFirstBigOrAtomCoreFromSocket(
                cpuInfo, loggerStruct.nameProcessing.jsonOutput);
            if (bigOrAtomCore == NULL)
            {
                CRASHDUMP_PRINT(ERR, stderr,
                                "Couldn't log big_core or atom_core _time\n");
            }
            else
            {
                logSectionRunTime(bigOrAtomCore, &runTimeInfo->sectionRunTime,
                                  sectionTimeString);
            }
        }
    }
    // Log section failCount if failthreshold is enabled
    if (failThreshold)
    {
        logSectionFailCount(loggerStruct.nameProcessing.jsonOutput, failCount,
                            section->child->string);
    }
    cJSON_Delete(cmdInOut.internalVarsTracker);
    hdestroy();

    // Unload Remote Module object
    if (rmSectionInfo.isRemoteSection)
    {
        unloadModule(rmSectionInfo.moduleHandle);
    }

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

void ResetRemoteModuleStructures(CmdInOut* cmdInOut)
{
    // Reset input parameters
    resetModuleInfo(cmdInOut->rmSectionInfo->rmModuleInfo);
    // Reset index of "Instance"
    cmdInOut->rmSectionInfo->instanceArgumentIndex = INVALID_VALUE;
}
