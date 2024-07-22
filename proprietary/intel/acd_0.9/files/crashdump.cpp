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
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <peci.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <systemd/sd-id128.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <future>
#include <regex>
#include <sstream>
#include <vector>

extern "C" {
#include <cjson/cJSON.h>

#include "engine/BigCore.h"
#include "engine/TorDump.h"
#include "engine/cmdprocessor.h"
#include "engine/crashdump.h"
#include "engine/flow.h"
#include "engine/inputparser.h"
#include "engine/logger.h"
#include "engine/tpmi.h"
#include "engine/validator.h"
#include "safe_mem_lib.h"
#include "safe_str_lib.h"
}

#include "crashdump.hpp"
#include "utils_triage.hpp"

#ifdef PMT_NAC_CRASHLOG
#include "pmt_crashlog.hpp"
#endif

namespace crashdump
{
static CPUInfo cpus[MAX_CPUS];
static std::shared_ptr<sdbusplus::asio::dbus_interface> logIface;
constexpr char triggerTypeOnDemand[MAX_TRIGGER_STR_LEN] = "On-Demand";
int sectionFailCount = 0;
static constexpr const int peciCheckInterval = 10;
static constexpr const int MAX_CHECK_INTERVALS(3 * 60 /
                                               peciCheckInterval); // 3 min

int scandir_filter(const struct dirent* dirEntry)
{
    // Filter for just the crashdump files
    return (strncmp(dirEntry->d_name, crashdumpPrefix.c_str(),
                    crashdumpPrefix.size()) == 0);
}

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

void dbusAddLog(char** logContents, const std::string& timestamp,
                const std::string& dbusPath, std::string& filename)
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
        fprintf(fpJson, "%s", *logContents);
        fclose(fpJson);
    }
#ifdef USE_GZIP_COMPRESSION
    // Copy outfile name as lower levels calls don't take const char
    char outfile[MAX_FILENAME_LEN];
    strcpy_s(outfile, sizeof(outfile), out_file.c_str());

    compressOptions options;
    options.keepOriginal = false;
    options.inputFilePath = outfile;
    if (ACD_SUCCESS == CompressFile(options))
    {
        CRASHDUMP_PRINT(ERR, stderr, "Compressing output\n");
        // update out_file and append .gz extension
        filename += ".gz";
        out_file += ".gz";
    }

#endif
    logIface = server->add_interface(dbusPath, crashdumpInterface);
    logIface->register_property("Log", out_file.string());
    logIface->register_property("Timestamp", timestamp);
    logIface->register_property("Filename", filename);
    logIface->initialize();
}

void executeBAFI(char** contents, cJSON** root)
{
#ifdef BAFI_NDA_OUTPUT
    appendSummarySection(contents, root);
#endif
}
void executeTriage(char** contents, cJSON** root)
{
#ifdef TRIAGE_SECTION
    appendTriageSection(contents, root);
#endif
}

void newOnDemandLog(CPUInfo* cpus, char** onDemandLogContents,
                    std::string& timestamp)
{
    // Start the log to the on-demand location
    char timestampCStr[MAX_TIMESTAMP_LEN];
    strcpy_s(timestampCStr, sizeof(timestampCStr), timestamp.c_str());
    bool isTelemetry = false;
    bool bafiEnabled = false;
    bool triageEnabled = false;
    uint8_t bafiMaxNumSockets = 0;

    cJSON* root = NULL;
    InputFileInfo inputFileInfo = {.unique = true,
                                   .filenames = {NULL},
                                   .buffers = {NULL},
                                   .defaultPath = {0},
                                   .overridePath = {0}};
    errno_t rc = EOK;

    // Setup default & override paths
    rc |= strcpy_s(inputFileInfo.defaultPath, MAX_PATH_LEN, DEFAULT_INPUT_DIR);
    rc |=
        strcpy_s(inputFileInfo.overridePath, MAX_PATH_LEN, OVERRIDE_INPUT_DIR);
    if (rc != EOK)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Failed to setup default & override paths: (%s)\n",
                        strerror(rc));
    }
    createCrashdump(cpus, onDemandLogContents,
                    const_cast<char*>(triggerTypeOnDemand), timestampCStr,
                    isTelemetry, &sectionFailCount, &inputFileInfo, &root);

    if (root != NULL)
    {
        bafiEnabled = isBAFIEnabled(cpus[0].inputFile.bufferPtr, isTelemetry);
        triageEnabled =
            isTriageEnabled(cpus[0].inputFile.bufferPtr, isTelemetry);

        cleanupInputFiles(cpus, &inputFileInfo);

        timestamp = timestampCStr;
        if (*onDemandLogContents != NULL)
        {
            bafiMaxNumSockets =
                getMaxNumSocketsForBafi(inputFileInfo.buffers[0]);
            if (getNumberOfCpus(cpus) > bafiMaxNumSockets)
            {
                CRASHDUMP_PRINT(ERR, stderr,
                                "Number of sockets exceed max number of %d "
                                "sockets for BAFI\n",
                                bafiMaxNumSockets);
                bafiEnabled = false;
                triageEnabled = false;
            }

            if (bafiEnabled)
            {
                executeBAFI(onDemandLogContents, &root);
            }
            if (triageEnabled)
            {
                executeTriage(onDemandLogContents, &root);
            }
        }
        // Clean up root first to free up large memory usage
        CRASHDUMP_PRINT(INFO, stderr, "Cleaning ConsoleLog\n");
        deinitConsoleLog();

        CRASHDUMP_PRINT(INFO, stderr, "Cleaning root\n");
        cJSON_Delete(root);
        root = NULL;
    }
}

void newTelemetryLog(CPUInfo* cpus, char** telemetryLogContents,
                     std::string& timestamp)
{
    // Start the log to the telemetry location
    char timestampCStr[MAX_TIMESTAMP_LEN];
    strcpy_s(timestampCStr, sizeof(timestampCStr), timestamp.c_str());

    cJSON* root = NULL;
    bool isTelemetry = true;

    bool bafiEnabled = false;
    bool triageEnabled = false;
    uint8_t bafiMaxNumSockets = 0;
    InputFileInfo inputFileInfo = {.unique = true,
                                   .filenames = {NULL},
                                   .buffers = {NULL},
                                   .defaultPath = {0},
                                   .overridePath = {0}};
    errno_t rc = EOK;

    // Setup default & override paths
    rc |= strcpy_s(inputFileInfo.defaultPath, MAX_PATH_LEN, DEFAULT_INPUT_DIR);
    rc |=
        strcpy_s(inputFileInfo.overridePath, MAX_PATH_LEN, OVERRIDE_INPUT_DIR);
    if (rc != EOK)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Failed to setup default & override paths: (%s)\n",
                        strerror(rc));
    }
    createCrashdump(cpus, telemetryLogContents,
                    const_cast<char*>(triggerTypeOnDemand), timestampCStr,
                    isTelemetry, &sectionFailCount, &inputFileInfo, &root);

    if (root != NULL)
    {
        bafiEnabled = isBAFIEnabled(cpus[0].inputFile.bufferPtr, isTelemetry);
        triageEnabled =
            isTriageEnabled(cpus[0].inputFile.bufferPtr, isTelemetry);

        cleanupInputFiles(cpus, &inputFileInfo);

        timestamp = timestampCStr;
        if (*telemetryLogContents != NULL)
        {
            bafiMaxNumSockets =
                getMaxNumSocketsForBafi(inputFileInfo.buffers[0]);
            if (getNumberOfCpus(cpus) > bafiMaxNumSockets)
            {
                CRASHDUMP_PRINT(ERR, stderr,
                                "Number of sockets exceed max number of %d "
                                "sockets for BAFI\n",
                                bafiMaxNumSockets);
                bafiEnabled = false;
                triageEnabled = false;
            }

            if (bafiEnabled)
            {
                executeBAFI(telemetryLogContents, &root);
            }
            if (triageEnabled)
            {
                executeTriage(telemetryLogContents, &root);
            }
        }
        // Clean up root first to free up large memory usage
        CRASHDUMP_PRINT(INFO, stderr, "Cleaning ConsoleLog\n");
        deinitConsoleLog();

        CRASHDUMP_PRINT(INFO, stderr, "Cleaning root\n");
        cJSON_Delete(root);
        root = NULL;
    }
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

void dbusAddStoredLog(char** storedLogContents, const std::string& timestamp,
                      char const* crashdumpStatusStr, char const* modifier)
{
    constexpr char const* crashdumpFile = "crashdump_%" PRIu64 "";
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
        // Now remove the interface for the deleted log
        auto eraseit =
            std::find_if(storedLogIfaces.begin(), storedLogIfaces.end(),
                         [&filename](auto& log) {
                             std::string& storedName = std::get<0>(log);
                             return (storedName == filename);
                         });

        if (eraseit != std::end(storedLogIfaces))
        {
            auto iface = std::get<1>(*eraseit);
            // Signaling that a crashdump log was deleted as consequence
            // the max number of StoredLogs was reached.
            sendSdBusSignal("CrashdumpLogDeleted", iface);

            crashdump::server->remove_interface(iface);
            storedLogIfaces.erase(eraseit);
        }
    }

    // Create the new crashdump filename
    std::string new_logfile_name;
    if (modifier[0] == '\0')
    {
        new_logfile_name = crashdumpPrefix + std::to_string(crashdumpNum) +
                           "-" + timestamp + ".json";
    }
    else
    {
        char lowerCaseModifier[MAX_MODIFIER_LEN];
        strcpy_s(lowerCaseModifier, MAX_MODIFIER_LEN, modifier);
        errno_t err = strtolowercase_s(lowerCaseModifier, MAX_MODIFIER_LEN);
        if (err != 0)
        {
            CRASHDUMP_PRINT(ERR, stderr,
                            "Failed to append lowercase modifier %s\n",
                            modifier);
        }
        new_logfile_name = crashdumpPrefix + std::to_string(crashdumpNum) +
                           "-" + timestamp + "_" + lowerCaseModifier + ".json";
    }
    std::filesystem::path out_file = crashdumpDir / new_logfile_name;

    // open the JSON file to write the crashdump contents
    fpJson = fopen(out_file.c_str(), "w");
    if (fpJson != NULL)
    {
        fprintf(fpJson, "%s", *storedLogContents);
        fclose(fpJson);
    }

#ifdef USE_GZIP_COMPRESSION
    // Copy outfile name as lower levels calls don't take const char
    char outfile[MAX_FILENAME_LEN];
    strcpy_s(outfile, sizeof(outfile), out_file.c_str());

    compressOptions options;
    options.keepOriginal = false;
    options.inputFilePath = outfile;
    if (ACD_SUCCESS == CompressFile(options))
    {
        CRASHDUMP_PRINT(ERR, stderr, "Compressed output\n");
        // Update out_file and append .gz extension
        new_logfile_name += ".gz";
        out_file += ".gz";
    }

#endif

    // Make dbusAddStoredLog() partially unit-testable
#ifdef BUILD_MAIN
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
    ifaceLog->register_signal<void>("CrashdumpComplete");
    ifaceLog->register_signal<void>("CrashdumpFail");
    ifaceLog->register_signal<void>("CrashdumpLogDeleted");
    ifaceLog->initialize();

    // Signaling that the Crashdump log has been stored
    sendSdBusSignal(crashdumpStatusStr, ifaceLog);

    // Increment the count for this completed crashdump
    incrementCrashdumpCount();
#endif
}

void sendSdBusSignal(char const* signalName,
                     std::shared_ptr<sdbusplus::asio::dbus_interface> ifaceLog)
{
    try
    {
        sdbusplus::message_t msg = ifaceLog->new_signal(signalName);
        msg.signal_send();
    }
    catch (const sdbusplus::exception_t& e)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Failed to send sdbus signal %s\n",
                        signalName);
    }
}

void newStoredLog(CPUInfo* cpus, char** storedLogContents,
                  const std::string& triggerType, std::string& timestamp)
{
    // Start the log
    char timestampCStr[MAX_TIMESTAMP_LEN];
    char triggerTypeCStr[MAX_TRIGGER_STR_LEN];
    strcpy_s(timestampCStr, sizeof(timestampCStr), timestamp.c_str());
    strcpy_s(triggerTypeCStr, sizeof(triggerTypeCStr), triggerType.c_str());

    cJSON* root = NULL;
    bool bafiEnabled = false;
    bool triageEnabled = false;
    bool isTelemetry = false;
    uint8_t bafiMaxNumSockets = 0;
    InputFileInfo inputFileInfo = {.unique = true,
                                   .filenames = {NULL},
                                   .buffers = {NULL},
                                   .defaultPath = {0},
                                   .overridePath = {0}};
    errno_t rc = EOK;

    // Setup default & override paths
    rc |= strcpy_s(inputFileInfo.defaultPath, MAX_PATH_LEN, DEFAULT_INPUT_DIR);
    rc |=
        strcpy_s(inputFileInfo.overridePath, MAX_PATH_LEN, OVERRIDE_INPUT_DIR);
    if (rc != EOK)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Failed to setup default & override paths: (%s)\n",
                        strerror(rc));
    }

    createCrashdump(cpus, storedLogContents, triggerTypeCStr, timestampCStr,
                    isTelemetry, &sectionFailCount, &inputFileInfo, &root);

    if (root != NULL)
    {
        bafiEnabled = isBAFIEnabled(cpus[0].inputFile.bufferPtr, isTelemetry);
        triageEnabled =
            isTriageEnabled(cpus[0].inputFile.bufferPtr, isTelemetry);

        cleanupInputFiles(cpus, &inputFileInfo);

        timestamp = timestampCStr;
        if (*storedLogContents != NULL)
        {
            bafiMaxNumSockets =
                getMaxNumSocketsForBafi(inputFileInfo.buffers[0]);
            if (getNumberOfCpus(cpus) > bafiMaxNumSockets)
            {
                CRASHDUMP_PRINT(ERR, stderr,
                                "Number of sockets exceed max number of %d "
                                "sockets for BAFI\n",
                                bafiMaxNumSockets);
                bafiEnabled = false;
                triageEnabled = false;
            }

            if (bafiEnabled)
            {
                executeBAFI(storedLogContents, &root);
            }
            if (triageEnabled)
            {
                executeTriage(storedLogContents, &root);
            }
        }

        CRASHDUMP_PRINT(INFO, stderr, "Cleaning ConsoleLog\n");
        deinitConsoleLog();

        CRASHDUMP_PRINT(INFO, stderr, "Cleaning root\n");
        cJSON_Delete(root);
        root = NULL;
    }
}

bool isPECIAvailable()
{
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    getClientAddrs(cpus);
    return true;
}

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
} // namespace crashdump

static void signalHandler(int sig)
{
    CRASHDUMP_PRINT(INFO, stderr,
                    "Stopping crashdump and cleaning up memory (%d)...\n", sig);
    crashdump::io.stop();
    cleanupComputeDies(crashdump::cpus);
}

static acdStatus initCrashdump()
{
    CRASHDUMP_PRINT(INFO, stderr, "Initializing crashdump...\n");
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&crashdump::cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    acdStatus initStatus = initCPUInfo(crashdump::cpus);
    if (initStatus != ACD_SUCCESS)
    {
        return initStatus;
    }
    initStatus = getCPUData(crashdump::cpus, STARTUP);
    return initStatus;
}

static void peciAvailableCheck()
{
    static uint16_t checkCount = 0;
    static bool initComplete = false;
    static boost::asio::steady_timer peciWaitTimer(crashdump::io);
    // Check for peci for cpu info
    bool peciAvailable = crashdump::isPECIAvailable();
    // Check and try to retrieve phy2log tables
    acdStatus phy2logCoreValid =
        tpmi_getPHY2LOG(crashdump::cpus, STARTUP, CORE_TYPE);
    acdStatus phy2logCHAValid =
        tpmi_getPHY2LOG(crashdump::cpus, STARTUP, CHA_TYPE);

    checkCount++;

    // Init crashdump when peci becomes available
    if (peciAvailable && !initComplete)
    {
        initComplete = (initCrashdump() == ACD_SUCCESS);
    }

    if (phy2logCoreValid == ACD_SUCCESS)
    {
        CRASHDUMP_PRINT(INFO, stderr, "Phy2log Core initialize complete\n");
    }

    if (phy2logCHAValid == ACD_SUCCESS)
    {
        CRASHDUMP_PRINT(INFO, stderr, "Phy2log Cha initialize complete\n");
        return;
    }
    // Maximum wait of 3 minutes for Phy2log
    else if (checkCount > crashdump::MAX_CHECK_INTERVALS)
    {
        CRASHDUMP_PRINT(INFO, stderr, "Phy2log did not initialize\n");
        return;
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

int exec_main(int enable_i3c)
{
    // Print version
    CRASHDUMP_PRINT(INFO, stderr, "Crashdump version: %s\n", CRASHDUMP_VER);
    if (enable_i3c)
    {
        CRASHDUMP_PRINT(INFO, stderr, "Setting to /dev/peci-i3c.\n");
        peci_SetDevName("/dev/peci-i3c");
    }

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

    ifaceStored->register_signal<void>("CrashdumpAllDeleted");

    // NAC Crashlog discovery
#ifdef PMT_NAC_CRASHLOG
    PMTCrashlog pmtCrashLog(crashdump::conn);
    CRASHDUMP_PRINT(INFO, stderr, "Starting pmtCrashLog init...\n");
    pmtCrashLog.initPMTDiscovery(crashdump::io);
#endif

    peciAvailableCheck();

    // Generate a Stored Log
    ifaceStored->register_method("GenerateStoredLog", [&future, ifaceStored](
                                                          const std::string&
                                                              triggerType) {
        if (future.valid() && future.wait_for(std::chrono::seconds(0)) !=
                                  std::future_status::ready)
        {
            throw crashdump::LogInProgressException();
        }
        if (!crashdump::isPECIAvailable())
        {
            throw crashdump::PowerOffException();
        }
        future = std::async(std::launch::async, [triggerType, ifaceStored]() {
            constexpr char const* crashdumpCompleteStr = "CrashdumpComplete";
            constexpr char const* crashdumpFailStr = "CrashdumpFail";
            char* storedLogContents = NULL;
            char logTime[MAX_TIMESTAMP_LEN];
            newTimestamp(logTime, sizeof(logTime));
            std::string storedLogTime = logTime;
            crashdump::newStoredLog(crashdump::cpus, &storedLogContents,
                                    triggerType, storedLogTime);

            // Signal that Crashdump is complete or fail

            bool isCrashdumpFail = isCrashDumpFail(crashdump::sectionFailCount,
                                                   triggerType.c_str());
            char const* crashdumpStatusStr =
                isCrashdumpFail ? crashdumpFailStr : crashdumpCompleteStr;
            CRASHDUMP_PRINT(INFO, stderr, "Status: %s (%d)\n",
                            crashdumpStatusStr, crashdump::sectionFailCount);

            TriggerString triggerStr = {{0}, {0}};
            splitTriggerString(triggerType.c_str(), &triggerStr);

            if (storedLogContents == NULL)
            {
                // Log is empty, so don't save it
                return;
            }
            boost::asio::post(crashdump::io,
                              [storedLogContents = std::move(storedLogContents),
                               storedLogTime = std::move(storedLogTime),
                               crashdumpStatusStr, triggerStr]() mutable {
                                  crashdump::dbusAddStoredLog(
                                      &storedLogContents, storedLogTime,
                                      crashdumpStatusStr, triggerStr.modifier);
                                  cJSON_free(storedLogContents);
                                  storedLogContents = NULL;
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
    ifaceDeleteAll->register_method("DeleteAll", [ifaceStored]() {
        std::error_code ec;
        bool emitSignal = !crashdump::storedLogIfaces.empty();
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

        if (emitSignal)
        {
            // Signaling the deletion of All Crashdump logs found
            crashdump::sendSdBusSignal("CrashdumpAllDeleted", ifaceStored);
        }

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
            char* onDemandLogContents = NULL;
            char logTime[MAX_TIMESTAMP_LEN];
            newTimestamp(logTime, sizeof(logTime));
            std::string onDemandTimestamp = logTime;
            std::string filename =
                crashdump::crashdumpFileRoot + onDemandTimestamp + ".json";
            crashdump::newOnDemandLog(crashdump::cpus, &onDemandLogContents,
                                      onDemandTimestamp);

            boost::asio::post(
                crashdump::io,
                [onDemandLogContents = std::move(onDemandLogContents),
                 onDemandTimestamp = std::move(onDemandTimestamp),
                 filename]() mutable {
                    crashdump::dbusAddLog(
                        &onDemandLogContents, onDemandTimestamp,
                        crashdump::crashdumpOnDemandPath, filename);
                    cJSON_free(onDemandLogContents);
                    onDemandLogContents = NULL;
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
            char* telemetryLogContents = NULL;
            char logTime[MAX_TIMESTAMP_LEN];
            newTimestamp(logTime, sizeof(logTime));
            std::string telemetryTimestamp = logTime;
            std::string filename = crashdump::crashdumpTelemetryFileRoot +
                                   telemetryTimestamp + ".json";
            crashdump::newTelemetryLog(crashdump::cpus, &telemetryLogContents,
                                       telemetryTimestamp);

            boost::asio::post(
                crashdump::io,
                [telemetryLogContents = std::move(telemetryLogContents),
                 telemetryTimestamp = std::move(telemetryTimestamp),
                 filename]() mutable {
                    crashdump::dbusAddLog(
                        &telemetryLogContents, telemetryTimestamp,
                        crashdump::crashdumpTelemetryPath, filename);
                    cJSON_free(telemetryLogContents);
                    telemetryLogContents = NULL;
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
        crashdump::startHostStateMonitor(crashdump::conn,
                                         &crashdump::cpus[0].platformState);

    try
    {
        CRASHDUMP_PRINT(INFO, stderr, "crashdump io service starting...\n");
        crashdump::io.run();
    }
    catch (const boost::system::system_error& e)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Failed to run io\n");
    }

    CRASHDUMP_PRINT(INFO, stderr, "crashdump stopped successfully.\n");
    return ACD_SUCCESS;
}

#ifdef BUILD_MAIN
int main(int argc, char* argv[])
{
    int enable_i3c = 0;
    errno_t err;

    for (int i = 1; i < argc; i++)
    {
        int ind = 0;
        err = strcmp_s(argv[i], strnlen_s(argv[i], RSIZE_MAX_STR),
                       "--peci-i3c-enable", &ind);
        if (err == EOK && ind == 0)
        {
            enable_i3c = 1;
        }
    }
    return exec_main(enable_i3c);
}
#endif // BUILD_MAIN
