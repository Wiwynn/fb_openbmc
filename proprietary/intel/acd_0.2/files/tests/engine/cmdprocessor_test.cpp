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
using std::filesystem::current_path;
using ::testing::Return;
#include <fstream>
namespace fs = std::filesystem;

class ProcessCmdTestFixture : public ::testing::Test
{
  protected:
    static cJSON* inRoot;
    static fs::path UTFile;
    static void readUncoreSampleFile()
    {
        UTFile = fs::current_path();
        UTFile = UTFile.parent_path();
        UTFile /= "tests/UnitTestFiles/ut_uncore.json";
        inRoot = readInputFile(UTFile.c_str());
        if (inRoot == NULL)
        {
            EXPECT_TRUE(inRoot != NULL);
        }
    }
    static void SetUpTestSuite()
    {
        readUncoreSampleFile();
    }

    static void TearDownTestSuite()
    {
        cJSON_Delete(inRoot);
    }

    void SetUp() override
    {
        // Build a list of cpus
        CPUInfo cpuInfo = {};
        cpuInfo.model = cd_spr;
        cpuInfo.clientAddr = 0x30;
        cpusInfo.push_back(cpuInfo);
        cpuInfo.model = cd_spr;
        cpuInfo.clientAddr = 0x31;
        cpusInfo.push_back(cpuInfo);
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
    std::vector<CPUInfo> cpusInfo;
};

fs::path ProcessCmdTestFixture::UTFile;
cJSON* ProcessCmdTestFixture::inRoot;

TEST_F(ProcessCmdTestFixture, SimpleCmdPassFailTest)
{
    TestCrashdump crashdump(cpusInfo);

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
    cmdInOut.out.ret = PECI_CC_INVALID_REQ;
    cmdInOut.in.params = cJSON_GetObjectItem(params, "Params");
    cmdInOut.internalVarName = "UINT64Var";

    status = BuildCmdsTable(&entry);
    EXPECT_EQ(ACD_SUCCESS, status);

    entry.key = "RdIAMSR";
    cmdInOut.paramsTracker = cJSON_CreateObject();
    cmdInOut.internalVarsTracker = cJSON_CreateObject();
    status = Execute(&entry, &cmdInOut);
    EXPECT_EQ(ACD_SUCCESS, status);
    EXPECT_EQ(cmdInOut.out.ret, PECI_CC_SUCCESS);
    EXPECT_EQ(cmdInOut.out.cc, cc);
    EXPECT_EQ((uint64_t)cmdInOut.out.val.u64, (uint64_t)0x1122334455667788);

    entry.key = "RdIAMSRInvalid";
    status = Execute(&entry, &cmdInOut);
    EXPECT_EQ(ACD_INVALID_CMD, status);

    Logging(cmdInOut.internalVarsTracker, "internalVarsTracker:");
    cJSON* val = cJSON_GetObjectItem(cmdInOut.internalVarsTracker, "UINT64Var");
    EXPECT_STREQ(val->valuestring, "0x1122334455667788");
    hdestroy();
}
