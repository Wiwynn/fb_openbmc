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

#include "consolelog.h"

#include "utils.h"

consoleInfo console = {};

acdStatus addConsoleLine(const char* line)
{
    char logEnumeration[8];

    if (console.rootObject == NULL)
    {
        return ACD_INVALID_OBJECT;
    }
    if (console.lineCount >= CONSOLE_MAX_LINE_COUNT)
    {
        return ACD_FAILURE;
    }
    // Key name
    cd_snprintf_s(logEnumeration, sizeof(logEnumeration), "log%d",
                  console.lineCount);
    console.lineCount++;

    // Line in value
    cJSON_AddItemToObject(console.rootObject, logEnumeration,
                          cJSON_CreateString(line));

    return ACD_SUCCESS;
}

void initConsoleLog()
{
    console.rootObject = cJSON_CreateObject();
    console.lineCount = 0;
}

void deinitConsoleLog()
{
    // Call deinit when the rootObject has been deleted by main root
    console.rootObject = NULL;
}

void consoleLog(uint32_t level, FILE* file, const char* fmt, ...)
{
    acdStatus result;
    struct timespec runTime;
    int strIdx;

    // Add timestamp to original line
    clock_gettime(CLOCK_MONOTONIC, &runTime);
    strIdx = strftime(console.lineStr, sizeof(console.lineStr), "%T ",
                      gmtime(&runTime.tv_sec));

    // Variable parameters start after fmt
    va_list vl;
    va_start(vl, fmt);
    int ret = vsnprintf_s(&console.lineStr[strIdx],
                          CONSOLE_MAX_STRING_LEN - strIdx, fmt, vl);
    if (ret < CD_SNPRINTF_S_INVALID)
    {
        JOURNAL_PRINT(stderr,
                      "consoleLog: Error writing formatted data to string %d\n",
                      ret);
    }
    va_end(vl);

    // Add to log cjson object
    result = addConsoleLine(console.lineStr);

    // Print if we couldn't add; keep uninitialized consolelog silent
    if (result == ACD_FAILURE)
    {
        // Output error to stream
        JOURNAL_PRINT(stderr, "ConsoleLog: Could not add line to log\n");
    }

    // Forward to file stream; No Timestamp
    va_start(vl, fmt);
    vfprintf(file, fmt, vl);
    va_end(vl);
}
