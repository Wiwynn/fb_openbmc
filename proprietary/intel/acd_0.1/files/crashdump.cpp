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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#ifndef IPMB_PECI_INTF
#include <peci.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef USE_SYSTEMD
#include <systemd/sd-id128.h>
#endif
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <boost/asio/io_service.hpp>
#include <filesystem>
#include <fstream>
#include <future>
#include <regex>
#ifdef USE_SYSTEMD
#include <sdbusplus/asio/object_server.hpp>
#endif
#include <sstream>
#include <vector>
#ifndef USE_SYSTEMD
#include <CLI/CLI.hpp>
#endif

extern "C" {
#include <cjson/cJSON.h>

#include "safe_str_lib.h"
}

#include "CrashdumpSections/AddressMap.hpp"
#include "CrashdumpSections/BigCore.hpp"
#include "CrashdumpSections/CoreMca.hpp"
#include "CrashdumpSections/MetaData.hpp"
#include "CrashdumpSections/PowerManagement.hpp"
#include "CrashdumpSections/SqDump.hpp"
#include "CrashdumpSections/TorDump.hpp"
#include "CrashdumpSections/Uncore.hpp"
#include "CrashdumpSections/UncoreMca.hpp"
#include "utils.hpp"
#ifdef OEMDATA_SECTION
#include "CrashdumpSections/OemData.hpp"
#endif

extern int getSystemGuid(std::string &);
extern void platformInit(uint8_t);

namespace crashdump
{
static std::vector<crashdump::CPUInfo> cpuInfo;
#ifdef USE_SYSTEMD
static boost::asio::io_service io;
static std::shared_ptr<sdbusplus::asio::connection> conn;
static std::shared_ptr<sdbusplus::asio::object_server> server;
static std::shared_ptr<sdbusplus::asio::dbus_interface> logIface;
static std::vector<
    std::pair<std::string, std::shared_ptr<sdbusplus::asio::dbus_interface>>>
    storedLogIfaces;

constexpr char const* crashdumpService = "com.intel.crashdump";
constexpr char const* crashdumpPath = "/com/intel/crashdump";
constexpr char const* crashdumpInterface = "com.intel.crashdump";
constexpr char const* crashdumpOnDemandPath = "/com/intel/crashdump/OnDemand";
constexpr char const* crashdumpTelemetryPath = "/com/intel/crashdump/Telemetry";
constexpr char const* crashdumpStoredInterface = "com.intel.crashdump.Stored";
constexpr char const* crashdumpDeleteAllInterface =
    "xyz.openbmc_project.Collection.DeleteAll";
constexpr char const* crashdumpOnDemandInterface =
    "com.intel.crashdump.OnDemand";
constexpr char const* crashdumpTelemetryInterface =
    "com.intel.crashdump.Telemetry";
constexpr char const* crashdumpRawPeciInterface =
    "com.intel.crashdump.SendRawPeci";
#endif
static const std::filesystem::path crashdumpDir = "/tmp/crashdump/output";
static const std::string crashdumpFileRoot{"crashdump_ondemand_"};
static const std::string crashdumpTelemetryFileRoot{"telemetry_"};
static const std::string crashdumpPrefix{"crashdump_"};

constexpr char const* triggerTypeOnDemand = "On-Demand";
constexpr int const vcuPeciWake = 5;

static PlatformState platformState = {false, DEFAULT_VALUE, DEFAULT_VALUE,
                                      DEFAULT_VALUE, DEFAULT_VALUE};

const CrashdumpSection sectionNames[NUMBER_OF_SECTIONS] = {
    {"uncore", UNCORE, record_type::uncoreStatusLog, logUncoreStatus},
    {"TOR", TOR, record_type::torDump, logTorDump},
    {"PM_info", PM_INFO, record_type::pmInfo, logPowerManagement},
    {"address_map", ADDRESS_MAP, record_type::addressMap, logAddressMap},
    {"big_core", BIG_CORE, record_type::coreCrashLog, nullptr},
    {"MCA", MCA, record_type::mcaLog, nullptr},
    {"METADATA", METADATA, record_type::metadata, nullptr}};

static void logRunTime(cJSON* parent, timespec* start, char* key)
{
    char timeString[64];
    timespec finish = {};
    uint64_t timeVal = 0;

    clock_gettime(CLOCK_MONOTONIC, &finish);
    uint64_t runTimeInNs = tsToNanosecond(&finish) - tsToNanosecond(start);

    timeVal = runTimeInNs;

    // only log the last metaData run
    cJSON_GetObjectItemCaseSensitive(parent, "_time");

    cd_snprintf_s(timeString, sizeof(timeString), "%.2fs",
                  (double)timeVal / 1e9);
    cJSON_AddStringToObject(parent, key, timeString);

    clock_gettime(CLOCK_MONOTONIC, start);
}

static const std::string getUuid()
{
    std::string ret;
#ifndef USE_SYSTEMD
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

static void getClientAddrs(std::vector<CPUInfo>& cpuInfo)
{
    for (int cpu = 0, addr = MIN_CLIENT_ADDR; addr <= MAX_CLIENT_ADDR;
         cpu++, addr++)
    {
        if (peci_Ping(addr) == PECI_CC_SUCCESS)
        {
            cpuInfo.emplace_back();
            cpuInfo[cpu].clientAddr = addr;
        }
    }
}

static bool savePeciWake(std::vector<CPUInfo>& cpuInfo)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;
    uint32_t peciRdValue = 0;
    for (CPUInfo& cpu : cpuInfo)
    {
        retval =
            peci_RdPkgConfig(cpu.clientAddr, vcuPeciWake, ON, sizeof(uint32_t),
                             (uint8_t*)&peciRdValue, &cc);
        if (retval != PECI_CC_SUCCESS)
        {
            CRASHDUMP_PRINT(
                ERR, stderr,
                "Cannot read Wake_on_PECI-> addr: (0x%x), ret: (0x%x), \
                        cc: (0x%x)\n",
                cpu.clientAddr, retval, cc);
            cpu.initialPeciWake = UNKNOWN;
            continue;
        }
        cpu.initialPeciWake = (peciRdValue & 0x1) ? ON : OFF;
    }
    return true;
}

static bool setPeciWake(std::vector<CPUInfo>& cpuInfo, pwState desiredState)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;
    int writeValue = OFF;
    for (CPUInfo& cpu : cpuInfo)
    {
        if ((cpu.initialPeciWake == ON) || (cpu.initialPeciWake == UNKNOWN))
            continue;
        writeValue = static_cast<int>(desiredState);
        retval = peci_WrPkgConfig(cpu.clientAddr, vcuPeciWake, writeValue,
                                  writeValue, sizeof(uint32_t), &cc);
        if (retval != PECI_CC_SUCCESS)
        {
            CRASHDUMP_PRINT(
                ERR, stderr,
                "Cannot set Wake_on_PECI-> addr: (0x%x), ret: (0x%x), cc: "
                "(0x%x)\n",
                cpu.clientAddr, retval, cc);
        }
    }
    return true;
}

static bool checkPeciWake(std::vector<CPUInfo>& cpuInfo)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;
    uint32_t peciRdValue = 0;
    for (CPUInfo& cpu : cpuInfo)
    {
        retval =
            peci_RdPkgConfig(cpu.clientAddr, vcuPeciWake, ON, sizeof(uint32_t),
                             (uint8_t*)&peciRdValue, &cc);
        if (retval != PECI_CC_SUCCESS)
        {
            CRASHDUMP_PRINT(
                ERR, stderr,
                "Cannot read Wake_on_PECI-> addr: (0x%x), ret: (0x%x), \
                        cc: (0x%x)\n",
                cpu.clientAddr, retval, cc);
            continue;
        }
        if (peciRdValue != 1)
        {
            CRASHDUMP_PRINT(ERR, stderr, "Wake_on_PECI in OFF state: (0x%x)\n",
                            cpu.clientAddr);
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

inline void updateCurrentSection(const CrashdumpSection& sectionName,
                                 const CPUInfo& cpuInfo)
{
    platformState.currentSection = sectionName.section;
    platformState.currentCpu = cpuInfo.clientAddr & 0xF;
}

static void loadInputFiles(std::vector<CPUInfo>& cpuInfo,
                           InputFileInfo* inputFileInfo, bool isTelemetry)
{
    int uniqueCount = 0;
    cJSON* defaultStateSection = NULL;
    bool enable = false;

    for (CPUInfo& cpu : cpuInfo)
    {
        // read and allocate memory for crashdump input file
        // if it hasn't been read before
        if (inputFileInfo->buffers[cpu.model] == NULL)
        {
            inputFileInfo->buffers[cpu.model] = selectAndReadInputFile(
                cpu.model, &inputFileInfo->filenames[cpu.model], isTelemetry);
            if (inputFileInfo->buffers[cpu.model] != NULL)
            {
                uniqueCount++;
            }
        }

        inputFileInfo->unique = (uniqueCount <= 1);
        cpu.inputFile.filenamePtr = inputFileInfo->filenames[cpu.model];
        cpu.inputFile.bufferPtr = inputFileInfo->buffers[cpu.model];

        // Get and Check global enable/disable value from "DefaultState"
        defaultStateSection = getCrashDataSection(
            inputFileInfo->buffers[cpu.model], "DefaultState", &enable);
        int defaultStateEnable = 1;
        if (defaultStateSection != NULL)
        {
            strcmp_s(defaultStateSection->valuestring, CRASHDUMP_VALUE_LEN,
                     "Enable", &defaultStateEnable);
        }
        else
        {
            CRASHDUMP_PRINT(ERR, stderr, "Missing \"DefaultState\" in %s\n",
                            inputFileInfo->filenames[cpu.model]);
        }
        if (defaultStateEnable == ENABLE)
        {
            // Check local enable "_record_enable"
            for (uint8_t i = 0; i < crashdump::NUMBER_OF_SECTIONS; i++)
            {
                getCrashDataSection(inputFileInfo->buffers[cpu.model],
                                    crashdump::sectionNames[i].name, &enable);
                enable ? SET_BIT(cpu.sectionMask, i)
                       : CLEAR_BIT(cpu.sectionMask, i);
            }
        }
        else
        {
            cpu.sectionMask = 0x00;
        }
    }
}

static void getUPIDisable(std::vector<CPUInfo>& cpuInfo)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;
    for (CPUInfo& cpu : cpuInfo)
    {
        switch (cpu.model)
        {
            case cpu::cpx:
                // CAPID2 Local PCI B1:D30:F3 Reg 0x8C
                uint32_t capid2;
                retval = peci_RdPCIConfigLocal(cpu.clientAddr, 1, 30, 3, 0x8C,
                                               sizeof(capid2),
                                               (uint8_t*)&capid2, &cc);
                cpu.capidRead.capid2Cc = cc;
                cpu.capidRead.capid2Ret = retval;
                if (retval != PECI_CC_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Cannot find capid2! ret: (0x%x), cc: (0x%x)\n", retval,
                        cc);
                    cpu.capidRead.capid2 = 0;
                    continue;
                }
                cpu.capidRead.capid2 = capid2;
                break;
            case cpu::clx:
            case cpu::skx:
            case cpu::icx:
            case cpu::icx2:
            case cpu::spr:
            default:
                break;
        }
    }
}

static bool getCoreMasks(std::vector<CPUInfo>& cpuInfo,
                         cpuid::cpuidState cpuState)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;

    for (CPUInfo& cpu : cpuInfo)
    {
        if (cpu.coreMaskRead.coreMaskValid)
        {
            break;
        }
        uint32_t coreMask0;
        uint32_t coreMask1;

        switch (cpu.model)
        {
            case cpu::cpx:
            case cpu::clx:
            case cpu::skx:
                // RESOLVED_CORES Local PCI B1:D30:F3 Reg 0xB4
                uint32_t coreMask;
                retval = peci_RdPCIConfigLocal(cpu.clientAddr, 1, 30, 3, 0xB4,
                                               sizeof(coreMask),
                                               (uint8_t*)&coreMask, &cc);
                cpu.coreMaskRead.coreMaskCc = cc;
                cpu.coreMaskRead.coreMaskRet = retval;
                if (retval != PECI_CC_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Cannot find coreMask! ret: (0x%x), cc: (0x%x)\n",
                        retval, cc);
                    break;
                }
                cpu.coreMask = coreMask;
                cpu.coreMaskRead.coreMaskValid = true;
                cpu.coreMaskRead.source = cpuState;
                break;
            case cpu::icx:
            case cpu::icx2:
            case cpu::icxd:
                // RESOLVED_CORES Local PCI B14:D30:F3 Reg 0xD0 and 0xD4
                retval = peci_RdPCIConfigLocal(cpu.clientAddr, 14, 30, 3, 0xD0,
                                               sizeof(coreMask0),
                                               (uint8_t*)&coreMask0, &cc);
                cpu.coreMaskRead.coreMaskCc = cc;
                cpu.coreMaskRead.coreMaskRet = retval;
                if (retval != PECI_CC_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Cannot find coreMask0! ret: (0x%x), cc: (0x%x)\n",
                        retval, cc);
                    break;
                }
                retval = peci_RdPCIConfigLocal(cpu.clientAddr, 14, 30, 3, 0xD4,
                                               sizeof(coreMask1),
                                               (uint8_t*)&coreMask1, &cc);
                cpu.coreMaskRead.coreMaskCc = cc;
                cpu.coreMaskRead.coreMaskRet = retval;
                if (retval != PECI_CC_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Cannot find coreMask1! ret: (0x%x), cc: (0x%x)\n",
                        retval, cc);
                    break;
                }
                cpu.coreMask = coreMask1;
                cpu.coreMask <<= 32;
                cpu.coreMask |= coreMask0;
                cpu.coreMaskRead.coreMaskValid = true;
                cpu.coreMaskRead.source = cpuState;
                break;
            case cpu::spr:
                // RESOLVED_CORES EP Local PCI B31:D30:F6 Reg 0x80 and 0x84
                retval = peci_RdEndPointConfigPciLocal(
                    cpu.clientAddr, 0, 31, 30, 6, 0x80, sizeof(coreMask0),
                    (uint8_t*)&coreMask0, &cc);
                cpu.coreMaskRead.coreMaskCc = cc;
                cpu.coreMaskRead.coreMaskRet = retval;
                if (retval != PECI_CC_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Cannot find coreMask0! ret: (0x%x), cc: (0x%x)\n",
                        retval, cc);
                    break;
                }
                retval = peci_RdEndPointConfigPciLocal(
                    cpu.clientAddr, 0, 31, 30, 6, 0x84, sizeof(coreMask1),
                    (uint8_t*)&coreMask1, &cc);
                cpu.coreMaskRead.coreMaskCc = cc;
                cpu.coreMaskRead.coreMaskRet = retval;
                if (retval != PECI_CC_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Cannot find coreMask1! ret: (0x%x), cc: (0x%x)\n",
                        retval, cc);
                    break;
                }
                cpu.coreMask = coreMask1;
                cpu.coreMask <<= 32;
                cpu.coreMask |= coreMask0;
                cpu.coreMaskRead.coreMaskValid = true;
                cpu.coreMaskRead.source = cpuState;
                break;
            default:
                return false;
        }
    }
    return true;
}

static bool getCHACounts(std::vector<CPUInfo>& cpuInfo,
                         cpuid::cpuidState cpuState)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;

    for (CPUInfo& cpu : cpuInfo)
    {
        if (cpu.chaCountRead.chaCountValid)
        {
            break;
        }
        uint32_t chaMask0;
        uint32_t chaMask1;

        switch (cpu.model)
        {
            case cpu::cpx:
            case cpu::clx:
            case cpu::skx:
                // LLC_SLICE_EN Local PCI B1:D30:F3 Reg 0x9C
                uint32_t chaMask;
                retval = peci_RdPCIConfigLocal(cpu.clientAddr, 1, 30, 3, 0x9C,
                                               sizeof(chaMask),
                                               (uint8_t*)&chaMask, &cc);
                cpu.chaCountRead.chaCountCc = cc;
                cpu.chaCountRead.chaCountRet = retval;
                if (retval != PECI_CC_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Cannot find chaMask! ret: (0x%x), cc: (0x%x)\n",
                        retval, cc);
                    break;
                }
                cpu.chaCount = __builtin_popcount(chaMask);
                cpu.chaCountRead.chaCountValid = true;
                cpu.chaCountRead.source = cpuState;
                break;
            case cpu::icx:
            case cpu::icx2:
            case cpu::icxd:
                // LLC_SLICE_EN Local PCI B14:D30:F3 Reg 0x9C and 0xA0
                retval = peci_RdPCIConfigLocal(cpu.clientAddr, 14, 30, 3, 0x9C,
                                               sizeof(chaMask0),
                                               (uint8_t*)&chaMask0, &cc);
                cpu.chaCountRead.chaCountCc = cc;
                cpu.chaCountRead.chaCountRet = retval;
                if (retval != PECI_CC_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Cannot find chaMask0! ret: (0x%x),  cc: (0x%x)\n",
                        retval, cc);
                    break;
                }
                retval = peci_RdPCIConfigLocal(cpu.clientAddr, 14, 30, 3, 0xA0,
                                               sizeof(chaMask1),
                                               (uint8_t*)&chaMask1, &cc);
                cpu.chaCountRead.chaCountCc = cc;
                cpu.chaCountRead.chaCountRet = retval;
                if (retval != PECI_CC_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Cannot find chaMask1! ret: (0x%x), cc: (0x%x)\n",
                        retval, cc);
                    break;
                }
                cpu.chaCount =
                    __builtin_popcount(chaMask0) + __builtin_popcount(chaMask1);
                cpu.chaCountRead.chaCountValid = true;
                cpu.chaCountRead.source = cpuState;
                break;
            case cpu::spr:
                // LLC_SLICE_EN EP Local PCI B31:D30:F3 Reg 0x9C and 0xA0
                retval = peci_RdEndPointConfigPciLocal(
                    cpu.clientAddr, 0, 31, 30, 3, 0x9c, sizeof(chaMask0),
                    (uint8_t*)&chaMask0, &cc);
                cpu.chaCountRead.chaCountCc = cc;
                cpu.chaCountRead.chaCountRet = retval;
                if (retval != PECI_CC_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Cannot find chaMask0! ret: (0x%x),  cc: (0x%x)\n",
                        retval, cc);
                    break;
                }
                retval = peci_RdEndPointConfigPciLocal(
                    cpu.clientAddr, 0, 31, 30, 3, 0xa0, sizeof(chaMask1),
                    (uint8_t*)&chaMask1, &cc);
                cpu.chaCountRead.chaCountCc = cc;
                cpu.chaCountRead.chaCountRet = retval;
                if (retval != PECI_CC_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Cannot find chaMask1! ret: (0x%x), cc: (0x%x)\n",
                        retval, cc);
                    break;
                }
                cpu.chaCount =
                    __builtin_popcount(chaMask0) + __builtin_popcount(chaMask1);
                cpu.chaCountRead.chaCountValid = true;
                cpu.chaCountRead.source = cpuState;
                break;
            default:
                return false;
        }
    }
    return true;
}

static void getCPUID(CPUInfo& cpuInfo)
{
    uint8_t cc = 0;
    CPUModel cpuModel{};
    uint8_t stepping = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;
    retval = peci_GetCPUID(cpuInfo.clientAddr, &cpuModel, &stepping, &cc);
    cpuInfo.cpuidRead.cpuModel = cpuModel;
    cpuInfo.cpuidRead.stepping = stepping;
    cpuInfo.cpuidRead.cpuidCc = cc;
    cpuInfo.cpuidRead.cpuidRet = retval;
    if (retval != PECI_CC_SUCCESS)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Cannot get CPUID! ret: (0x%x), cc: (0x%x)\n", retval,
                        cc);
    }
}

static void parseCPUInfo(CPUInfo& cpuInfo, cpuid::cpuidState cpuState)
{
    switch ((int)cpuInfo.cpuidRead.cpuModel)
    {
        case skx:
            if (cpuInfo.cpuidRead.stepping >= cpu::stepping::cpx)
            {
                CRASHDUMP_PRINT(INFO, stderr, "CPX detected (CPUID 0x%x)\n",
                                cpuInfo.cpuidRead.cpuModel |
                                    cpuInfo.cpuidRead.stepping);
                cpuInfo.model = cpu::cpx;
            }
            else if (cpuInfo.cpuidRead.stepping >= cpu::stepping::clx)
            {
                CRASHDUMP_PRINT(INFO, stderr, "CLX detected (CPUID 0x%x)\n",
                                cpuInfo.cpuidRead.cpuModel |
                                    cpuInfo.cpuidRead.stepping);
                cpuInfo.model = cpu::clx;
            }
            else
            {
                CRASHDUMP_PRINT(INFO, stderr, "SKX detected (CPUID 0x%x)\n",
                                cpuInfo.cpuidRead.cpuModel |
                                    cpuInfo.cpuidRead.stepping);
                cpuInfo.model = cpu::skx;
            }
            cpuInfo.cpuidRead.cpuidValid = true;
            cpuInfo.cpuidRead.source = cpuState;
            break;
        case icx:
            CRASHDUMP_PRINT(INFO, stderr, "ICX detected (CPUID 0x%x)\n",
                            cpuInfo.cpuidRead.cpuModel |
                                cpuInfo.cpuidRead.stepping);
            if (cpuInfo.cpuidRead.stepping >= cpu::stepping::icx2)
            {
                cpuInfo.model = cpu::icx2;
            }
            else
            {
                cpuInfo.model = cpu::icx;
            }
            cpuInfo.cpuidRead.cpuidValid = true;
            cpuInfo.cpuidRead.source = cpuState;
            break;
        case SPR_MODEL:
            CRASHDUMP_PRINT(INFO, stderr, "SPR detected (CPUID 0x%x)\n",
                            cpuInfo.cpuidRead.cpuModel |
                                cpuInfo.cpuidRead.stepping);
            cpuInfo.model = cpu::spr;
            cpuInfo.cpuidRead.cpuidValid = true;
            cpuInfo.cpuidRead.source = cpuState;
            break;
        case ICXD_MODEL:
            CRASHDUMP_PRINT(INFO, stderr, "ICXD detected (CPUID 0x%x)\n",
                            cpuInfo.cpuidRead.cpuModel |
                                cpuInfo.cpuidRead.stepping);
            cpuInfo.model = cpu::icxd;
            cpuInfo.cpuidRead.cpuidValid = true;
            cpuInfo.cpuidRead.source = cpuState;
            break;
        default:
            CRASHDUMP_PRINT(ERR, stderr, "Unsupported CPUID 0x%x\n",
                            cpuInfo.cpuidRead.cpuModel |
                                cpuInfo.cpuidRead.stepping);
            cpuInfo.cpuidRead.cpuidValid = false;
            break;
    }
}

static void overwriteCPUInfo(std::vector<CPUInfo>& cpuInfo)
{
    bool found = false;
    cpu::Model defaultModel;
    CPUModel defaultCpuModel;
    uint8_t defaultStepping;
    for (CPUInfo& cpu : cpuInfo)
    {
        if (cpu.cpuidRead.cpuidValid)
        {
            defaultModel = cpu.model;
            defaultCpuModel = cpu.cpuidRead.cpuModel;
            defaultStepping = cpu.cpuidRead.stepping;
            found = true;
            break;
        }
    }
    for (CPUInfo& cpu : cpuInfo)
    {
        if (!cpu.cpuidRead.cpuidValid)
        {
            if (found)
            {
                cpu.model = defaultModel;
                cpu.cpuidRead.cpuModel = defaultCpuModel;
                cpu.cpuidRead.stepping = defaultStepping;
                cpu.cpuidRead.source = cpuid::OVERWRITTEN;
                cpu.cpuidRead.cpuidCc = PECI_DEV_CC_SUCCESS;
                cpu.cpuidRead.cpuidRet = PECI_CC_SUCCESS;
                cpu.cpuidRead.cpuidValid = true;
            }
            else
            {
                cpu.cpuidRead.source = cpuid::INVALID;
            }
        }
    }
}
static void initCPUInfo(std::vector<CPUInfo>& cpuInfo)
{
    cpuInfo.reserve(MAX_CPUS);
    getClientAddrs(cpuInfo);
    for (CPUInfo& cpu : cpuInfo)
    {
        cpu.coreMaskRead.coreMaskValid = false;
        cpu.chaCountRead.chaCountValid = false;
        cpu.cpuidRead.cpuidValid = false;
        cpu.coreMaskRead.source = cpuid::INVALID;
        cpu.chaCountRead.source = cpuid::INVALID;
    }
}

static void getCPUData(std::vector<CPUInfo>& cpuInfo,
                       cpuid::cpuidState cpuState)
{
    for (CPUInfo& cpu : cpuInfo)
    {
        if (!cpu.cpuidRead.cpuidValid ||
            (cpu.cpuidRead.source == cpuid::OVERWRITTEN))
        {
            getCPUID(cpu);
            parseCPUInfo(cpu, cpuState);
        }
    }
    if (cpuState == cpuid::EVENT)
    {
        overwriteCPUInfo(cpuInfo);
    }
    getCoreMasks(cpuInfo, cpuState);
    getCHACounts(cpuInfo, cpuState);
    getUPIDisable(cpuInfo);
}

static std::string newTimestamp(void)
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

static void logTimestamp(cJSON* parent, std::string& logTime)
{
    cJSON_AddStringToObject(parent, "timestamp", logTime.c_str());
}

static void logTriggerType(cJSON* parent, const std::string& triggerType)
{
    cJSON_AddStringToObject(parent, "trigger_type", triggerType.c_str());
}

static void logPlatformName(cJSON* parent)
{
    cJSON_AddStringToObject(parent, "platform_name", getUuid().c_str());
}

static void logMetaDataCommon(cJSON* parent, std::string& logTime,
                              const std::string& triggerType)
{
    logTimestamp(parent, logTime);
    logTriggerType(parent, triggerType);
    logPlatformName(parent);
}

static cJSON*
    addSectionLog(cJSON* parent, crashdump::CPUInfo& cpuInfo,
                  std::string sectionName,
                  const std::function<int(crashdump::CPUInfo& cpuInfo, cJSON*)>
                      sectionLogFunc)
{
    CRASHDUMP_PRINT(INFO, stderr, "Logging %s on PECI address %d\n",
                    sectionName.c_str(), cpuInfo.clientAddr);

    // Create an empty JSON object for this section if it doesn't already
    // exist
    cJSON* logSectionJson;
    if ((logSectionJson = cJSON_GetObjectItemCaseSensitive(
             parent, sectionName.c_str())) == NULL)
    {
        cJSON_AddItemToObject(parent, sectionName.c_str(),
                              logSectionJson = cJSON_CreateObject());
    }

    // Get the log for this section
    int ret = 0;
    if ((ret = sectionLogFunc(cpuInfo, logSectionJson)) != 0)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Error %d during %s log\n", ret,
                        sectionName.c_str());
    }

    // Check if child data is added to the JSON section
    if (logSectionJson->child == NULL)
    {
        // If there was supposed to be child data, add a failed status
        if (ret != 0)
        {
            cJSON_AddStringToObject(logSectionJson,
                                    crashdump::dbgStatusItemName,
                                    crashdump::dbgFailedStatus);
        }
    }
    return logSectionJson;
}

static cJSON* addDisableSection(cJSON* parent, std::string sectionName)
{
    cJSON* logSection = NULL;
    cJSON* logSectionJson = NULL;
    logSection = cJSON_GetObjectItemCaseSensitive(parent, sectionName.c_str());
    if (logSection == NULL)
    {
        cJSON_AddItemToObject(parent, sectionName.c_str(),
                              logSectionJson = cJSON_CreateObject());
        cJSON_AddBoolToObject(logSectionJson, RECORD_ENABLE, false);
    }
    return logSectionJson;
}

void fillSection(cJSON* cpu, CPUInfo& cpuInfo, CrashdumpSection sectionName,
                 const std::function<int(crashdump::CPUInfo& cpuInfo, cJSON*)>
                     sectionLogFunc,
                 timespec* sectionStart, char* timeStr)
{
    cJSON* logSection = NULL;

    if (CHECK_BIT(cpuInfo.sectionMask, sectionName.section))
    {
        updateCurrentSection(sectionName, cpuInfo);

        logSection =
            addSectionLog(cpu, cpuInfo, sectionName.name, sectionLogFunc);

        // Do not log uncore version version here
        if (sectionName.section != Section::UNCORE)
        {
            logCrashdumpVersion(logSection, cpuInfo, sectionName.record_type);
        }

        logRunTime(logSection, sectionStart, timeStr);
    }
    else
    {
        addDisableSection(cpu, sectionName.name);
    }
}

void fillSectionWithChild(
    cJSON* cpu, CPUInfo& cpuInfo, CrashdumpSection sectionName,
    const std::function<int(crashdump::CPUInfo& cpuInfo, cJSON*)>
        sectionLogFunc1,
    const std::function<int(crashdump::CPUInfo& cpuInfo, cJSON*)>
        sectionLogFunc2,
    timespec* sectionStart, char* timeStr)
{
    cJSON* logSection = NULL;

    if (CHECK_BIT(cpuInfo.sectionMask, sectionName.section))
    {
        updateCurrentSection(sectionName, cpuInfo);

        addSectionLog(cpu, cpuInfo, sectionName.name, sectionLogFunc1);
        logSection =
            addSectionLog(cpu, cpuInfo, sectionName.name, sectionLogFunc2);
        if (logSection)
        {
            logCrashdumpVersion(logSection, cpuInfo, sectionName.record_type);
            logRunTime(logSection, sectionStart, timeStr);
        }
    }
    else
    {
        addDisableSection(cpu, sectionName.name);
    }
}

void fillMetaDataCPU(cJSON* crashlogData, CPUInfo cpuInfo,
                     InputFileInfo* inputFileInfo)
{
    cJSON* logSection = NULL;
    if (CHECK_BIT(cpuInfo.sectionMask, crashdump::METADATA))
    {
        updateCurrentSection(sectionNames[Section::METADATA], cpuInfo);

        logSection =
            addSectionLog(crashlogData, cpuInfo, "METADATA", logSysInfo);

        if (!inputFileInfo->unique)
        {
            // Fill in Input File Info in METADATA cpu section
            logSysInfoInputfile(cpuInfo, logSection, inputFileInfo);
        }
    }
}

void logInputFileVersion(cJSON* root, CPUInfo cpuInfo,
                         InputFileInfo* inputFileInfo)
{
    cJSON* jsonVer = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(inputFileInfo->buffers[cpuInfo.model],
                                         "crash_data"),
        "Version");

    if ((jsonVer != NULL) && cJSON_IsString(jsonVer))
    {
        cJSON_AddStringToObject(root, "_input_file_ver", jsonVer->valuestring);
    }
    else
    {
        cJSON_AddStringToObject(root, "_input_file_ver", MD_NA);
    }
}

void fillMetaDataCommon(cJSON* metaData, CPUInfo cpuInfo,
                        InputFileInfo* inputFileInfo,
                        const std::string& triggerType, std::string& timestamp,
                        timespec* crashdumpStart, timespec* sectionStart,
                        char* timeStr)
{
    if (CHECK_BIT(cpuInfo.sectionMask, crashdump::METADATA))
    {
        updateCurrentSection(sectionNames[Section::METADATA], cpuInfo);

        // Fill in common System Info
        logSysInfoCommon(metaData);

        if (metaData != NULL)
        {
            logMetaDataCommon(metaData, timestamp, triggerType);
            logCrashdumpVersion(metaData, cpuInfo, record_type::metadata);
            logInputFileVersion(metaData, cpuInfo, inputFileInfo);
            if (inputFileInfo->unique)
            {
                logSysInfoInputfile(cpuInfo, metaData, inputFileInfo);
            }
            logRunTime(metaData, sectionStart, timeStr);
            logRunTime(metaData, crashdumpStart, "_total_time");
        }
    }
    else
    {
        cJSON_AddBoolToObject(metaData, RECORD_ENABLE, false);
    }
}

static void cleanupInputFiles(InputFileInfo* inputFileInfo)
{
    for (int i = 0; i < NUMBER_OF_CPU_MODELS; i++)
    {
        FREE(inputFileInfo->filenames[i]);
        cJSON_Delete(inputFileInfo->buffers[i]);
    }
}

static void iterateByCPU(std::vector<crashdump::CPUInfo>& cpuInfo,
                         cJSON* processors, cJSON* crashlogData,
                         timespec& crashdumpStart, timespec& sectionStart,
                         InputFileInfo& inputFileInfo)

{
    // Fill in the Crashdump data in the correct order (uncore to core) for
    // each CPU
    cJSON* cpu = NULL;

    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &crashdumpStart);

    char timeStr[] = "_time";
    for (size_t i = 0; i < NUMBER_OF_SECTIONS; i++)
    {
        for (size_t j = 0; j < cpuInfo.size(); j++)
        {
            // Create a section for this cpu
            char cpuString[8];
            cd_snprintf_s(cpuString, sizeof(cpuString), "cpu%d", j);

            cpu = cJSON_GetObjectItemCaseSensitive(processors, cpuString);

            if (cpu == NULL)
            {
                cJSON_AddItemToObject(processors, cpuString,
                                      cpu = cJSON_CreateObject());
            }
            if (cpu != NULL)
            {
                if (cpuInfo[j].cpuidRead.cpuidValid)
                {
                    switch ((int)sectionNames[i].section)
                    {
                        case BIG_CORE:
                            cpuInfo[j].launchDelay = calculateDelay(
                                &crashdumpStart,
                                getDelayFromInputFile(
                                    cpuInfo[j],
                                    sectionNames[Section::BIG_CORE].name));

                            fillSectionWithChild(
                                cpu, cpuInfo[j],
                                sectionNames[Section::BIG_CORE], logCrashdump,
                                logSqDump, &sectionStart, timeStr);

                            break;

                        case METADATA:
                            fillMetaDataCPU(crashlogData, cpuInfo[j],
                                            &inputFileInfo);
                            break;

                        case ADDRESS_MAP:
                            if (cpu::spr != cpuInfo[j].model)
                            {
                                fillSection(cpu, cpuInfo[j], sectionNames[i],
                                            sectionNames[i].fptr, &sectionStart,
                                            timeStr);
                            }
                            break;

                        case MCA:
                            fillSectionWithChild(
                                cpu, cpuInfo[j], sectionNames[i], logCoreMca,
                                logUncoreMca, &sectionStart, timeStr);
                            break;

                        default:
                            fillSection(cpu, cpuInfo[j], sectionNames[i],
                                        sectionNames[i].fptr, &sectionStart,
                                        timeStr);
                            break;
                    }
                }
            }
        }
    }
}

static void iterateBySections(std::vector<crashdump::CPUInfo>& cpuInfo,
                              cJSON* processors, cJSON* crashlogData,
                              timespec& crashdumpStart, timespec& sectionStart,
                              InputFileInfo& inputFileInfo)
{
    // Fill in the Crashdump data in the correct order (uncore to core) for
    // each CPU
    cJSON* cpu = NULL;

    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &crashdumpStart);

    char timeStr[] = "_time";
    for (size_t i = 0; i < cpuInfo.size(); i++)
    {
        // Create a section for this cpu
        char cpuString[8];
        cd_snprintf_s(cpuString, sizeof(cpuString), "cpu%d", i);
        cJSON_AddItemToObject(processors, cpuString,
                              cpu = cJSON_CreateObject());

        // Fill in the Core Crashdump
        if (cpuInfo[i].cpuidRead.cpuidValid)
        {
            fillSection(cpu, cpuInfo[i], sectionNames[Section::UNCORE],
                        logUncoreStatus, &sectionStart, timeStr);
            fillSection(cpu, cpuInfo[i], sectionNames[Section::TOR], logTorDump,
                        &sectionStart, timeStr);
            fillSection(cpu, cpuInfo[i], sectionNames[Section::PM_INFO],
                        logPowerManagement, &sectionStart, timeStr);
            fillSection(cpu, cpuInfo[i], sectionNames[Section::ADDRESS_MAP],
                        logAddressMap, &sectionStart, timeStr);
            if (cpuInfo[i].cpuidRead.cpuModel == skx)
            {
                fillSectionWithChild(cpu, cpuInfo[i],
                                     sectionNames[Section::MCA], logCoreMca,
                                     logUncoreMca, &sectionStart, timeStr);
                fillSectionWithChild(
                    cpu, cpuInfo[i], sectionNames[Section::BIG_CORE],
                    logCrashdump, logSqDump, &sectionStart, timeStr);
            }
            else
            {
                cpuInfo[i].launchDelay = calculateDelay(
                    &crashdumpStart,
                    getDelayFromInputFile(
                        cpuInfo[i], sectionNames[Section::BIG_CORE].name));

                fillSectionWithChild(
                    cpu, cpuInfo[i], sectionNames[Section::BIG_CORE],
                    logCrashdump, logSqDump, &sectionStart, timeStr);
                fillSectionWithChild(cpu, cpuInfo[i],
                                     sectionNames[Section::MCA], logCoreMca,
                                     logUncoreMca, &sectionStart, timeStr);
            }
            fillMetaDataCPU(crashlogData, cpuInfo[i], &inputFileInfo);
        }
    }
}

void createCrashdump(std::vector<crashdump::CPUInfo>& cpuInfo,
                     std::string& crashdumpContents,
                     const std::string& triggerType, std::string& timestamp,
                     bool isTelemetry)
{
    cJSON* root = NULL;
    cJSON* crashlogData = NULL;
    cJSON* metaData = NULL;
    cJSON* processors = NULL;
    char* out = NULL;
    InputFileInfo inputFileInfo = {
        .unique = true, .filenames = {NULL}, .buffers = {NULL}};

    // Clear any resets that happened before the crashdump collection started
    clearResetDetected();

    CRASHDUMP_PRINT(INFO, stderr, "Crashdump started...\n");
    crashdump::getCPUData(cpuInfo, cpuid::EVENT);
    crashdump::savePeciWake(cpuInfo);
    crashdump::setPeciWake(cpuInfo, ON);

    loadInputFiles(cpuInfo, &inputFileInfo, isTelemetry);

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

    // Include the version field
    logCrashdumpVersion(processors, cpuInfo[0], record_type::bmcAutonomous);

    struct timespec sectionStart, crashdumpStart;

    if ((crashdump::cpu::icx == cpuInfo[0].model) ||
        (crashdump::cpu::icx2 == cpuInfo[0].model) ||
        (crashdump::cpu::icxd == cpuInfo[0].model) ||
        (crashdump::cpu::spr == cpuInfo[0].model))
    {
        iterateByCPU(cpuInfo, processors, crashlogData, crashdumpStart,
                     sectionStart, inputFileInfo);
    }
    else
    {
        iterateBySections(cpuInfo, processors, crashlogData, crashdumpStart,
                          sectionStart, inputFileInfo);
    }

    char timeStr[] = "_time";
    fillMetaDataCommon(metaData, cpuInfo[0], &inputFileInfo, triggerType,
                       timestamp, &crashdumpStart, &sectionStart, timeStr);

    logResetDetected(metaData, platformState.resetCpu,
                     platformState.resetSection);

#ifdef OEMDATA_SECTION
    // OEM Customer Section
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    crashdumpStart.tv_sec = sectionStart.tv_sec;
    crashdumpStart.tv_nsec = sectionStart.tv_nsec;
    cJSON* oemCrashlogData = NULL;
    cJSON* oemProcessors = NULL;
    cJSON* oemCpu = NULL;
    cJSON* oemLogSection = NULL;
    cJSON_AddItemToObject(root, "oemdata",
                          oemCrashlogData = cJSON_CreateObject());
    cJSON_AddItemToObject(oemCrashlogData, "PROCESSORS",
                          oemProcessors = cJSON_CreateObject());
    logCrashdumpVersion(oemProcessors, cpuInfo[0], record_type::bmcAutonomous);

    for (size_t i = 0; i < cpuInfo.size(); i++)
    {
        char cpuString[8];
        cd_snprintf_s(cpuString, sizeof(cpuString), "cpu%d", i);
        cJSON_AddItemToObject(oemProcessors, cpuString,
                              oemCpu = cJSON_CreateObject());
        oemLogSection =
            addSectionLog(oemCpu, cpuInfo[i], "oemdata", logOemData);
        if (oemLogSection)
        {
            logRunTime(oemLogSection, &sectionStart, timeStr);
        }
    }
    if (oemLogSection != NULL)
    {
        logTimestamp(oemCrashlogData, timestamp);
        logRunTime(oemCrashlogData, &crashdumpStart, "_total_time");
    }
#endif
#ifdef CRASHDUMP_PRINT_UNFORMATTED
    out = cJSON_PrintUnformatted(root);
#else
    out = cJSON_Print(root);
#endif
    if (out != NULL)
    {
        crashdumpContents = out;
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
    for (size_t i = 0; i < cpuInfo.size(); i++)
    {
        cpuInfo[i].crashedCoreMask = 0;
    }

    cleanupInputFiles(&inputFileInfo);
    cJSON_Delete(root);
    crashdump::checkPeciWake(cpuInfo);
    crashdump::setPeciWake(cpuInfo, OFF);
}

static int scandir_filter(const struct dirent* dirEntry)
{
    // Filter for just the crashdump files
    return (strncmp(dirEntry->d_name, crashdumpPrefix.c_str(),
                    crashdumpPrefix.size()) == 0);
}

#ifdef USE_SYSTEMD
static void dbusRemoveOnDemandLog()
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

static void dbusRemoveTelemetryLog()
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

static void dbusAddLog(const std::string& logContents,
                       const std::string& timestamp,
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

static void newOnDemandLog(std::vector<crashdump::CPUInfo>& cpuInfo,
                           std::string& onDemandLogContents,
                           std::string& timestamp)
{
    // Start the log to the on-demand location
    createCrashdump(cpuInfo, onDemandLogContents, triggerTypeOnDemand,
                    timestamp, false);
}

static void newTelemetryLog(std::vector<crashdump::CPUInfo>& cpuInfo,
                            std::string& telemetryLogContents,
                            std::string& timestamp)
{
    // Start the log to the telemetry location
    createCrashdump(cpuInfo, telemetryLogContents, triggerTypeOnDemand,
                    timestamp, true);
}

static void incrementCrashdumpCount()
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

constexpr int numStoredLogs = 3;
static void dbusAddStoredLog(const std::string& storedLogContents,
                             const std::string& timestamp)
{
    constexpr char const* crashdumpFile = "crashdump_%llu";
    uint64_t crashdump_num = 0;
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

    // Get the number for this crashdump. The crashdump_num is not
    // kept as a static in order to cover crashdump service
    // restarts. In that case it is undesirable to restart the number
    // at zero.
    if (numLogFiles > 0)
    {
        // otherwise, get the number of the last log and increment it
        sscanf_s(namelist[numLogFiles - 1]->d_name, crashdumpFile,
                 &crashdump_num);
        crashdump_num++;
    }

    // In case multiple crashdumps are triggered for the same error, the policy
    // is to keep the first log until it is manually cleared and rotate through
    // additional logs.  This guarantees that we have the first and last log of
    // a failure.
    for (int i = 0; i < numLogFiles; i++)
    {
        // Except for log 0, if it's below the number of saved logs, delete it
        if ((i != 0) && (i <= (numLogFiles - (numStoredLogs - 1))))
        {
            std::error_code ec;
            if (!(std::filesystem::remove(crashdumpDir / namelist[i]->d_name,
                                          ec)))
            {
                CRASHDUMP_PRINT(ERR, stderr, "failed to remove %s: %s\n",
                                namelist[i]->d_name, ec.message().c_str());
            }
#ifdef USE_SYSTEMD
            // Now remove the interface for the deleted log
            auto eraseit =
                std::find_if(storedLogIfaces.begin(), storedLogIfaces.end(),
                             [&namelist, &i](auto& log) {
                                 std::string& storedName = std::get<0>(log);
                                 return (storedName == namelist[1]->d_name);
                             });

            if (eraseit != std::end(storedLogIfaces))
            {
                crashdump::server->remove_interface(std::get<1>(*eraseit));
                storedLogIfaces.erase(eraseit);
            }
#endif
        }
        free(namelist[i]);
    }
    free(namelist);

    // Create the new crashdump filename
    std::string new_logfile_name = crashdumpPrefix +
                                   std::to_string(crashdump_num) + "-" +
                                   timestamp + ".json";
    std::filesystem::path out_file = crashdumpDir / new_logfile_name;

    // open the JSON file to write the crashdump contents
    fpJson = fopen(out_file.c_str(), "w");
    if (fpJson != NULL)
    {
        fprintf(fpJson, "%s", storedLogContents.c_str());
        fclose(fpJson);
    }

#ifdef USE_SYSTEMD
    // Add the new interface for this log
    std::filesystem::path path =
        std::filesystem::path(crashdumpPath) / std::to_string(crashdump_num);
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

static void newStoredLog(std::vector<crashdump::CPUInfo>& cpuInfo,
                         std::string& storedLogContents,
                         const std::string& triggerType, std::string& timestamp)
{
    // Start the log
    createCrashdump(cpuInfo, storedLogContents, triggerType, timestamp, false);
}

static bool isPECIAvailable()
{
    std::vector<CPUInfo> cpuInfo;
    getClientAddrs(cpuInfo);
    if (cpuInfo.empty())
    {
        CRASHDUMP_PRINT(ERR, stderr, "PECI is not available!\n");
        return false;
    }
    return true;
}

#ifdef USE_SYSTEMD
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
};
#endif
} // namespace crashdump

int main(int argc, char* argv[])
{
#ifndef USE_SYSTEMD
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
        fprintf(stderr, "PECI not available\n");
    }
    crashdump::initCPUInfo(crashdump::cpuInfo);
    crashdump::getCPUData(crashdump::cpuInfo, cpuid::STARTUP);

    std::string storedLogContents;
    std::string storedLogTime = crashdump::newTimestamp();
    crashdump::newStoredLog(crashdump::cpuInfo, storedLogContents,
                            triggerType, storedLogTime);
    if (storedLogContents.empty())
    {
        // Log is empty, so don't save it
        fprintf(stderr, "Log is empty, so don't save it\n");
        return -1;
    }

    crashdump::dbusAddStoredLog(storedLogContents, storedLogTime);
#else
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

    if (!crashdump::isPECIAvailable())
    {
        CRASHDUMP_PRINT(ERR, stderr, "PECI not available\n");
    }
    crashdump::initCPUInfo(crashdump::cpuInfo);
    crashdump::getCPUData(crashdump::cpuInfo, cpuid::STARTUP);

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
            future = std::async(std::launch::async, [triggerType,
                                                     ifaceStored]() {
                std::string storedLogContents;
                std::string storedLogTime = crashdump::newTimestamp();
                crashdump::newStoredLog(crashdump::cpuInfo, storedLogContents,
                                        triggerType, storedLogTime);

                // Signal that Crashdump is complete
                try
                {
                    sdbusplus::message::message msg =
                        ifaceStored->new_signal("CrashdumpComplete");

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
            crashdump::newOnDemandLog(crashdump::cpuInfo, onDemandLogContents,
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
            crashdump::newTelemetryLog(crashdump::cpuInfo, telemetryLogContents,
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

    // Send Raw PECI Interface
    std::shared_ptr<sdbusplus::asio::dbus_interface> ifaceRawPeci =
        crashdump::server->add_interface(crashdump::crashdumpPath,
                                         crashdump::crashdumpRawPeciInterface);

    // Send a Raw PECI command
    ifaceRawPeci->register_method(
        "SendRawPeci", [](const std::vector<std::vector<uint8_t>>& rawCmds) {
            std::vector<std::vector<uint8_t>> rawResp;
            for (auto const& rawCmd : rawCmds)
            {
                if (rawCmd.size() < 3)
                {
                    throw std::invalid_argument("Command Length too short");
                }
                std::vector<uint8_t> resp(rawCmd[2]);
                EPECIStatus rc = peci_raw(rawCmd[0], rawCmd[2], &rawCmd[3],
                                          rawCmd[1], resp.data(), resp.size());
                if (rc == PECI_CC_SUCCESS || rc == PECI_CC_TIMEOUT)
                {
                    rawResp.push_back(resp);
                }
                else
                {
                    rawResp.push_back({0});
                }
            }
            return rawResp;
        });
    ifaceRawPeci->initialize();

    // Start tracking host state
    std::shared_ptr<sdbusplus::bus::match::match> hostStateMonitor =
        crashdump::startHostStateMonitor(crashdump::conn);

    crashdump::io.run();
#endif

    return 0;
}
