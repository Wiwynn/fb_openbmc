/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2024 Intel Corporation.
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

#include "crashdump.h"
#include "utils.h"

static void createOutputFile(const char* filename, const char* outputContents)
{
    FILE* file = fopen(filename, "w");
    if (file == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Failed to open file\n");
        return;
    }
    fprintf(file, "%s\n", outputContents);
    fclose(file);
}

static void createCrashdumpOutputFileWithTimeStamp(const char* outputContents)
{
    // Create output file
    char filename[MAX_FILENAME_LEN];
    char timestamp[MAX_TIMESTAMP_LEN];
    newTimestamp(timestamp, sizeof(timestamp));
    cd_snprintf_s(filename, sizeof(filename), "crashdump_%s.json", timestamp);
    createOutputFile(filename, outputContents);
    CRASHDUMP_PRINT(INFO, stderr, "\nFile saved: %s\n\n", filename);
}

int exec_main(int enable_i3c)
{
    // peci-i3c is currently an experimental feature
    if (enable_i3c)
    {
        CRASHDUMP_PRINT(INFO, stderr, "Setting to /dev/peci-i3c.\n");
        peci_SetDevName("/dev/peci-i3c");
    }

    // Variables Initialization
    acdStatus status;
    CPUInfo cpus[MAX_CPUS];

    errno_t err;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        err = memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        if (err != EOK)
        {
            CRASHDUMP_PRINT(INFO, stderr, "Fail to run memset_s!\n");
            return ACD_FAILURE;
        }
    }

    // Initializing CPUs
    CRASHDUMP_PRINT(INFO, stderr, "Initializing crashdump...\n");

    // Setup peci address and device name
    /* example of 1x2S configuration */
    int cpu0Addr = 0x30;
    int cpu1Addr = 0x30;
    setPECIAddressAndDeviceName(&cpus[0], cpu0Addr, "/dev/peci-0");
    setPECIAddressAndDeviceName(&cpus[1], cpu1Addr, "/dev/peci-1");

    /* an example of 2x4S configuration */
    /*
    int cpu0Addr = 0x30;
    int cpu1Addr = 0x31;
    setPECIAddressAndDeviceName(&cpus[0], cpu0Addr, "/dev/peci-0");
    setPECIAddressAndDeviceName(&cpus[1], cpu1Addr, "/dev/peci-1");
    setPECIAddressAndDeviceName(&cpus[2], cpu0Addr, "/dev/peci-2");
    setPECIAddressAndDeviceName(&cpus[3], cpu1Addr, "/dev/peci-3");
    setPECIAddressAndDeviceName(&cpus[4], cpu0Addr, "/dev/peci-0");
    setPECIAddressAndDeviceName(&cpus[5], cpu1Addr, "/dev/peci-1");
    setPECIAddressAndDeviceName(&cpus[6], cpu0Addr, "/dev/peci-2");
    setPECIAddressAndDeviceName(&cpus[7], cpu1Addr, "/dev/peci-3");
     */

    status = initCPUs(cpus, NULL);
    if (status != ACD_SUCCESS)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Error initializing CPUs Info\n");
    }

    // Create an instance of CrashdumpContext and initialize it
    CrashdumpContext context = {.triggerType = "On-Demand",
                                .timestamp = "",
                                .outputContents = NULL,
                                .root = NULL,
                                .inputFileInfo = {.unique = true,
                                                  .filenames = {NULL},
                                                  .buffers = {NULL}},
                                .sectionFailCount = 0,
                                .isTelemetry = false};

    // Setup default & override paths
    err |= strcpy_s(context.inputFileInfo.defaultPath, MAX_PATH_LEN,
                    "/usr/share/crashdump/input/");
    err |= strcpy_s(context.inputFileInfo.overridePath, MAX_PATH_LEN,
                    "/tmp/crashdump/input/");
    if (err != EOK)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Failed to setup default & override paths: (%s)\n",
                        strerror(err));
        return ACD_FAILURE;
    }

    // Collect CPUs debug data
    CRASHDUMP_PRINT(INFO, stderr, "Crashdump version: %s\n", CRASHDUMP_VER);
    newTimestamp(context.timestamp, sizeof(context.timestamp));
    status = createCrashdumpContents(cpus, &context);
    if (status != ACD_SUCCESS)
    {
        CRASHDUMP_PRINT(INFO, stderr,
                        "Error Creating Crashdump Contents (%d)\n", status);
    }

    // Store output in format of "crashdump_[timestamp].json"
    // Notes: Use createOutputFile() for custom path and filename
    createCrashdumpOutputFileWithTimeStamp(context.outputContents);

    // Memory cleanup
    deinitCPUsAndContext(cpus, &context);

    return status;
}

int main(int argc, char* argv[])
{
    // Notes: I3C is currently an experimental feature
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
