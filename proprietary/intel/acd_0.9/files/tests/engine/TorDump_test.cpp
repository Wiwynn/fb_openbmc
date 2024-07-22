/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2022 Intel Corporation.
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
#include "engine/TorDump.h"
#include "engine/crashdump.h"
#include "engine/utils.h"
}

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace ::testing;
using ::testing::Return;

void validateCha(cJSON* output, int cha, bool hasFailure,
                 std::string expectedGoodValue,
                 std::string expectedFailureValue,
                 std::string expectedAfterFailureValue)
{
    char sCha[8];
    char sIndex[9];
    char sSubIndex[12];
    bool failureFound = false;
    cd_snprintf_s(sCha, sizeof(sCha), "cha%d", cha);
    ASSERT_TRUE(cJSON_HasObjectItem(output, sCha));
    cJSON* jCha = cJSON_GetObjectItemCaseSensitive(output, sCha);
    ASSERT_NE(jCha, nullptr);

    for (uint8_t index = 0; index < TD_TORS_PER_CHA; index++)
    {
        cd_snprintf_s(sIndex, sizeof(sIndex), "index%d", index);
        ASSERT_TRUE(cJSON_HasObjectItem(jCha, sIndex));
        cJSON* jIndex = cJSON_GetObjectItemCaseSensitive(jCha, sIndex);
        ASSERT_NE(jIndex, nullptr);
        for (uint8_t subIndex = 0; subIndex < TD_SUBINDEX_PER_TOR; subIndex++)
        {
            cd_snprintf_s(sSubIndex, sizeof(sSubIndex), "subindex%d", subIndex);
            ASSERT_TRUE(cJSON_HasObjectItem(jIndex, sSubIndex));

            cJSON* jSubIndex =
                cJSON_GetObjectItemCaseSensitive(jIndex, sSubIndex);
            ASSERT_NE(jSubIndex, nullptr);
            if (hasFailure)
            {
                if (!failureFound)
                {
                    EXPECT_STREQ(jSubIndex->valuestring,
                                 expectedFailureValue.c_str());
                    failureFound = true;
                }
                else
                {
                    EXPECT_STREQ(jSubIndex->valuestring,
                                 expectedAfterFailureValue.c_str());
                }
            }
            else
            {
                EXPECT_STREQ(jSubIndex->valuestring, expectedGoodValue.c_str());
            }
        }
    }
}

TEST(TorDumpTestFixture, logTorSection_Null_output_node)
{
    int ret;

    TestCrashdump crashdump(cd_spr);
    RunTimeInfo runTimeInfo = {};
    LoggerStruct loggerStruct = {};
    for (auto cpuinfo : crashdump.cpus)
    {
        if (!cpuinfo.clientAddr)
            continue;
        ret =
            logTorSection(&cpuinfo, NULL, 0, 0, 0, runTimeInfo, &loggerStruct);
        EXPECT_EQ(ret, ACD_INVALID_OBJECT);
    }
}

TEST(TorDumpTestFixture, logTorSection_ValidModel)
{
    int ret;
    TestCrashdump crashdump(cd_spr);

    RunTimeInfo runTimeInfo = {};
    LoggerStruct loggerStruct = {};
    for (auto cpuinfo : crashdump.cpus)
    {
        if (!cpuinfo.clientAddr)
            continue;
        ret = logTorSection(&cpuinfo, crashdump.root, 0, 0, 0, runTimeInfo,
                            &loggerStruct);
        EXPECT_EQ(ret, ACD_SUCCESS);
    }
}

TEST(TorDumpTestFixture, logTorSection_Basic_Data_Flow)
{
    int ret;
    TestCrashdump crashdump(cd_spr);
    RunTimeInfo runTimeInfo = {};
    LoggerStruct loggerStruct = {};
    uint8_t Data[16] = {0x0d, 0xf0, 0xad, 0xba, 0xef, 0xbe, 0xad, 0xde,
                        0x0d, 0xf0, 0xad, 0xba, 0xef, 0xbe, 0xad, 0xde};
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame)
        .WillRepeatedly(DoAll(SetArrayArgument<5>(Data, Data + 16),
                              SetArgPointee<6>(0x40), Return(PECI_CC_SUCCESS)));

    for (auto cpuinfo : crashdump.cpus)
    {
        if (!cpuinfo.clientAddr)
            continue;
        cpuinfo.chaCount = 1;
        ret = logTorSection(&cpuinfo, crashdump.root, 16, 0, 0, runTimeInfo,
                            &loggerStruct);
        // char* str1 = cJSON_Print(crashdump.root);
        // printf("%s\n", str1);
        EXPECT_EQ(ret, ACD_SUCCESS);
    }

    validateCha(crashdump.root, 0, false, "0xdeadbeefbaadf00ddeadbeefbaadf00d",
                "", "");
}

TEST(TorDumpTestFixture, logTorSection_BadCC)
{
    int ret;
    TestCrashdump crashdump(cd_spr);
    RunTimeInfo runTimeInfo = {};
    LoggerStruct loggerStruct = {};
    uint8_t Data[16] = {0x0d, 0xf0, 0xad, 0xba, 0xef, 0xbe, 0xad, 0xde,
                        0x0d, 0xf0, 0xad, 0xba, 0xef, 0xbe, 0xad, 0xde};

    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame)
        .WillRepeatedly(DoAll(SetArrayArgument<5>(Data, Data + 16),
                              SetArgPointee<6>(0x80), Return(PECI_CC_SUCCESS)));

    for (auto cpuinfo : crashdump.cpus)
    {
        if (!cpuinfo.clientAddr)
            continue;
        cpuinfo.chaCount = 1;
        ret = logTorSection(&cpuinfo, crashdump.root, 16, 0, 0, runTimeInfo,
                            &loggerStruct);
        EXPECT_EQ(ret, ACD_SUCCESS);
    }

    validateCha(crashdump.root, 0, true, "0xdeadbeefbaadf00ddeadbeefbaadf00d",
                "0xdeadbeefbaadf00ddeadbeefbaadf00d,CC:0x80,RC:0x0", "N/A");
}

TEST(TorDumpTestFixture, logTorSection_BadCC_skip_on_fail_false)
{
    int ret;
    TestCrashdump crashdump(cd_spr);
    RunTimeInfo runTimeInfo = {};
    LoggerStruct loggerStruct = {};
    uint8_t Data[16] = {0x0d, 0xf0, 0xad, 0xba, 0xef, 0xbe, 0xad, 0xde,
                        0x0d, 0xf0, 0xad, 0xba, 0xef, 0xbe, 0xad, 0xde};

    cJSON* tor =
        getNewCrashDataSection(crashdump.cpus[0].inputFile.bufferPtr, "TOR");

    EXPECT_TRUE(tor != NULL);

    int replace = cJSON_ReplaceItemInObjectCaseSensitive(tor, "SkipOnFail",
                                                         cJSON_CreateFalse());

    EXPECT_TRUE(replace);

    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame)
        .WillRepeatedly(DoAll(SetArrayArgument<5>(Data, Data + 16),
                              SetArgPointee<6>(0x80), Return(PECI_CC_SUCCESS)));

    for (auto cpuinfo : crashdump.cpus)
    {
        if (!cpuinfo.clientAddr)
            continue;
        cpuinfo.chaCount = 1;
        ret = logTorSection(&cpuinfo, crashdump.root, 16, 0, 0, runTimeInfo,
                            &loggerStruct);
        EXPECT_EQ(ret, ACD_SUCCESS);
    }
    validateCha(crashdump.root, 0, true, "0xdeadbeefbaadf00ddeadbeefbaadf00d",
                "0xdeadbeefbaadf00ddeadbeefbaadf00d,CC:0x80,RC:0x0",
                "0xdeadbeefbaadf00ddeadbeefbaadf00d,CC:0x80,RC:0x0");
}

TEST(TorDumpTestFixture, logTorSection_BadRet)
{
    int ret;
    TestCrashdump crashdump(cd_spr);

    RunTimeInfo runTimeInfo = {};
    LoggerStruct loggerStruct = {};
    uint8_t Data[16] = {0x0d, 0xf0, 0xad, 0xba, 0xef, 0xbe, 0xad, 0xde,
                        0x0d, 0xf0, 0xad, 0xba, 0xef, 0xbe, 0xad, 0xde};

    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame)
        .WillRepeatedly(DoAll(SetArrayArgument<5>(Data, Data + 16),
                              SetArgPointee<6>(0x80),
                              Return(PECI_CC_DRIVER_ERR)));

    for (auto cpuinfo : crashdump.cpus)
    {
        if (!cpuinfo.clientAddr)
            continue;
        cpuinfo.chaCount = 1;
        ret = logTorSection(&cpuinfo, crashdump.root, 16, 0, 0, runTimeInfo,
                            &loggerStruct);
        EXPECT_EQ(ret, ACD_SUCCESS);
    }
    validateCha(crashdump.root, 0, true, "0xdeadbeefbaadf00ddeadbeefbaadf00d",
                "0x0,CC:0x80,RC:0x3", "N/A");
}

TEST(TorDumpTestFixture, logTorSection_Leading_Zeros)
{
    int ret;
    TestCrashdump crashdump(cd_spr);

    RunTimeInfo runTimeInfo = {};
    LoggerStruct loggerStruct = {};
    uint8_t Data[16] = {0x0d, 0xf0, 0xad, 0xba, 0xef, 0xbe, 0xad, 0xde,
                        0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0};

    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame)
        .WillRepeatedly(DoAll(SetArrayArgument<5>(Data, Data + 16),
                              SetArgPointee<6>(0x40), Return(PECI_CC_SUCCESS)));

    for (auto cpuinfo : crashdump.cpus)
    {
        if (!cpuinfo.clientAddr)
            continue;
        cpuinfo.chaCount = 1;
        ret = logTorSection(&cpuinfo, crashdump.root, 16, 0, 0, runTimeInfo,
                            &loggerStruct);
        EXPECT_EQ(ret, ACD_SUCCESS);
    }
    validateCha(crashdump.root, 0, false, "0xdeadbeefbaadf00d",
                "0x0,CC:0x80,RC:0x3", "N/A");
}

TEST(TorDumpTestFixture, logTorSection_GNR_Global_Timeout)
{
    int ret;
    TestCrashdump crashdump(cd_gnr);
    LoggerStruct loggerStruct;
    CmdInOut cmdInOut;
    RunTimeInfo runTimeInfo = {};
    LogConfig(&loggerStruct, KEY_VALUE);
    // Set Output path for Logger
    static const char* OutputPathStr = R"({"OutputPath":"Tor"})";
    static const char* OutputStr = R"({"Output":""})";

    cJSON* OutputPathjson = cJSON_Parse(OutputPathStr);
    cJSON* Outputjson = cJSON_Parse(OutputStr);
    cmdInOut.in.outputPath = Outputjson;

    getPath(OutputPathjson, &loggerStruct);
    ParsePath(&loggerStruct);
    ParseNameSection(&cmdInOut, &loggerStruct);

    cmdInOut.out.ret = PECI_CC_SUCCESS;
    cmdInOut.out.cc = PECI_DEV_CC_SUCCESS;
    loggerStruct.contextLogger.skipFlag = false;
    loggerStruct.nameProcessing.size = 4;
    loggerStruct.contextLogger.skipFlag = false;

    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.maxGlobalTime = 0;

    cmdInOut.runTimeInfo = &runTimeInfo;

    for (auto cpuinfo : crashdump.cpus)
    {
        if (!cpuinfo.clientAddr)
            continue;
        cpuinfo.chaCount = 1;
        ret = logTorSection(&cpuinfo, crashdump.root, 16, 0, 0, runTimeInfo,
                            &loggerStruct);
        EXPECT_EQ(ret, ACD_GLOBAL_TIMEOUT);
    }
}

TEST(TorDumpTestFixture, logTorSection_GNR_Section_Timeout)
{
    int ret;
    TestCrashdump crashdump(cd_gnr);
    LoggerStruct loggerStruct;
    CmdInOut cmdInOut;
    RunTimeInfo runTimeInfo = {};
    LogConfig(&loggerStruct, KEY_VALUE);
    // Set Output path for Logger
    static const char* OutputPathStr = R"({"OutputPath":"Tor"})";
    static const char* OutputStr = R"({"Output":""})";

    cJSON* OutputPathjson = cJSON_Parse(OutputPathStr);
    cJSON* Outputjson = cJSON_Parse(OutputStr);
    cmdInOut.in.outputPath = Outputjson;

    getPath(OutputPathjson, &loggerStruct);
    ParsePath(&loggerStruct);
    ParseNameSection(&cmdInOut, &loggerStruct);

    cmdInOut.out.ret = PECI_CC_SUCCESS;
    cmdInOut.out.cc = PECI_DEV_CC_SUCCESS;
    loggerStruct.contextLogger.skipFlag = false;
    loggerStruct.nameProcessing.size = 4;
    loggerStruct.contextLogger.skipFlag = false;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.maxGlobalTime = 700;
    runTimeInfo.maxSectionTime = 0;
    cmdInOut.runTimeInfo = &runTimeInfo;

    for (auto cpuinfo : crashdump.cpus)
    {
        if (!cpuinfo.clientAddr)
            continue;
        cpuinfo.chaCount = 1;
        ret = logTorSection(&cpuinfo, crashdump.root, 16, 0, 0, runTimeInfo,
                            &loggerStruct);
        EXPECT_EQ(ret, ACD_SECTION_TIMEOUT);
    }
}
