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
#include <boost/asio/steady_timer.hpp>
#include <filesystem>
#include <sdbusplus/asio/object_server.hpp>
#include <vector>

extern "C" {
#include <cjson/cJSON.h>

#include "engine/crashdump.h"
#include "engine/utils.h"
#include "safe_str_lib.h"
}

#include "utils_common_dbusplus.hpp"

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
constexpr int numStoredLogs = 3;

static boost::asio::io_service io;
static std::shared_ptr<sdbusplus::asio::connection> conn;
static std::shared_ptr<sdbusplus::asio::object_server> server;
static std::vector<
    std::pair<std::string, std::shared_ptr<sdbusplus::asio::dbus_interface>>>
    storedLogIfaces;

const std::filesystem::path crashdumpDir = "/tmp/crashdump/output";
const std::string crashdumpFileRoot{"crashdump_ondemand_"};
const std::string crashdumpTelemetryFileRoot{"telemetry_"};
const std::string crashdumpPrefix{"crashdump_"};
int scandir_filter(const struct dirent* dirEntry);
void dbusRemoveOnDemandLog();
void dbusRemoveTelemetryLog();
void dbusAddLog(char** logContents, const std::string& timestamp,
                const std::string& dbusPath, const std::string& filename);
void newOnDemandLog(CPUInfo* cpus, char** onDemandLogContents,
                    std::string& timestamp);
void newTelemetryLog(CPUInfo* cpus, char** telemetryLogContents,
                     std::string& timestamp);
void incrementCrashdumpCount();
void dbusAddStoredLog(const std::string& storedLogContents,
                      const std::string& timestamp,
                      char const* crashdumpStatusStr, char const* modifier);
void newStoredLog(CPUInfo* cpus, std::string& storedLogContents,
                  const std::string& triggerType, std::string& timestamp);
bool isPECIAvailable();
void sendSdBusSignal(char const* signalName,
                     std::shared_ptr<sdbusplus::asio::dbus_interface> ifaceLog);
#ifndef BUILD_MAIN
extern int sectionFailCount;
#endif
} // namespace crashdump
