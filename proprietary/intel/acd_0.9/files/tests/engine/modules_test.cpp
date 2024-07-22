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
#include "../mock/test_crashdump.hpp"

extern "C" {
#include "engine/modules.h"
#include "engine/utils.h"
}

#include "tests/test_utils.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#define MODULE_EXAMPLE "./libmoduleExample.so";

#ifdef REMOTE_MODULE_EXAMPLE
// #define DEBUG_FLAG

TEST(ModuleTestFixture, loadModule_valid)
{
    char* name = MODULE_EXAMPLE;
    void* handle = NULL;
    acdStatus result = ACD_SUCCESS;

    result = loadModule(&handle, name);
    EXPECT_TRUE(result == ACD_SUCCESS);
    EXPECT_TRUE(handle != NULL);
    unloadModule(handle);
}

TEST(ModuleTestFixture, loadModule_not_found)
{
    char* name = "./wrongModule.so";
    void* handle = NULL;
    acdStatus result = ACD_SUCCESS;

    result = loadModule(&handle, name);
    EXPECT_TRUE(result != ACD_SUCCESS);
    EXPECT_TRUE(handle == NULL);
    unloadModule(handle);
}

TEST(ModuleTestFixture, validateModule_valid)
{
    char* name = MODULE_EXAMPLE;
    void* handle = NULL;
    acdStatus result = ACD_SUCCESS;

    result = loadModule(&handle, name);
    EXPECT_TRUE(result == ACD_SUCCESS);
    EXPECT_TRUE(handle != NULL);

    result = validateModule(handle);
    EXPECT_TRUE(result == ACD_SUCCESS);
    unloadModule(handle);
}

TEST(ModuleTestFixture, validate_api_flow)
{
    char* name = MODULE_EXAMPLE;
    void* handle = NULL;
    acdStatus result = ACD_SUCCESS;
    RemoteModuleInfo remoteModuleInfo = {};
    RemoteModuleConfig remoteModuleConfig = {};
    RemoteModuleResponse remoteModuleResponse = {};

    result = loadModule(&handle, name);
    EXPECT_TRUE(result == ACD_SUCCESS);
    EXPECT_TRUE(handle != NULL);

    result = validateModule(handle);
    EXPECT_TRUE(result == ACD_SUCCESS);

    remoteModuleInfo.iteration = 0;
    result = moduleCallInit(&remoteModuleInfo);

    result = moduleCallConfig(&remoteModuleInfo, &remoteModuleConfig);
    EXPECT_TRUE(result == ACD_SUCCESS);
    EXPECT_TRUE(remoteModuleConfig.apiVersion == VERSION_1);
    EXPECT_TRUE(remoteModuleConfig.executionCount == 100);

    // Simple execution loop
    for (int i = 1; i <= 20; i++)
    {
        CRASHDUMP_PRINT(ERR, stderr, "\nExecution Loop: iteration %d\n", i);
        result = moduleCallExecute(&remoteModuleInfo, &remoteModuleResponse);
        EXPECT_TRUE(result == ACD_SUCCESS);
        EXPECT_TRUE(remoteModuleResponse.jsonString != NULL);
        CRASHDUMP_PRINT(ERR, stderr, "Received node levels: %s, %s, %s, %s\n",
                        remoteModuleResponse.nodeLevel[0],
                        remoteModuleResponse.nodeLevel[1],
                        remoteModuleResponse.nodeLevel[2],
                        remoteModuleResponse.nodeLevel[3]);
        CRASHDUMP_PRINT(ERR, stderr, "Received value: %s\n",
                        remoteModuleResponse.jsonString);
    }
}
#endif
