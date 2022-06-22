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

#pragma once

#include <linux/peci-ioctl.h>
#include <peci.h>

#include <array>
#include <boost/asio/io_service.hpp>
#include <filesystem>
#ifndef NO_SYSTEMD
#include <sdbusplus/asio/object_server.hpp>
#endif
#include <vector>

extern "C" {
#include <cjson/cJSON.h>

#include "engine/crashdump.h"
#include "engine/utils.h"
#include "safe_str_lib.h"
}

#include "utils_dbusplus.hpp"


namespace crashdump
{
constexpr char const* dbgStatusItemName = "status";
constexpr const char* dbgFailedStatus = "N/A";

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
constexpr int numStoredLogs = 3;

#ifndef NO_SYSTEMD
static boost::asio::io_service io;
static std::shared_ptr<sdbusplus::asio::connection> conn;
static std::shared_ptr<sdbusplus::asio::object_server> server;
static std::vector<
    std::pair<std::string, std::shared_ptr<sdbusplus::asio::dbus_interface>>>
    storedLogIfaces;
#endif

const std::filesystem::path crashdumpDir = "/tmp/crashdump/output";
const std::string crashdumpFileRoot{"crashdump_ondemand_"};
const std::string crashdumpTelemetryFileRoot{"telemetry_"};
const std::string crashdumpPrefix{"crashdump_"};
void getCPUData(std::vector<CPUInfo>& cpuInfo, cpuidState cpuState);
void getClientAddrs(std::vector<CPUInfo>& cpuInfo);
std::string newTimestamp(void);
void initCPUInfo(std::vector<CPUInfo>& cpuInfo);
int scandir_filter(const struct dirent* dirEntry);
void dbusRemoveOnDemandLog();
void dbusRemoveTelemetryLog();
void dbusAddLog(const std::string& logContents, const std::string& timestamp,
                const std::string& dbusPath, const std::string& filename);
void newOnDemandLog(std::vector<CPUInfo>& cpuInfo,
                    std::string& onDemandLogContents, std::string& timestamp);
void newTelemetryLog(std::vector<CPUInfo>& cpuInfo,
                     std::string& telemetryLogContents, std::string& timestamp);
void incrementCrashdumpCount();
void dbusAddStoredLog(const std::string& storedLogContents,
                      const std::string& timestamp);
void newStoredLog(std::vector<CPUInfo>& cpuInfo, std::string& storedLogContents,
                  const std::string& triggerType, std::string& timestamp);
bool isPECIAvailable();
void setResetDetected();
acdStatus loadInputFiles(std::vector<CPUInfo>& cpuInfo,
                         InputFileInfo* inputFileInfo, bool isTelemetry);
void createCrashdump(std::vector<CPUInfo>& cpuInfo,
                     std::string& crashdumpContents,
                     const std::string& triggerType, std::string& timestamp,
                     bool isTelemetry);
std::string newTimestamp(void);
} // namespace crashdump
