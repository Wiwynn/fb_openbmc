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

#ifndef CONSOLELOG_H
#define CONSOLELOG_H

#include "crashdump.h"

#define CONSOLE_MAX_LINE_COUNT 500
#define CONSOLE_MAX_STRING_LEN 256
#define CONSOLE_LOG_TIME_LEN 64
#define CONSOLE_KEY "journal"

typedef struct
{
    cJSON* rootObject;
    uint32_t lineCount;
    char lineStr[CONSOLE_MAX_STRING_LEN];
} consoleInfo;

void initConsoleLog();
void deinitConsoleLog();
void consoleLog(uint32_t level, FILE* file, const char* fmt, ...);
extern consoleInfo console;

#endif // CONSOLELOG.H
