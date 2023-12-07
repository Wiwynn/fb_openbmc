/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2019 Intel Corporation.
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
#ifndef BUILD_MAIN
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#ifndef IPMB_PECI_INTF
#include <peci.h>
#endif
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef NO_SYSTEMD
#include <systemd/sd-id128.h>
#endif
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <future>
#include <regex>
#include <sstream>
#include <vector>
#ifdef NO_SYSTEMD
#include <CLI/CLI.hpp>
#endif

extern "C" {
#include <cjson/cJSON.h>

#include "engine/BigCore.h"
#include "engine/TorDump.h"
#include "engine/cmdprocessor.h"
#include "engine/crashdump.h"
#include "engine/flow.h"
#include "engine/inputparser.h"
#include "engine/logger.h"
#include "engine/validator.h"
#include "safe_mem_lib.h"
#include "safe_str_lib.h"
}

#include "crashdump.hpp"
#include "utils_triage.hpp"

#ifdef NO_SYSTEMD
extern void getSystemGuid(std::string &);
extern void platformInit(uint8_t);
#endif

#ifdef PMT_NAC_CRASHLOG
#include "pmt_crashlog.hpp"
#endif

std::string crashdumpFruName("");

namespace crashdump
{
static CPUInfo cpus[MAX_CPUS];
#ifndef NO_SYSTEMD
static std::shared_ptr<sdbusplus::asio::dbus_interface> logIface;
#endif

constexpr char const* triggerTypeOnDemand = "On-Demand";
constexpr int const vcuPeciWake = 5;

static PlatformState platformState = {false, DEFAULT_VALUE, DEFAULT_VALUE,
                                      DEFAULT_VALUE, DEFAULT_VALUE};
int sectionFailCount = 0;
static constexpr const int peciCheckInterval = 10;

static const std::string getUuid()
{
    std::string ret;
#ifdef NO_SYSTEMD
    getSystemGuid(ret);
#else
    sd_id128_t appId = SD_ID128_MAKE(e0, e1, 73, 76, 64, 61, 47, da, a5, 0c, d0,
                                     cc, 64, 12, 45, 78);
    sd_id128_t machineId = SD_ID128_NULL;

    if (sd_id128_get_machine_app_specific(appId, &machineId) == 0)
    {
        std::array<char, SD_ID128_STRING_MAX> str;
        ret = sd_id128_to_string(machineId, str.data());
        ret.insert(8, 1, '-');
        ret.insert(13, 1, '-');
        ret.insert(18, 1, '-');
        ret.insert(23, 1, '-');
    }
#endif

    return ret;
}

void getClientAddrs(CPUInfo* cpus)
{
    for (int cpu = 0, addr = MIN_CLIENT_ADDR; addr <= MAX_CLIENT_ADDR;
         cpu++, addr++)
    {
        if (peci_Ping(addr) == PECI_CC_SUCCESS)
        {
            cpus[cpu].clientAddr = addr;
        }
    }
}

static bool savePeciWake(CPUInfo* cpus)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;
    uint32_t peciRdValue = 0;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        retval =
            peci_RdPkgConfig(cpu->clientAddr, vcuPeciWake, ON, sizeof(uint32_t),
                             (uint8_t*)&peciRdValue, &cc);
        if (retval != PECI_CC_SUCCESS)
        {
            CRASHDUMP_PRINT(
                ERR, stderr,
                "Cannot read Wake_on_PECI-> addr: (0x%x), ret: (0x%x), \
                        cc: (0x%x)\n",
                cpu->clientAddr, retval, cc);
            cpu->initialPeciWake = UNKNOWN;
            continue;
        }
        cpu->initialPeciWake = (peciRdValue & 0x1) ? ON : OFF;
    }
    return true;
}

static bool setPeciWake(CPUInfo* cpus, pwState desiredState)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;
    int writeValue = OFF;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        if ((cpu->initialPeciWake == ON) || (cpu->initialPeciWake == UNKNOWN))
            continue;
        writeValue = static_cast<int>(desiredState);
        retval = peci_WrPkgConfig(cpu->clientAddr, vcuPeciWake, writeValue,
                                  writeValue, sizeof(writeValue), &cc);
        if (retval != PECI_CC_SUCCESS)
        {
            CRASHDUMP_PRINT(
                ERR, stderr,
                "Cannot set Wake_on_PECI-> addr: (0x%x), ret: (0x%x), cc: "
                "(0x%x)\n",
                cpu->clientAddr, retval, cc);
        }
    }
    return true;
}

static bool checkPeciWake(CPUInfo* cpus)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;
    uint32_t peciRdValue = 0;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        retval =
            peci_RdPkgConfig(cpu->clientAddr, vcuPeciWake, ON, sizeof(uint32_t),
                             (uint8_t*)&peciRdValue, &cc);
        if (retval != PECI_CC_SUCCESS)
        {
            CRASHDUMP_PRINT(
                ERR, stderr,
                "Cannot read Wake_on_PECI-> addr: (0x%x), ret: (0x%x), \
                        cc: (0x%x)\n",
                cpu->clientAddr, retval, cc);
            continue;
        }
        if (peciRdValue != 1)
        {
            CRASHDUMP_PRINT(ERR, stderr, "Wake_on_PECI in OFF state: (0x%x)\n",
                            cpu->clientAddr);
        }
    }
    return true;
}

void setResetDetected()
{
    if (!platformState.resetDetected)
    {
        platformState.resetDetected = true;
        platformState.resetCpu = platformState.currentCpu;
        platformState.resetSection = platformState.currentSection;
    }
}

inline void clearResetDetected()
{
    platformState.resetDetected = false;
    platformState.resetCpu = DEFAULT_VALUE;
    platformState.resetSection = DEFAULT_VALUE;
    platformState.currentSection = DEFAULT_VALUE;
    platformState.currentCpu = DEFAULT_VALUE;
}

inline void updateCurrentSection(uint8_t section, uint8_t cpu)
{
    platformState.currentSection = section;
    platformState.currentCpu = cpu;
}

acdStatus loadInputFiles(CPUInfo* cpus, InputFileInfo* inputFileInfo,
                         bool isTelemetry)
{
    int uniqueCount = 0;
    cJSON* defaultStateSection = NULL;
    acdStatus status = ACD_SUCCESS;

    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if ((cpu->clientAddr == 0) || (cpu->model >= cd_numberOfModels))
        {
            continue;
        }
        // read and allocate memory for crashdump input file
        // if it hasn't been read before
        if (inputFileInfo->buffers[cpu->model] == NULL)
        {
            inputFileInfo->buffers[cpu->model] = selectAndReadInputFile(
                cpu->model, &inputFileInfo->filenames[cpu->model], isTelemetry);
            if (inputFileInfo->buffers[cpu->model] != NULL)
            {
                uniqueCount++;
            }
        }

        inputFileInfo->unique = (uniqueCount <= 1);
        cpu->inputFile.filenamePtr = inputFileInfo->filenames[cpu->model];
        cpu->inputFile.bufferPtr = inputFileInfo->buffers[cpu->model];

        // Get and Check global enable/disable value from "DefaultState"
        defaultStateSection = cJSON_GetObjectItemCaseSensitive(
            inputFileInfo->buffers[cpu->model], "DefaultState");
        int defaultStateEnable = 1;
        if (defaultStateSection != NULL)
        {
            strcmp_s(defaultStateSection->valuestring, CRASHDUMP_VALUE_LEN,
                     "Enable", &defaultStateEnable);
            if (defaultStateEnable == 0)
            {
                status = ACD_SUCCESS;
            }
            else
            {
                defaultStateEnable = 1;
                strcmp_s(defaultStateSection->valuestring, CRASHDUMP_VALUE_LEN,
                         "Disable", &defaultStateEnable);
                if (defaultStateEnable == 0)
                {
                    status = ACD_INPUT_FILE_ERROR;
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Exiting, \"DefaultState\" set to Disable in: %s\n",
                        inputFileInfo->filenames[cpu->model]);
                }
                else
                {
                    status = ACD_SUCCESS;
                    CRASHDUMP_PRINT(ERR, stderr,
                                    "\"DefaultState\" (%s) is neither "
                                    "Enable/Disable in: %s\n",
                                    defaultStateSection->valuestring,
                                    inputFileInfo->filenames[cpu->model]);
                }
            }
        }
        else
        {
            status = ACD_SUCCESS;
        }
    }

    return status;
}

static void getCPUID(CPUInfo* cpuInfo)
{
    uint8_t cc = 0;
    CPUModel cpuModel{};
    uint8_t stepping = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;

    retval = peci_GetCPUID(cpuInfo->clientAddr, &cpuModel, &stepping, &cc);
    cpuInfo->cpuidRead.cpuModel = cpuModel;
    cpuInfo->cpuidRead.stepping = stepping;
    cpuInfo->cpuidRead.cpuidCc = cc;
    cpuInfo->cpuidRead.cpuidRet = retval;
    if (retval != PECI_CC_SUCCESS || PECI_CC_UA(cc))
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "Cannot get CPUID! ret: (0x%x), cc: (0x%x) on PECI address %d\n",
            retval, cc, cpuInfo->clientAddr);
    }
}

static void parseCPUInfo(CPUInfo* cpu, cpuidState cpuState)
{
    switch ((int)cpu->cpuidRead.cpuModel)
    {
        case icx:
            CRASHDUMP_PRINT(INFO, stderr,
                            "ICX detected (CPUID 0x%x) on PECI address %d\n",
                            cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                            cpu->clientAddr);
            if (cpu->cpuidRead.stepping >= STEPPING_ICX2)
            {
                cpu->model = cd_icx2;
            }
            else
            {
                cpu->model = cd_icx;
            }
            cpu->cpuidRead.cpuidValid = true;
            cpu->cpuidRead.source = cpuState;
            break;
        case EMR_MODEL:
            CRASHDUMP_PRINT(INFO, stderr,
                            "EMR detected (CPUID 0x%x) on PECI address %d\n",
                            cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                            cpu->clientAddr);
            cpu->model = cd_emr;
            cpu->cpuidRead.cpuidValid = true;
            cpu->cpuidRead.source = cpuState;
            break;
        case SPR_MODEL:
            if (isSprHbm(cpu))
            {
                CRASHDUMP_PRINT(
                    INFO, stderr,
                    "SPR-HBM detected (CPUID 0x%x) on PECI address %d\n",
                    cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                    cpu->clientAddr);
                cpu->model = cd_sprhbm;
            }
            else
            {
                CRASHDUMP_PRINT(
                    INFO, stderr,
                    "SPR detected (CPUID 0x%x) on PECI address %d\n",
                    cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                    cpu->clientAddr);
                cpu->model = cd_spr;
            }
            cpu->cpuidRead.cpuidValid = true;
            cpu->cpuidRead.source = cpuState;
            break;
        case ICXD_MODEL:
            CRASHDUMP_PRINT(INFO, stderr,
                            "ICXD detected (CPUID 0x%x) on PECI address %d\n",
                            cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                            cpu->clientAddr);
            cpu->model = cd_icxd;
            cpu->cpuidRead.cpuidValid = true;
            cpu->cpuidRead.source = cpuState;
            break;
        case GNR_MODEL:
            CRASHDUMP_PRINT(INFO, stderr,
                            "GNR detected (CPUID 0x%x) on PECI address %d\n",
                            cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                            cpu->clientAddr);
            cpu->model = cd_gnr;
            cpu->cpuidRead.cpuidValid = true;
            cpu->cpuidRead.source = cpuState;
            break;
        case SRF_MODEL:
            CRASHDUMP_PRINT(INFO, stderr,
                            "SRF detected (CPUID 0x%x) on PECI address %d\n",
                            cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                            cpu->clientAddr);
            cpu->model = cd_srf;
            cpu->cpuidRead.cpuidValid = true;
            cpu->cpuidRead.source = cpuState;
            break;
        default:
            CRASHDUMP_PRINT(ERR, stderr,
                            "Unsupported CPUID 0x%x on PECI address %d using "
                            "gnr as default model\n",
                            cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                            cpu->clientAddr);
            cpu->model = cd_gnr;
            cpu->cpuidRead.cpuidValid = true;
            cpu->cpuidRead.source = cpuState;
            break;
    }
}

static void overwriteCPUInfo(CPUInfo* cpus)
{
    bool found = false;
    Model defaultModel = (Model)0x0;
    CPUModel defaultCpuModel = (CPUModel)0x0;
    uint8_t defaultStepping = 0x0;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr != 0x0 && cpu->cpuidRead.cpuidValid)
        {
            defaultModel = cpu->model;
            defaultCpuModel = cpu->cpuidRead.cpuModel;
            defaultStepping = cpu->cpuidRead.stepping;
            found = true;
            break;
        }
    }
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr != 0x0 && !cpu->cpuidRead.cpuidValid)
        {
            if (found)
            {
                CRASHDUMP_PRINT(
                    INFO, stderr,
                    "Overwriting model for CPU: %d on PECI address %d\n", i,
                    cpu->clientAddr);
                cpu->model = defaultModel;
                cpu->cpuidRead.cpuModel = defaultCpuModel;
                cpu->cpuidRead.stepping = defaultStepping;
                cpu->cpuidRead.source = OVERWRITTEN;
                cpu->cpuidRead.cpuidCc = PECI_DEV_CC_SUCCESS;
                cpu->cpuidRead.cpuidRet = PECI_CC_SUCCESS;
                cpu->cpuidRead.cpuidValid = true;
            }
            else
            {
                cpu->cpuidRead.source = INVALID;
            }
        }
    }
}

static bool isCpusInfoEmpty(CPUInfo* cpus)
{
    for (int i = 0; i < MAX_CPUS; i++)
    {
        if (cpus[i].clientAddr != 0x0)
        {
            return false;
        }
    }
    return true;
}

void initCPUInfo(CPUInfo* cpus)
{
    if (isCpusInfoEmpty(cpus))
    {
        getClientAddrs(cpus);
        for (int i = 0; i < MAX_CPUS; i++)
        {
            cpus[i].coreMaskRead.coreMaskValid = false;
            cpus[i].chaCountRead.chaCountValid = false;
            cpus[i].cpuidRead.cpuidValid = false;
            cpus[i].coreMaskRead.source = INVALID;
            cpus[i].chaCountRead.source = INVALID;
            cpus[i].cpuidRead.source = INVALID;
            cpus[i].model = (Model)0x0;
            cpus[i].cpuidRead.cpuModel = (CPUModel)0x0;

            // Do not reset DieMask maskValid and dieMask
            cpus[i].dieMaskInfo.dieMaskSupported = false;
            cpus[i].dieMaskInfo.compute = {};
            cpus[i].dieMaskInfo.io = {};
        }
    }
}

void getCPUData(CPUInfo* cpus, cpuidState cpuState)
{
    if (isCpusInfoEmpty(cpus))
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "cpuInfo is empty, no PECI addresses are available\n");
    }
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        if (!cpu->cpuidRead.cpuidValid ||
            (cpu->cpuidRead.source == OVERWRITTEN))
        {
            getCPUID(cpu);
            parseCPUInfo(cpu, cpuState);
        }
    }
    if (cpuState == EVENT)
    {
        overwriteCPUInfo(cpus);
    }
    getDieMasks(cpus, cpuState);
    getCoreMasks(cpus, cpuState);
    getCHACounts(cpus, cpuState);
}

std::string newTimestamp(void)
{
    char logTime[64];
    time_t curtime;
    struct tm* loctime;

    // Add the timestamp
    curtime = time(NULL);
    loctime = localtime(&curtime);
    if (NULL != loctime)
    {
        strftime(logTime, sizeof(logTime), "%FT%TZ", loctime);
    }
    return logTime;
}

void logInputFileVersion(CPUInfo* cpuInfo, PlatformInfo* platformInfo)
{
    cJSON* jsonVer = cJSON_GetObjectItemCaseSensitive(
        cpuInfo->inputFile.bufferPtr, "Version");
    if ((jsonVer != NULL) && cJSON_IsString(jsonVer))
    {
        platformInfo->inputfile_ver = jsonVer->valuestring;
    }
    else
    {
        platformInfo->inputfile_ver = MD_NA;
    }
}

static void cleanupInputFiles(InputFileInfo* inputFileInfo)
{
    for (int i = 0; i < cd_numberOfModels; i++)
    {
        FREE(inputFileInfo->filenames[i]);
        cJSON_Delete(inputFileInfo->buffers[i]);
    }
}

void fillMetaDataCommon(PlatformInfo* platformInfo, CPUInfo* cpuInfo,
                        const std::string& platformName,
                        const std::string& triggerType,
                        const std::string& timestamp, char* biosVersion,
                        char* bmcVersion)
{
    platformInfo->trigger_type = (char*)triggerType.c_str();
    platformInfo->time_stamp = (char*)timestamp.c_str();
    platformInfo->platform_name = (char*)platformName.c_str();
    fillBiosId(biosVersion, SI_BMC_VER_LEN);
    platformInfo->bios_id = biosVersion;
    fillBmcVersion(bmcVersion, SI_BMC_VER_LEN);
    platformInfo->bmc_fw_ver = bmcVersion;
    platformInfo->crashdump_ver = CRASHDUMP_VER;
    fillInputFile(cpuInfo, platformInfo);
    logInputFileVersion(cpuInfo, platformInfo);
    platformInfo->me_fw_ver = MD_NA;
}

void createCrashdump(CPUInfo* cpus, std::string& crashdumpContents,
                     const std::string& triggerType, std::string& timestamp,
                     bool isTelemetry)
{
    cJSON* root = NULL;
    cJSON* crashlogData = NULL;
    cJSON* metaData = NULL;
    cJSON* processors = NULL;
    char* out = NULL;
    acdStatus status;
    RunTimeInfo runTimeInfo;
    runTimeInfo.maxGlobalTime = 0xFFFFFFFF;
    std::string platformName = getUuid();
    PlatformInfo platformInfo = {.bmc_fw_ver = NULL,
                                 .bios_id = NULL,
                                 .me_fw_ver = NULL,
                                 .time_stamp = NULL,
                                 .trigger_type = NULL,
                                 .platform_name = NULL,
                                 .crashdump_ver = NULL,
                                 .inputfile_ver = NULL,
                                 .input_file = NULL,
                                 .loopOverOneCpu = false,
                                 .loopOverOneDie = false,
                                 .loopOverOneIO = false};
    InputFileInfo inputFileInfo = {
        .unique = true, .filenames = {NULL}, .buffers = {NULL}};

    // Clear any resets that happened before the crashdump collection started
    clearResetDetected();

    CRASHDUMP_PRINT(INFO, stderr, "Crashdump started...\n");
    crashdump::initCPUInfo(cpus);
    crashdump::getCPUData(cpus, EVENT);
    crashdump::savePeciWake(cpus);
    crashdump::setPeciWake(cpus, ON);

    acdStatus inputFileLoadError =
        loadInputFiles(cpus, &inputFileInfo, isTelemetry);

    if (inputFileLoadError == ACD_INPUT_FILE_ERROR)
    {
        cJSON_free(out);
        CRASHDUMP_PRINT(INFO, stderr, "Completed!\n");

        cleanupInputFiles(&inputFileInfo);
        crashdump::checkPeciWake(cpus);
        crashdump::setPeciWake(cpus, OFF);

        return;
    }

    // start the JSON tree for CPU dump
    root = cJSON_CreateObject();

    // Build the CPU Crashdump JSON file
    // Everything is logged under a "crash_data" section
    cJSON_AddItemToObject(root, "crash_data",
                          crashlogData = cJSON_CreateObject());

    // Create the METADATA section
    cJSON_AddItemToObject(crashlogData, "METADATA",
                          metaData = cJSON_CreateObject());

    // Create the processors section
    cJSON_AddItemToObject(crashlogData, "PROCESSORS",
                          processors = cJSON_CreateObject());

    if (inputFileLoadError)
    {
        cJSON_AddStringToObject(metaData, "_input_file_error",
                                INPUT_FILE_ERROR_STR);
    }

    // Include the version field
    logCrashdumpVersion(processors, &cpus[0], RECORD_TYPE_BMCAUTONOMOUS);

    // Fill in the Crashdump data in the correct order (uncore to core) for
    // each CPU
    cJSON* maxCrashdumpTime = cJSON_GetObjectItemCaseSensitive(
        cpus[0].inputFile.bufferPtr, "MaxTimeInSec");
    if (maxCrashdumpTime != NULL)
    {
        runTimeInfo.maxGlobalTime = (uint32_t)maxCrashdumpTime->valueint;
    }

    clock_gettime(CLOCK_MONOTONIC, &runTimeInfo.sectionRunTime);
    clock_gettime(CLOCK_MONOTONIC, &crashdumpStart);

    runTimeInfo.globalRunTime = crashdumpStart;

    char biosVersion[SI_BMC_VER_LEN] = {0};
    char bmcVersion[SI_BMC_VER_LEN] = {0};
    fillMetaDataCommon(&platformInfo, &cpus[0], platformName, triggerType,
                       timestamp, biosVersion, bmcVersion);

    uint8_t numberOfSections = getNumberOfSections(&cpus[0]);
    sectionFailCount = 0;
    for (uint8_t section = 0; section < numberOfSections; section++)
    {
        platformInfo.loopOverOneCpu = false;
        for (uint8_t cpuNum = 0; cpuNum < MAX_CPUS; cpuNum++)
        {
            updateCurrentSection((int)section, (int)cpuNum);
            if (cpus[cpuNum].clientAddr == 0)
            {
                continue;
            }
            platformInfo.loopOverOneDie = true;
            platformInfo.loopOverOneIO = true;
            if (cpus[cpuNum].cpuidRead.cpuidValid)
            {
                if (cpus[cpuNum].dieMaskInfo.dieMaskSupported)
                {
                    cJSON* loopOnIoObj = getJsonFlagFromSection(
                        cpus[0].inputFile.bufferPtr, section, "LoopOnIO");
                    cJSON* loopOnComputeObj = getJsonFlagFromSection(
                        cpus[0].inputFile.bufferPtr, section, "LoopOnCompute");
                    cJSON* loopOnDomainObj = getJsonFlagFromSection(
                        cpus[0].inputFile.bufferPtr, section, "LoopOnDomain");

                    bool loopOnIO = cJSON_IsTrue(loopOnIoObj);
                    bool loopOnDomain = cJSON_IsTrue(loopOnDomainObj);
                    bool loopOnCompute = cJSON_IsTrue(loopOnComputeObj);
                    bool noDomainLoop =
                        !loopOnIO && !loopOnCompute && !loopOnDomain;

                    if (loopOnIO || loopOnDomain)
                    {
                        int dieNum = 0;
                        for (uint8_t die = 0;
                             die < cpus[cpuNum].dieMaskInfo.io.maxDies; die++)
                        {
                            if (!CHECK_BIT(
                                    cpus[cpuNum].dieMaskInfo.io.effectiveMask,
                                    die) ||
                                !platformInfo.loopOverOneIO)
                            {
                                continue;
                            }
                            if (!getFlagFromSection(cpus[0].inputFile.bufferPtr,
                                                    section, "LoopOnIO"))
                            {
                                platformInfo.loopOverOneIO = false;
                            }
                            status = fillNewSection(
                                root, &platformInfo, &cpus[cpuNum], cpuNum, die,
                                dieNum, &runTimeInfo, section,
                                isPostResetFlow(triggerType.c_str()));
                            if (status == ACD_FAILURE)
                            {
                                sectionFailCount++;
                            }
                            dieNum++;
                        }
                    }
                    // Note: Separate if condition to check LoopOnDomain and run
                    // both IO/compute paths.
                    if (loopOnCompute || loopOnDomain)
                    {
                        int dieNum = 0;
                        for (uint8_t die = 0;
                             die < cpus[cpuNum].dieMaskInfo.compute.maxDies;
                             die++)
                        {
                            if (!CHECK_BIT(
                                    cpus[cpuNum]
                                        .dieMaskInfo.compute.effectiveMask,
                                    die) ||
                                !platformInfo.loopOverOneDie)
                            {
                                continue;
                            }
                            if (!getFlagFromSection(cpus[0].inputFile.bufferPtr,
                                                    section, "LoopOnCompute"))
                            {
                                platformInfo.loopOverOneDie = false;
                            }
                            // Compute die starts at bit8 so increment die
                            status = fillNewSection(
                                root, &platformInfo, &cpus[cpuNum], cpuNum,
                                die + cpus[cpuNum].dieMaskInfo.compute.offset,
                                dieNum, &runTimeInfo, section,
                                isPostResetFlow(triggerType.c_str()));
                            dieNum++;
                            if (status == ACD_FAILURE)
                            {
                                sectionFailCount++;
                            }
                        }
                    }
                    else if (noDomainLoop)
                    {
                        status = fillNewSection(
                            root, &platformInfo, &cpus[cpuNum], cpuNum, 0, 0,
                            &runTimeInfo, section,
                            isPostResetFlow(triggerType.c_str()));
                        if (status == ACD_FAILURE)
                        {
                            sectionFailCount++;
                        }
                    }
                }
                else
                {
                    status =
                        fillNewSection(root, &platformInfo, &cpus[cpuNum],
                                       cpuNum, 0, 0, &runTimeInfo, section,
                                       isPostResetFlow(triggerType.c_str()));
                    if (status == ACD_FAILURE)
                    {
                        sectionFailCount++;
                    }
                }
            }
        }
    }
    logSectionRunTime(metaData, &crashdumpStart, GLOBAL_TIME_KEY);
    logResetDetected(metaData, cpus[0], platformState);

#ifdef CRASHDUMP_PRINT_UNFORMATTED
    out = cJSON_PrintUnformatted(root);
#else
    out = cJSON_Print(root);
#endif
    if (out != NULL)
    {
        crashdumpContents = out;
        // TODO: Temporarily disable BAFI due to new output format.
        //#ifdef TRIAGE_SECTION
        //        bool triageEnable = true;
        //        if (isTelemetry)
        //        {
        //            triageEnable = false;
        //        }
        //        if (readInputFileFlag(cpus[0].inputFile.bufferPtr,
        //        triageEnable,
        //                              TRIAGE_ENABLE))
        //        {
        //            appendTriageSection(crashdumpContents);
        //        }
        //        updateTotalTime(crashdumpContents, &crashdumpStart);
        //
        //#endif
        //#ifdef BAFI_NDA_OUTPUT
        //        bool summaryEnable = true;
        //        if (isTelemetry)
        //        {
        //            summaryEnable = false;
        //        }
        //        if (readInputFileFlag(cpus[0].inputFile.bufferPtr,
        //        summaryEnable,
        //                              SUMMARY_ENABLE))
        //        {
        //            appendSummarySection(crashdumpContents);
        //        }
        //        updateTotalTime(crashdumpContents, &crashdumpStart);
        //#endif
        cJSON_free(out);
        CRASHDUMP_PRINT(INFO, stderr, "Completed!\n");
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "cJSON_Print Failed\n");
    }

    if (platformState.resetDetected)
    {
        clearResetDetected();
    }

    // Clear crashedCoreMask every time crashdump is run
    for (size_t i = 0; i < MAX_CPUS; i++)
    {
        if (cpus[i].computeDies != NULL)
        {
            uint8_t computeDieCount =
                __builtin_popcount(cpus[i].dieMaskInfo.compute.effectiveMask);
            for (uint8_t die = 0; die < computeDieCount; die++)
            {
                cpus[i].computeDies[die].crashedCoreMask = 0;
            }
        }
        cpus[i].crashedCoreMask = 0;
    }

    cleanupInputFiles(&inputFileInfo);
    cJSON_Delete(root);
    crashdump::checkPeciWake(cpus);
    crashdump::setPeciWake(cpus, OFF);
}

int scandir_filter(const struct dirent* dirEntry)
{
    const std::string filePrefix(crashdumpPrefix + crashdumpFruName);
    return (strncmp(dirEntry->d_name, filePrefix.c_str(),
                    filePrefix.size()) == 0);
}

#ifndef NO_SYSTEMD
void dbusRemoveOnDemandLog()
{
    // Always make sure the D-Bus properties are removed
    server->remove_interface(logIface);
    logIface.reset();

    std::error_code ec;
    if (!std::filesystem::exists(crashdumpDir))
    {
        // Can't delete something that doesn't exist
        return;
    }

    for (auto& fileList : std::filesystem::directory_iterator(crashdumpDir))
    {
        // always iterate through all files in the directory to clear
        // on-demand files. This is just a safeguard in the event more
        // than a single file is created.
        std::string fname = fileList.path();
        if (fname.substr(crashdumpDir.string().size() + 1,
                         crashdumpFileRoot.size()) == crashdumpFileRoot)
        {
            if (!(std::filesystem::remove(fname, ec)))
            {
                CRASHDUMP_PRINT(ERR, stderr, "failed to remove %s: %s\n",
                                fname.c_str(), ec.message().c_str());
                break;
            }
        }
    }
}

void dbusRemoveTelemetryLog()
{
    // Always make sure the D-Bus properties are removed
    server->remove_interface(logIface);
    logIface.reset();

    std::error_code ec;
    if (!std::filesystem::exists(crashdumpDir))
    {
        // Can't delete something that doesn't exist
        return;
    }

    for (auto& fileList : std::filesystem::directory_iterator(crashdumpDir))
    {
        // always iterate through all files in the directory to clear
        // on-demand files. This is just a safeguard in the event more
        // than a single file is created.
        std::string fname = fileList.path();
        if (fname.substr(crashdumpDir.string().size() + 1,
                         crashdumpTelemetryFileRoot.size()) ==
            crashdumpTelemetryFileRoot)
        {
            if (!(std::filesystem::remove(fname, ec)))
            {
                fprintf(stderr, "failed to remove %s: %s\n", fname.c_str(),
                        ec.message().c_str());
                break;
            }
        }
    }
}

void dbusAddLog(const std::string& logContents, const std::string& timestamp,
                const std::string& dbusPath, const std::string& filename)
{
    FILE* fpJson = NULL;
    std::error_code ec;
    std::filesystem::path out_file = crashdumpDir / filename;

    // create the crashdump/output directory if it doesn't exist
    if (!(std::filesystem::create_directories(crashdumpDir, ec)))
    {
        if (ec.value() != 0)
        {
            CRASHDUMP_PRINT(ERR, stderr, "failed to create %s: %s\n",
                            crashdumpDir.c_str(), ec.message().c_str());
            return;
        }
    }

    fpJson = fopen(out_file.c_str(), "w");
    if (fpJson)
    {
        fprintf(fpJson, "%s", logContents.c_str());
        fclose(fpJson);
    }
    logIface = server->add_interface(dbusPath, crashdumpInterface);
    logIface->register_property("Log", out_file.string());
    logIface->register_property("Timestamp", timestamp);
    logIface->register_property("Filename", filename);
    logIface->initialize();
}

void newOnDemandLog(CPUInfo* cpus, std::string& onDemandLogContents,
                    std::string& timestamp)
{
    // Start the log to the on-demand location
    createCrashdump(cpus, onDemandLogContents, triggerTypeOnDemand, timestamp,
                    false);
}

void newTelemetryLog(CPUInfo* cpus, std::string& telemetryLogContents,
                     std::string& timestamp)
{
    // Start the log to the telemetry location
    createCrashdump(cpus, telemetryLogContents, triggerTypeOnDemand, timestamp,
                    true);
}

void incrementCrashdumpCount()
{
    // Get the current count
    conn->async_method_call(
        [](boost::system::error_code ec,
           const std::variant<uint8_t>& property) {
            if (ec)
            {
                CRASHDUMP_PRINT(ERR, stderr, "Failed to get Crashdump count\n");
                return;
            }
            const uint8_t* crashdumpCountVariant =
                std::get_if<uint8_t>(&property);
            if (crashdumpCountVariant == nullptr)
            {
                CRASHDUMP_PRINT(ERR, stderr,
                                "Unable to read Crashdump count\n");
                return;
            }
            uint8_t crashdumpCount = *crashdumpCountVariant;
            if (crashdumpCount == std::numeric_limits<uint8_t>::max())
            {
                CRASHDUMP_PRINT(ERR, stderr,
                                "Maximum crashdump count reached\n");
                return;
            }
            // Increment the count
            crashdumpCount++;
            conn->async_method_call(
                [](boost::system::error_code ec) {
                    if (ec)
                    {
                        CRASHDUMP_PRINT(ERR, stderr,
                                        "Failed to set Crashdump count\n");
                    }
                },
                "xyz.openbmc_project.Settings",
                "/xyz/openbmc_project/control/processor_error_config",
                "org.freedesktop.DBus.Properties", "Set",
                "xyz.openbmc_project.Control.Processor.ErrConfig",
                "CrashdumpCount", std::variant<uint8_t>{crashdumpCount});
        },
        "xyz.openbmc_project.Settings",
        "/xyz/openbmc_project/control/processor_error_config",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Control.Processor.ErrConfig", "CrashdumpCount");
}
#endif

void dbusAddStoredLog(const std::string& storedLogContents,
                      const std::string& timestamp)
{
    const std::string dumpFile(crashdumpPrefix + crashdumpFruName + "%llu");
    char const* crashdumpFile = dumpFile.c_str();
    uint64_t crashdumpNum = 0;
    struct dirent** namelist = NULL;
    FILE* fpJson = NULL;
    std::error_code ec;

    // create the crashdump/output directory if it doesn't exist
    if (!(std::filesystem::create_directories(crashdumpDir, ec)))
    {
        if (ec.value() != 0)
        {
            CRASHDUMP_PRINT(ERR, stderr, "failed to create %s: %s\n",
                            crashdumpDir.c_str(), ec.message().c_str());
            return;
        }
    }

    // Search the crashdump/output directory for existing log files
    int numLogFiles =
        scandir(crashdumpDir.c_str(), &namelist, scandir_filter, versionsort);
    if (numLogFiles < 0)
    {
        // scandir failed, so print the error
        perror("scandir");
        return;
    }

    // Get the number for this crashdump by finding the highest numbered
    // crashdump so far and incrementing by 1. The crashdumpNum is not kept as a
    // static in order to cover crashdump service restarts. In that case it is
    // undesirable to restart the number at zero.
    std::vector<std::string> storedLogsToRemove;
    for (int i = 0; i < numLogFiles; i++)
    {
        uint64_t currentNum = 0;
        int ret = sscanf_s(namelist[i]->d_name, crashdumpFile, &currentNum);
        if (ret > 0)
        {
            // This is a stored log, so track it for possible removal
            storedLogsToRemove.emplace_back(namelist[i]->d_name);
            if (currentNum >= crashdumpNum)
            {
                crashdumpNum = currentNum + 1;
            }
        }
        free(namelist[i]);
    }
    free(namelist);

    // In case multiple crashdumps are triggered for the same error, the policy
    // is to keep the first log until it is manually cleared and rotate through
    // additional logs.  This guarantees that we have the first and last log of
    // a failure.

    if (storedLogsToRemove.size() >= numStoredLogs)
    {
        // We want up to numStoredLogs including the first log and this log
        // So, keep log 0
        storedLogsToRemove.erase(storedLogsToRemove.begin());
        // ..and keep log 1 aka new begin()
        storedLogsToRemove.erase(storedLogsToRemove.begin());
        // and the highest numbered logs up to numStoredLogs - 2
        storedLogsToRemove.erase(
            std::prev(storedLogsToRemove.end(), numStoredLogs - 2),
            storedLogsToRemove.end());
    }
    else
    {
        // We haven't reached numStoredLogs yet, so keep all of them
        storedLogsToRemove.clear();
    }

    // Remove the remaining logs
    for (const std::string& filename : storedLogsToRemove)
    {
        std::error_code ec;
        if (!(std::filesystem::remove(crashdumpDir / filename, ec)))
        {
            CRASHDUMP_PRINT(ERR, stderr, "failed to remove %s: %s\n",
                            filename.c_str(), ec.message().c_str());
        }
#ifndef NO_SYSTEMD
        // Now remove the interface for the deleted log
        auto eraseit =
            std::find_if(storedLogIfaces.begin(), storedLogIfaces.end(),
                         [&filename](auto& log) {
                             std::string& storedName = std::get<0>(log);
                             return (storedName == filename);
                         });

        if (eraseit != std::end(storedLogIfaces))
        {
            crashdump::server->remove_interface(std::get<1>(*eraseit));
            storedLogIfaces.erase(eraseit);
        }
#endif
    }

    // Create the new crashdump filename
    std::string new_logfile_name = crashdumpPrefix + crashdumpFruName +
                                   std::to_string(crashdumpNum) + "-" +
                                   timestamp + ".json";
    std::filesystem::path out_file = crashdumpDir / new_logfile_name;

    // open the JSON file to write the crashdump contents
    fpJson = fopen(out_file.c_str(), "w");
    if (fpJson != NULL)
    {
        fprintf(fpJson, "%s", storedLogContents.c_str());
        fclose(fpJson);
    }

#ifndef NO_SYSTEMD
    // Add the new interface for this log
    std::filesystem::path path =
        std::filesystem::path(crashdumpPath) / std::to_string(crashdumpNum);
    std::shared_ptr<sdbusplus::asio::dbus_interface> ifaceLog =
        server->add_interface(path.c_str(), crashdumpInterface);
    storedLogIfaces.emplace_back(new_logfile_name, ifaceLog);
    // Log Property

    ifaceLog->register_property("Log", out_file.string());
    ifaceLog->register_property("Timestamp", timestamp);
    ifaceLog->register_property("Filename", new_logfile_name);
    ifaceLog->initialize();

    // Increment the count for this completed crashdump
    incrementCrashdumpCount();
#endif
}

void newStoredLog(CPUInfo* cpus, std::string& storedLogContents,
                  const std::string& triggerType, std::string& timestamp)
{
    // Start the log
    createCrashdump(cpus, storedLogContents, triggerType, timestamp, false);
}

bool isPECIAvailable()
{
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    getClientAddrs(cpus);
    if (isCpusInfoEmpty(cpus))
    {
        CRASHDUMP_PRINT(ERR, stderr, "PECI is not available!\n");
        return false;
    }
    return true;
}

#ifndef NO_SYSTEMD
/** Exception for when a log is attempted while power is off. */
struct PowerOffException final : public sdbusplus::exception_t
{
    const char* name() const noexcept override
    {
        return "org.freedesktop.DBus.Error.NotSupported";
    };
    const char* description() const noexcept override
    {
        return "Power off, cannot access peci";
    };
    const char* what() const noexcept override
    {
        return "org.freedesktop.DBus.Error.NotSupported: "
               "Power off, cannot access peci";
    };
    int get_errno() const noexcept override
    {
        return EOPNOTSUPP;
    }
};
/** Exception for when a log is attempted while another is in progress. */
struct LogInProgressException final : public sdbusplus::exception_t
{
    const char* name() const noexcept override
    {
        return "org.freedesktop.DBus.Error.ObjectPathInUse";
    };
    const char* description() const noexcept override
    {
        return "Log in progress";
    };
    const char* what() const noexcept override
    {
        return "org.freedesktop.DBus.Error.ObjectPathInUse: "
               "Log in progress";
    };
    int get_errno() const noexcept override
    {
        return EBUSY;
    }
};
#endif
} // namespace crashdump

#ifdef BUILD_MAIN

#ifndef NO_SYSTEMD
static void signalHandler(int sig)
{
    CRASHDUMP_PRINT(INFO, stderr,
                    "Stopping crashdump and cleaning up memory (%d)...\n", sig);
    crashdump::io.stop();
    cleanupComputeDies(crashdump::cpus);
}

static void initCrashdump()
{
    CRASHDUMP_PRINT(INFO, stderr, "Initializing crashdump...\n");
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&crashdump::cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    crashdump::initCPUInfo(crashdump::cpus);
    crashdump::getCPUData(crashdump::cpus, STARTUP);

// NAC Crashlog collection
#ifdef PMT_NAC_CRASHLOG
// TODO: temporarily comment out due to segfault on GNR
//    CRASHDUMP_PRINT(INFO, stderr, "Starting pmtCrashLog...\n");
//    PMTCrashlog pmtCrashLog(crashdump::conn);
//    CRASHDUMP_PRINT(INFO, stderr, "Initializing PMTDiscovery...\n");
//    pmtCrashLog.initPMTDiscovery(crashdump::io);
#endif
}

static void peciAvailableCheck()
{
    static boost::asio::steady_timer peciWaitTimer(crashdump::io);
    bool peciAvailable = crashdump::isPECIAvailable();

    if (peciAvailable)
    {
        initCrashdump();
    }
    else
    {
        peciWaitTimer.expires_after(
            std::chrono::seconds(crashdump::peciCheckInterval));
        peciWaitTimer.async_wait([](const boost::system::error_code& ec) {
            if (ec)
            {
                // operation_aborted is expected if timer is canceled
                // before completion.
                if (ec != boost::asio::error::operation_aborted)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "PECI Available Check async_wait failed %s\n",
                        ec.message().c_str());
                }
                return;
            }
            peciAvailableCheck();
        });
    }
}
#endif

int main(int argc, char *argv[])
{
    // Print version
    CRASHDUMP_PRINT(INFO, stderr, "Crashdump version: %s\n", CRASHDUMP_VER);

#ifdef NO_SYSTEMD
    uint8_t fru;
    std::string triggerType;
    CLI::App app("Crashdump");

    app.failure_message(CLI::FailureMessage::help);
    app.add_option("fru", fru, "FRU number");
    app.add_option("--type", triggerType, "Trigger Type");
    app.require_option(0, 2);
    CLI11_PARSE(app, argc, argv);

    platformInit(fru);

    if (!crashdump::isPECIAvailable())
    {
        CRASHDUMP_PRINT(ERR, stderr, "PECI not available\n");
    }
    crashdump::initCPUInfo(crashdump::cpus);
    crashdump::getCPUData(crashdump::cpus, STARTUP);

    std::string storedLogContents;
    std::string storedLogTime = crashdump::newTimestamp();
    crashdump::newStoredLog(crashdump::cpus, storedLogContents,
                            triggerType, storedLogTime);
    if (storedLogContents.empty())
    {
        // Log is empty, so don't save it
        CRASHDUMP_PRINT(ERR, stderr, "Log is empty, so don't save it\n");
        return -1;
    }

    crashdump::dbusAddStoredLog(storedLogContents, storedLogTime);
#else
    // capture ctrl-c & systemctl stop signals
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // future to use for long-running tasks
    std::future<void> future;

    // setup connection to dbus
    crashdump::conn =
        std::make_shared<sdbusplus::asio::connection>(crashdump::io);

    // CPU Debug Log Object
    crashdump::conn->request_name(crashdump::crashdumpService);
    crashdump::server =
        std::make_shared<sdbusplus::asio::object_server>(crashdump::conn);

    // Reserve space for the stored log interfaces
    crashdump::storedLogIfaces.reserve(crashdump::numStoredLogs);

    // Stored Log Interface
    std::shared_ptr<sdbusplus::asio::dbus_interface> ifaceStored =
        crashdump::server->add_interface(crashdump::crashdumpPath,
                                         crashdump::crashdumpStoredInterface);

    peciAvailableCheck();

    // Generate a Stored Log
    ifaceStored->register_method(
        "GenerateStoredLog",
        [&future, ifaceStored](const std::string& triggerType) {
            if (!crashdump::isPECIAvailable())
            {
                throw crashdump::PowerOffException();
            }
            if (future.valid() && future.wait_for(std::chrono::seconds(0)) !=
                                      std::future_status::ready)
            {
                throw crashdump::LogInProgressException();
            }
            future =
                std::async(std::launch::async, [triggerType, ifaceStored]() {
                    constexpr char const* crashdumpCompleteStr =
                        "CrashdumpComplete";
                    constexpr char const* crashdumpFailStr = "CrashdumpFail";
                    std::string storedLogContents;
                    std::string storedLogTime = crashdump::newTimestamp();
                    crashdump::newStoredLog(crashdump::cpus, storedLogContents,
                                            triggerType, storedLogTime);

                    // Signal that Crashdump is complete or fail
                    try
                    {
                        bool isCrashdumpFail = isCrashDumpFail(
                            crashdump::sectionFailCount, triggerType.c_str());
                        char const* crashdumpStatusStr =
                            isCrashdumpFail ? crashdumpFailStr
                                            : crashdumpCompleteStr;
                        CRASHDUMP_PRINT(INFO, stderr, "Status: %s (%d)\n",
                                        crashdumpStatusStr,
                                        crashdump::sectionFailCount);
                        sdbusplus::message::message msg =
                            ifaceStored->new_signal(crashdumpStatusStr);

                        msg.signal_send();
                    }
                    catch (const sdbusplus::exception::exception& e)
                    {
                        CRASHDUMP_PRINT(
                            ERR, stderr,
                            "Failed to send CrashdumpComplete signal\n");
                    }

                    if (storedLogContents.empty())
                    {
                        // Log is empty, so don't save it
                        return;
                    }
                    boost::asio::post(
                        crashdump::io,
                        [storedLogContents = std::move(storedLogContents),
                         storedLogTime = std::move(storedLogTime)]() mutable {
                            crashdump::dbusAddStoredLog(storedLogContents,
                                                        storedLogTime);
                        });
                });
            return std::string("Log Started");
        });
    ifaceStored->initialize();

    // DeleteAll Interface
    std::shared_ptr<sdbusplus::asio::dbus_interface> ifaceDeleteAll =
        crashdump::server->add_interface(
            crashdump::crashdumpPath, crashdump::crashdumpDeleteAllInterface);

    // Delete all stored logs
    ifaceDeleteAll->register_method("DeleteAll", []() {
        std::error_code ec;
        for (auto& [file, interface] : crashdump::storedLogIfaces)
        {
            if (!(std::filesystem::remove(crashdump::crashdumpDir / file, ec)))
            {
                CRASHDUMP_PRINT(ERR, stderr, "failed to remove %s: %s\n",
                                file.c_str(), ec.message().c_str());
            }
            crashdump::server->remove_interface(interface);
        }
        crashdump::storedLogIfaces.clear();
        crashdump::dbusRemoveOnDemandLog();
        CRASHDUMP_PRINT(INFO, stderr, "Crashdump logs cleared\n");
        return std::string("Logs Cleared");
    });
    ifaceDeleteAll->initialize();

    // OnDemand Log Interface
    std::shared_ptr<sdbusplus::asio::dbus_interface> ifaceOnDemand =
        crashdump::server->add_interface(crashdump::crashdumpPath,
                                         crashdump::crashdumpOnDemandInterface);

    // Generate an OnDemand Log
    ifaceOnDemand->register_method("GenerateOnDemandLog", [&future]() {
        if (!crashdump::isPECIAvailable())
        {
            throw crashdump::PowerOffException();
        }
        // Check if a Log is in progress
        if (future.valid() && future.wait_for(std::chrono::seconds(0)) !=
                                  std::future_status::ready)
        {
            throw crashdump::LogInProgressException();
        }
        // Remove the old on-demand log
        crashdump::dbusRemoveOnDemandLog();

        // Start the log asynchronously since it can take a long time
        future = std::async(std::launch::async, []() {
            std::string onDemandLogContents;
            std::string onDemandTimestamp = crashdump::newTimestamp();
            std::string filename =
                crashdump::crashdumpFileRoot + onDemandTimestamp + ".json";
            crashdump::newOnDemandLog(crashdump::cpus, onDemandLogContents,
                                      onDemandTimestamp);
            boost::asio::post(
                crashdump::io,
                [onDemandLogContents = std::move(onDemandLogContents),
                 onDemandTimestamp = std::move(onDemandTimestamp),
                 filename]() mutable {
                    crashdump::dbusAddLog(
                        onDemandLogContents, onDemandTimestamp,
                        crashdump::crashdumpOnDemandPath, filename);
                });
        });

        // Return success
        return std::string("Log Started");
    });

    ifaceOnDemand->initialize();

    // Telemetry Log Interface
    std::shared_ptr<sdbusplus::asio::dbus_interface> ifaceTelemetry =
        crashdump::server->add_interface(
            crashdump::crashdumpPath, crashdump::crashdumpTelemetryInterface);

    // Generate a Telemetry Log
    ifaceTelemetry->register_method("GenerateTelemetryLog", [&future]() {
        if (!crashdump::isPECIAvailable())
        {
            throw crashdump::PowerOffException();
        }
        // Check if a Log is in progress
        if (future.valid() && future.wait_for(std::chrono::seconds(0)) !=
                                  std::future_status::ready)
        {
            throw crashdump::LogInProgressException();
        }
        crashdump::dbusRemoveTelemetryLog();

        // Start the log asynchronously since it can take a long time
        future = std::async(std::launch::async, []() {
            std::string telemetryLogContents;
            std::string telemetryTimestamp = crashdump::newTimestamp();
            std::string filename = crashdump::crashdumpTelemetryFileRoot +
                                   telemetryTimestamp + ".json";
            crashdump::newTelemetryLog(crashdump::cpus, telemetryLogContents,
                                       telemetryTimestamp);
            boost::asio::post(
                crashdump::io,
                [telemetryLogContents = std::move(telemetryLogContents),
                 telemetryTimestamp = std::move(telemetryTimestamp),
                 filename]() mutable {
                    crashdump::dbusAddLog(
                        telemetryLogContents, telemetryTimestamp,
                        crashdump::crashdumpTelemetryPath, filename);
                });
        });

        // Return success
        return std::string("Log Started");
    });

    ifaceTelemetry->initialize();
    try
    {
        // Build up paths for any existing stored logs
        if (std::filesystem::exists(crashdump::crashdumpDir))
        {
            std::regex search("crashdump_([[:digit:]]+)-([[:graph:]]+?).json");
            std::smatch match;
            for (auto& p :
                 std::filesystem::directory_iterator(crashdump::crashdumpDir))
            {
                std::string file = p.path().filename();
                if (std::regex_match(file, match, search))
                {
                    // Log Interface
                    std::filesystem::path path =
                        std::filesystem::path(crashdump::crashdumpPath) /
                        match.str(1);
                    std::shared_ptr<sdbusplus::asio::dbus_interface> ifaceLog =
                        crashdump::server->add_interface(
                            path.c_str(), crashdump::crashdumpInterface);
                    crashdump::storedLogIfaces.emplace_back(file, ifaceLog);
                    // Log Property
                    ifaceLog->register_property("Log", p.path().string());
                    ifaceLog->register_property("Timestamp", match.str(2));
                    ifaceLog->register_property("Filename", file);
                    ifaceLog->initialize();
                }
            }
            crashdump::dbusRemoveOnDemandLog();
        }
    }
    catch (const std::regex_error& e)
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "A regex error ocurred while removing previous OnDemand logs\n");
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "A SdBusError ocurred while removing previous OnDemand logs\n");
    }
    // Start tracking host state
    std::shared_ptr<sdbusplus::bus::match::match> hostStateMonitor =
        crashdump::startHostStateMonitor(crashdump::conn);

    try
    {
        CRASHDUMP_PRINT(INFO, stderr, "crashdump io service starting...\n");
        crashdump::io.run();
    }
    catch (const boost::system::system_error& e)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Failed to run io\n");
    }
#endif

    CRASHDUMP_PRINT(INFO, stderr, "crashdump stopped successfully.\n");
    return ACD_SUCCESS;
}
#endif // BUILD_MAIN
