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
 *****************************************************************************/

#ifndef MODULE_EXAMPLE_H
#define MODULE_EXAMPLE_H
#include <stdbool.h>
#include <stdint.h>

#define JSON_VAL_LEN 64
#define MAX_INPUT_ARG_COUNT 8
#define MAX_NODE_LEVELS 4

typedef enum
{
    VERSION_1 = 1,
} rmVersion;

typedef struct
{
    int intArg[MAX_INPUT_ARG_COUNT];
    int intCount;

    bool boolArg[MAX_INPUT_ARG_COUNT];
    int boolCount;

    char* strArg[MAX_INPUT_ARG_COUNT];
    int strCount;

    int iteration;
} RemoteModuleInfo;

typedef struct
{
    rmVersion apiVersion;
    int executionCount;
} RemoteModuleConfig;

typedef struct
{
    char* nodeLevel[MAX_NODE_LEVELS];
    char* jsonString;
} RemoteModuleResponse;

typedef enum
{
    STATUS_SUCCESS,

} rmStatus;

#endif // MODULE_EXAMPLE_H
