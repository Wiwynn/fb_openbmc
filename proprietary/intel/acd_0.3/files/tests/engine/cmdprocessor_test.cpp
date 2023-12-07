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
#include "../test_utils.hpp"

extern "C" {
#include <search.h>

#include "engine/cmdprocessor.h"
#include "engine/crashdump.h"
#include "engine/flow.h"
#include "engine/inputparser.h"
#include "engine/logger.h"
#include "engine/utils.h"
}

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace ::testing;
using ::testing::Return;
#include <fstream>

class ProcessCmdTestFixture : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        inputJsonStr = R"({
    "CrashDump_Discovery": [{"Params": [48, 1, 2, 3, 4, 8]}],
    "CrashDump_GetFrame": [{"Params": [48, 1, 2, 3, 8]}],
    "RdEndpointConfigMMIO": [{"Params": [48, 1, 2, 3, 4, 5, 6, 7, 8]}],
    "RdEndpointConfigPCILocal": [
        {"Params": [48, 1, 2, 3, 4, 5, 4]},
        {"Params": [48, 1, 2, 3, 4, 5, 8]}],
    "WrEndPointPCIConfigLocal": [{"Params": [48, 1, 2, 3, 4, 5, 6, 8]}],
    "RdIAMSR": [{"Params": [48, 1, 8]}],
    "RdPkgConfig": [{"Params": [48, 1, 2, 8]}],
    "Telemetry_Discovery": [{"Params": [48, 1, 2, 3, 4, 8]}],
    "CrashDump_Discovery_dom": [{"Params": [48, 1, 2, 3, 4, 5, 8]}],
    "CrashDump_GetFrame_dom": [{"Params": [48, 1, 2, 3, 4, 8]}],
    "RdEndpointConfigMMIO_dom": [{"Params": [48, 1, 2, 3, 4, 5, 6, 7, 8, 8]}],
    "RdEndpointConfigPCILocal_dom": [
        {"Params": [48, 1, 2, 3, 4, 5, 6, 4]},
        {"Params": [48, 1, 2, 3, 4, 5, 6, 8]}],
    "WrEndPointPCIConfigLocal_dom": [{"Params": [48, 1, 2, 3, 4, 5, 6, 7, 8]}],
    "RdIAMSR_dom": [{"Params": [48, 1, 2, 4]}],
    "RdPkgConfig_dom": [{"Params": [48, 1, 2, 3, 4]}],
    "Telemetry_Discovery_dom": [{"Params": [48, 1, 2, 3, 4, 5, 8]}],
    "RdAndConcatenate": [{"Params": [48, 8]}],
    "SaveStrVars": [{"Params": [8]}]}
    )";
    }

    void TearDown() override
    {
        FREE(jsonStr);
        cJSON_Delete(outRoot);
    }

  public:
    cJSON* outRoot = cJSON_CreateObject();
    char* jsonStr = NULL;
    uint8_t cc = 0;
    bool enable = false;
    acdStatus status;
    char* inputJsonStr;
    CPUInfo cpus[MAX_CPUS] = {};
};

TEST_F(ProcessCmdTestFixture, SimpleCmdPassFailTest)
{
    TestCrashdump crashdump(cd_spr);

    // Build RdIAMSR params in cJSON form
    cJSON* params = cJSON_CreateObject();
    int paramsRdIAMSR[] = {0x30, 0x0, 0x435};
    cJSON_AddItemToObject(params, "Params",
                          cJSON_CreateIntArray(paramsRdIAMSR, 3));
    uint8_t cc = PECI_DEV_CC_SUCCESS;
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR)
        .Times(1)
        .WillRepeatedly(DoAll(SetArgPointee<3>(0x1122334455667788),
                              SetArgPointee<4>(cc), Return(PECI_CC_SUCCESS)));

    ENTRY entry;
    CmdInOut cmdInOut = {};
    ValidatorParams validateInput = {};
    cmdInOut.validatorParams = &validateInput;
    cmdInOut.out.ret = PECI_CC_INVALID_REQ;
    cmdInOut.in.params = cJSON_GetObjectItem(params, "Params");
    cmdInOut.internalVarName = "UINT64Var";

    status = BuildCmdsTable(&entry);
    EXPECT_EQ(ACD_SUCCESS, status);

    entry.key = "RdIAMSR";
    cmdInOut.paramsTracker = cJSON_CreateObject();
    cmdInOut.internalVarsTracker = cJSON_CreateObject();
    CPUInfo* cpuInfo = cpus;
    status = Execute(&entry, &cmdInOut, cpuInfo);
    EXPECT_EQ(ACD_SUCCESS, status);
    EXPECT_EQ(cmdInOut.out.ret, PECI_CC_SUCCESS);
    EXPECT_EQ(cmdInOut.out.cc, cc);
    EXPECT_EQ((uint64_t)cmdInOut.out.val.u64, (uint64_t)0x1122334455667788);

    entry.key = "RdIAMSRInvalid";
    status = Execute(&entry, &cmdInOut, cpuInfo);
    EXPECT_EQ(ACD_INVALID_CMD, status);

    Logging(cmdInOut.internalVarsTracker, "internalVarsTracker:");
    cJSON* val = cJSON_GetObjectItem(cmdInOut.internalVarsTracker, "UINT64Var");
    EXPECT_STREQ(val->valuestring, "0x1122334455667788");
    hdestroy();
}

TEST_F(ProcessCmdTestFixture, APIs_Invalid_test)
{
    TestCrashdump crashdump(cd_gnr);

    ENTRY entry;
    CmdInOut cmdInOut = {};

    // Enable validator to check number of parameters
    ValidatorParams validateInput = {.validateInput = true};
    cmdInOut.validatorParams = &validateInput;

    status = BuildCmdsTable(&entry);
    EXPECT_EQ(ACD_SUCCESS, status);

    cJSON* root = cJSON_Parse(this->inputJsonStr);
    ASSERT_FALSE(root == NULL);

    // Inject empty Params
    cJSON* params;
    cJSON* param;
    cJSON_ArrayForEach(params, root)
    {
        cJSON_ArrayForEach(param, params)
        {
            cJSON_ReplaceItemInObject(param, "Params", cJSON_CreateArray());
        }
    }

    // Actual Test
    cJSON* peciCmd;
    cJSON* peciParams;
    CPUInfo* cpuInfo = cpus;

    cJSON_ArrayForEach(peciCmd, root)
    {
        entry.key = peciCmd->string;
        cJSON_ArrayForEach(peciParams, peciCmd)
        {
            cmdInOut.in.params = cJSON_GetObjectItem(peciParams, "Params");
            status = Execute(&entry, &cmdInOut, cpuInfo);
            EXPECT_NE(ACD_SUCCESS, status);
        }
    }

    hdestroy();
}

TEST_F(ProcessCmdTestFixture, APIs_Valid_test)
{
    TestCrashdump crashdump(cd_gnr);

    // Mock all PECI APIs
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigMmio_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
        .Times(3)
        .WillRepeatedly(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrEndPointPCIConfigLocal_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_Discovery_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigMmio)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
        .Times(3)
        .WillRepeatedly(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrEndPointPCIConfigLocal)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_Discovery)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));

    // Actual Test
    ENTRY entry;
    CmdInOut cmdInOut = {};

    // Enable validator to check number of parameters
    ValidatorParams validateInput = {.validateInput = true};
    cmdInOut.validatorParams = &validateInput;
    cmdInOut.out.ret = PECI_CC_INVALID_REQ;

    status = BuildCmdsTable(&entry);
    EXPECT_EQ(ACD_SUCCESS, status);

    cJSON* root = cJSON_Parse(this->inputJsonStr);
    ASSERT_FALSE(root == NULL);

    cJSON* peciCmd;
    cJSON* peciParams;
    CPUInfo* cpuInfo = cpus;

    cJSON_ArrayForEach(peciCmd, root)
    {
        entry.key = peciCmd->string;
        cJSON_ArrayForEach(peciParams, peciCmd)
        {
            cmdInOut.in.params = cJSON_GetObjectItem(peciParams, "Params");
            cmdInOut.out.ret = PECI_CC_INVALID_REQ;
            status = Execute(&entry, &cmdInOut, cpuInfo);
            EXPECT_EQ(ACD_SUCCESS, status);
            EXPECT_EQ(PECI_CC_SUCCESS, cmdInOut.out.ret);
        }
    }

    hdestroy();
}

TEST_F(ProcessCmdTestFixture, Validator_disabled_test)
{
    TestCrashdump crashdump(cd_gnr);

    // Mock all PECI APIs
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigMmio_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
        .Times(3)
        .WillRepeatedly(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrEndPointPCIConfigLocal_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_Discovery_dom)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigMmio)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
        .Times(3)
        .WillRepeatedly(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrEndPointPCIConfigLocal)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_Discovery)
        .Times(1)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));

    // Actual Test
    ENTRY entry;
    CmdInOut cmdInOut = {};

    // Enable validator to check number of parameters
    ValidatorParams validateInput = {.validateInput = false};
    cmdInOut.validatorParams = &validateInput;
    cmdInOut.out.ret = PECI_CC_INVALID_REQ;

    status = BuildCmdsTable(&entry);
    EXPECT_EQ(ACD_SUCCESS, status);

    cJSON* root = cJSON_Parse(this->inputJsonStr);
    ASSERT_FALSE(root == NULL);

    cJSON* peciCmd;
    cJSON* peciParams;
    CPUInfo* cpuInfo = cpus;

    cJSON_ArrayForEach(peciCmd, root)
    {
        entry.key = peciCmd->string;
        cJSON_ArrayForEach(peciParams, peciCmd)
        {
            cmdInOut.in.params = cJSON_GetObjectItem(peciParams, "Params");
            cmdInOut.out.ret = PECI_CC_INVALID_REQ;
            status = Execute(&entry, &cmdInOut, cpuInfo);
            EXPECT_EQ(ACD_SUCCESS, status);
            EXPECT_EQ(PECI_CC_SUCCESS, cmdInOut.out.ret);
        }
    }

    hdestroy();
}
