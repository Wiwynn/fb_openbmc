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
#include "../mock/test_crashdump.hpp"

extern "C" {
#include <safe_mem_lib.h>

#include "engine/BigCore.h"
#include "engine/crashdump.h"
}

#include "../test_utils.hpp"

#include <chrono>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace std::chrono;
using namespace ::testing;
using ::testing::DoDefault;
using ::testing::Return;

void validateCrashlogVersionAndSize(cJSON* output, std::string expectedVersion,
                                    std::string expectedSize)
{
    cJSON* threadObj;
    cJSON* isSRF = getJsonObjectFromPath("compute0/atom_core", output);
    cJSON* isGNR = getJsonObjectFromPath("compute0/big_core", output);
    if (isSRF == NULL && isGNR == NULL)
    {
        threadObj = output->child->next->child;
    }
    else if (isGNR != NULL)
    {
        threadObj = output->child->child->next->child;
    }
    else
    {
        // atom_core has additional module level
        threadObj = output->child->child->child->next->child;
    }
    cJSON* actual = NULL;

    if (threadObj != NULL)
    {
        actual =
            cJSON_GetObjectItemCaseSensitive(threadObj, "crashlog_version");
        ASSERT_NE(actual, nullptr);
        EXPECT_STREQ(actual->valuestring, expectedVersion.c_str());
        actual = cJSON_GetObjectItemCaseSensitive(threadObj, "size");
        ASSERT_NE(actual, nullptr);
        EXPECT_STREQ(actual->valuestring, expectedSize.c_str());
    }
}

class AtomCoreTest : public ::testing::Test
{
  public:
    std::string outputFilename;
    int ret = 0;
    uint8_t cc = PECI_DEV_CC_SUCCESS;
    RunTimeInfo runTimeInfo = {};
    struct timespec globalStart = {};
    uint8_t numOfCPU;
    uint16_t totalNumOfFrames;
    // Core Record size = 0x0438, XQ size = 0x0238
    uint8_t firstFrame[8] = {0x04, 0xc0, 0x06, 0x46, 0x38, 0x04, 0x38, 0x02};
    uint8_t allOtherFrames[8] = {0xab, 0xfa, 0xed, 0xfe,
                                 0xef, 0xbe, 0xad, 0xde}; // 0xdeadbeeffeedfaab;
  protected:
    // You can use SetUp() and TearDown() for common setup and cleanup tasks
    void SetUp() override
    {
        const ::testing::TestInfo* const test_info =
            ::testing::UnitTest::GetInstance()->current_test_info();
        std::ostringstream filenameStream;
        filenameStream << test_info->test_case_name() << "."
                       << test_info->name() << ".json";
        outputFilename = filenameStream.str();
        clock_gettime(CLOCK_MONOTONIC, &globalStart);
        runTimeInfo.globalRunTime = globalStart;
        runTimeInfo.maxGlobalTime = 700;
        numOfCPU = 1;
        totalNumOfFrames = 206;
    }
};

TEST(BigCoreTest, isIerrPresent_SPR)
{
    int ret;
    TestCrashdump crashdump(cd_spr);
    static cJSON* inRoot;
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_bigcore.json";
    inRoot = readInputFile(UTFile.c_str());
    crashdump.cpus[0].inputFile.bufferPtr = inRoot;
    uint8_t mca_erc_src[8] = {0x00, 0x00, 0x1c, 0x14,
                              0x00, 0x00, 0x00, 0x00}; // 0x141c0000

    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_seq_dom)
        .WillOnce(DoAll(SetArrayArgument<8>(mca_erc_src, mca_erc_src + 8),
                        SetArgPointee<10>(0x94), Return(PECI_CC_SUCCESS)));

    for (auto cpuinfo : crashdump.cpus)
    {
        if (!cpuinfo.clientAddr)
            continue;
        ret = isIerrPresent(&cpuinfo, 0);
        EXPECT_EQ(ret, true);
    }
}

TEST(BigCoreTest, isIerrPresent_GNR)
{
    int ret;
    TestCrashdump crashdump(cd_gnr);
    static cJSON* inRoot;
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_bigcore.json";
    inRoot = readInputFile(UTFile.c_str());
    crashdump.cpus[0].inputFile.bufferPtr = inRoot;
    uint8_t mca_erc_src[8] = {0x00, 0x00, 0x1c, 0x14,
                              0x00, 0x00, 0x00, 0x00}; // 0x141c0000

    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_seq_dom)
        .WillOnce(DoAll(SetArrayArgument<8>(mca_erc_src, mca_erc_src + 8),
                        SetArgPointee<10>(0x94), Return(PECI_CC_SUCCESS)));

    for (auto cpuinfo : crashdump.cpus)
    {
        if (!cpuinfo.clientAddr)
            continue;
        ret = isIerrPresent(&cpuinfo, 0);
        EXPECT_EQ(ret, true);
    }
}

/* TODO: Convert them to GNR tests */
// TEST(BigCoreTest, logCrashdumpSPR_testRegDump)
//{
//     int ret;
//     uint8_t cc = PECI_DEV_CC_SUCCESS;
//     RunTimeInfo runTimeInfo;
//
//     struct timespec globalStart;
//     clock_gettime(CLOCK_MONOTONIC, &globalStart);
//     runTimeInfo.globalRunTime = globalStart;
//     runTimeInfo.maxGlobalTime = 700;
//     TestCrashdump crashdump(cd_spr);
//
//     uint8_t firstFrame[8] = {0x01, 0x90, 0x02, 0x04,
//                              0xa8, 0x05, 0x00, 0x00}; // 0x05a804029001;
//     uint8_t allOtherFrames[8] = {0xab, 0xfa, 0xed, 0xfe,
//                                  0xef, 0xbe, 0xad, 0xde}; //
//                                  0xdeadbeeffeedfaab;
//
//     EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
//         .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
//                         SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
//         .WillRepeatedly(
//             DoAll(SetArrayArgument<6>(allOtherFrames, allOtherFrames + 8),
//                   SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)));
//
//     for (auto cpuinfo : crashdump.cpus)
//     {
//         if (!cpuinfo.clientAddr)
//             continue;
//         ret = logBigCoreSection(&cpuinfo, crashdump.root, runTimeInfo, 0, 0,
//         0); EXPECT_EQ(ret, PECI_CC_SUCCESS);
//     }
//
//     validateCrashlogVersionAndSize(crashdump.root, "0x04029001", "0x05a8");
// }
//
// TEST(BigCoreTest, logCrashdump_NULL_JsonPtr)
//{
//     int ret;
//     RunTimeInfo runTimeInfo;
//     TestCrashdump crashdump(cd_spr);
//     for (auto cpuinfo : crashdump.cpus)
//     {
//         if (!cpuinfo.clientAddr)
//             continue;
//         ret = logBigCoreSection(&cpuinfo, NULL, runTimeInfo, 0, 0, 0);
//         EXPECT_EQ(ret, ACD_INVALID_OBJECT);
//     }
// }
//
// TEST(BigCoreTest, logCrashdumpSPR_testRegDumpWithSqDump)
//{
//     int ret;
//     uint8_t cc = PECI_DEV_CC_SUCCESS;
//     RunTimeInfo runTimeInfo;
//     struct timespec globalStart;
//     clock_gettime(CLOCK_MONOTONIC, &globalStart);
//     runTimeInfo.globalRunTime = globalStart;
//     runTimeInfo.maxGlobalTime = 700;
//
//     TestCrashdump crashdump(cd_spr);
//     static cJSON* inRoot;
//     static std::filesystem::path UTFile;
//     UTFile = std::filesystem::current_path();
//     UTFile = UTFile.parent_path();
//     UTFile /= "tests/UnitTestFiles/ut_bigcore.json";
//     inRoot = readInputFile(UTFile.c_str());
//     crashdump.cpus[0].inputFile.bufferPtr = inRoot;
//     // SQ size = 0x0300, Reg Dump size = 0x05a8
//     uint8_t firstFrame[8] = {0x01, 0x90, 0x02, 0x04,
//                              0xa8, 0x05, 0x00, 0x03}; // 0x030005a804029001;
//     uint8_t allOtherFrames[8] = {0xab, 0xfa, 0xed, 0xfe,
//                                  0xef, 0xbe, 0xad, 0xde}; //
//                                  0xdeadbeeffeedfaab;
//
//     EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
//         .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
//                         SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
//         .WillRepeatedly(
//             DoAll(SetArrayArgument<6>(allOtherFrames, allOtherFrames + 8),
//                   SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)));
//
//     for (auto cpuinfo : crashdump.cpus)
//     {
//         if (!cpuinfo.clientAddr)
//             continue;
//         ret = logBigCoreSection(&cpuinfo, crashdump.root, runTimeInfo, 0, 0,
//         0); EXPECT_EQ(ret, PECI_CC_SUCCESS);
//     }
//
//     validateCrashlogVersionAndSize(crashdump.root, "0x04029001",
//     "0x030005a8");
// }
//
// TEST(BigCoreTest, logCrashdumpSPR_B0_testRegDump)
//{
//     int ret;
//     uint8_t cc = PECI_DEV_CC_SUCCESS;
//     RunTimeInfo runTimeInfo;
//     struct timespec globalStart;
//     clock_gettime(CLOCK_MONOTONIC, &globalStart);
//     runTimeInfo.globalRunTime = globalStart;
//     runTimeInfo.maxGlobalTime = 700;
//     TestCrashdump crashdump(cd_spr);
//
//     uint8_t firstFrame[8] = {0x02, 0x90, 0x02, 0x04,
//                              0xa8, 0x05, 0x00, 0x00}; // 0x05a804029002;
//     uint8_t allOtherFrames[8] = {0xab, 0xfa, 0xed, 0xfe,
//                                  0xef, 0xbe, 0xad, 0xde}; //
//                                  0xdeadbeeffeedfaab;
//
//     EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
//         .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
//                         SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
//         .WillRepeatedly(
//             DoAll(SetArrayArgument<6>(allOtherFrames, allOtherFrames + 8),
//                   SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)));
//
//     for (auto cpuinfo : crashdump.cpus)
//     {
//         if (!cpuinfo.clientAddr)
//             continue;
//         ret = logBigCoreSection(&cpuinfo, crashdump.root, runTimeInfo, 0, 0,
//         0); EXPECT_EQ(ret, PECI_CC_SUCCESS);
//     }
//
//     validateCrashlogVersionAndSize(crashdump.root, "0x04029002", "0x05a8");
// }
//
// TEST(BigCoreTest, logCrashdumpSPR_B0_testRegDumpWithSqDump)
//{
//     int ret;
//     uint8_t cc = PECI_DEV_CC_SUCCESS;
//     RunTimeInfo runTimeInfo;
//     struct timespec globalStart;
//     clock_gettime(CLOCK_MONOTONIC, &globalStart);
//     runTimeInfo.globalRunTime = globalStart;
//     runTimeInfo.maxGlobalTime = 700;
//     TestCrashdump crashdump(cd_spr);
//
//     // SQ size = 0x0300, Reg Dump size = 0x05a8
//     uint8_t firstFrame[8] = {0x02, 0x90, 0x02, 0x04,
//                              0xa8, 0x05, 0x00, 0x03}; // 0x030005a804029002;
//     uint8_t allOtherFrames[8] = {0xab, 0xfa, 0xed, 0xfe,
//                                  0xef, 0xbe, 0xad, 0xde}; //
//                                  0xdeadbeeffeedfaab;
//
//     EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
//         .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
//                         SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
//         .WillRepeatedly(
//             DoAll(SetArrayArgument<6>(allOtherFrames, allOtherFrames + 8),
//                   SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)));
//
//     for (auto cpuinfo : crashdump.cpus)
//     {
//         if (!cpuinfo.clientAddr)
//             continue;
//         ret = logBigCoreSection(&cpuinfo, crashdump.root, runTimeInfo, 0, 0,
//         0); EXPECT_EQ(ret, PECI_CC_SUCCESS);
//     }
//
//     validateCrashlogVersionAndSize(crashdump.root, "0x04029002",
//     "0x030005a8");
// }
//
// TEST(BigCoreTest, logCrashdumpSPR_C0_testRegDump)
//{
//     int ret;
//     uint8_t cc = PECI_DEV_CC_SUCCESS;
//     RunTimeInfo runTimeInfo;
//     struct timespec globalStart;
//     clock_gettime(CLOCK_MONOTONIC, &globalStart);
//     runTimeInfo.globalRunTime = globalStart;
//     runTimeInfo.maxGlobalTime = 700;
//     TestCrashdump crashdump(cd_spr);
//
//     uint8_t firstFrame[8] = {0x03, 0x90, 0x02, 0x04,
//                              0xb0, 0x05, 0x00, 0x00}; // 0x05b004029003;
//     uint8_t allOtherFrames[8] = {0xab, 0xfa, 0xed, 0xfe,
//                                  0xef, 0xbe, 0xad, 0xde}; //
//                                  0xdeadbeeffeedfaab;
//
//     EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
//         .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
//                         SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
//         .WillRepeatedly(
//             DoAll(SetArrayArgument<6>(allOtherFrames, allOtherFrames + 8),
//                   SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)));
//
//     for (auto cpuinfo : crashdump.cpus)
//     {
//         if (!cpuinfo.clientAddr)
//             continue;
//         ret = logBigCoreSection(&cpuinfo, crashdump.root, runTimeInfo, 0, 0,
//         0); EXPECT_EQ(ret, PECI_CC_SUCCESS);
//     }
//
//     validateCrashlogVersionAndSize(crashdump.root, "0x04029003", "0x05b0");
// }
//
// TEST(BigCoreTest, logCrashdumpSPR_C0_testRegDumpWithSqDump)
//{
//     int ret;
//     uint8_t cc = PECI_DEV_CC_SUCCESS;
//     RunTimeInfo runTimeInfo;
//     struct timespec globalStart;
//     clock_gettime(CLOCK_MONOTONIC, &globalStart);
//     runTimeInfo.globalRunTime = globalStart;
//     runTimeInfo.maxGlobalTime = 700;
//     TestCrashdump crashdump(cd_spr);
//
//     // SQ size = 0x0300, Reg Dump size = 0x05b0
//     uint8_t firstFrame[8] = {0x03, 0x90, 0x02, 0x04,
//                              0xb0, 0x05, 0x00, 0x03}; // 0x030005b004029003;
//     uint8_t allOtherFrames[8] = {0xab, 0xfa, 0xed, 0xfe,
//                                  0xef, 0xbe, 0xad, 0xde}; //
//                                  0xdeadbeeffeedfaab;
//
//     EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
//         .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
//                         SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
//         .WillRepeatedly(
//             DoAll(SetArrayArgument<6>(allOtherFrames, allOtherFrames + 8),
//                   SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)));
//
//     for (auto cpuinfo : crashdump.cpus)
//     {
//         if (!cpuinfo.clientAddr)
//             continue;
//         ret = logBigCoreSection(&cpuinfo, crashdump.root, runTimeInfo, 0, 0,
//         0); EXPECT_EQ(ret, PECI_CC_SUCCESS);
//     }
//
//     validateCrashlogVersionAndSize(crashdump.root, "0x04029003",
//     "0x030005b0");
// }
//
// TEST(BigCoreTest, getTotalInputRegsSize_incorrectVersion_SPR)
//{
//     TestCrashdump crashdump(cd_spr);
//     uint32_t size = getTotalInputRegsSize(&crashdump.cpus[0], 0xff, 0);
//     EXPECT_EQ(size, ACD_INVALID_OBJECT);
// }
//
// TEST(BigCoreTest, getTotalInputRegsSize_correctSize_SPR)
//{
//     TestCrashdump crashdump(cd_spr);
//     uint32_t version_spr = 0x4029001;
//     uint32_t size =
//         getTotalInputRegsSize(&crashdump.cpus[0], version_spr, 0x5A8);
//
//     EXPECT_EQ(size, (uint32_t)0x5A8);
// }
//
// TEST(BigCoreTest, getTotalInputRegsSize_correctSizeOptimizedReturn_SPR)
//{
//     TestCrashdump crashdump(cd_spr);
//     uint32_t version_spr = 0x4029001;
//     uint32_t size =
//         getTotalInputRegsSize(&crashdump.cpus[0], version_spr, 0x5A8);
//
//     EXPECT_EQ(size, (uint32_t)0x5A8);
//
//     size = getTotalInputRegsSize(&crashdump.cpus[0], version_spr, 0x5A8);
//     EXPECT_EQ(size, (uint32_t)0x5A8);
// }
//
// TEST(BigCoreTest, getTotalInputRegsSize_correctSize_SPR_B0)
//{
//     TestCrashdump crashdump(cd_spr);
//     uint32_t version_spr = 0x4029002;
//     uint32_t size =
//         getTotalInputRegsSize(&crashdump.cpus[0], version_spr, 0x5A8);
//
//     EXPECT_EQ(size, (uint32_t)0x5A8);
// }
//
// TEST(BigCoreTest, getTotalInputRegsSize_correctSizeOptimizedReturn_SPR_B0)
//{
//     TestCrashdump crashdump(cd_spr);
//     uint32_t version_spr = 0x4029002;
//     uint32_t size =
//         getTotalInputRegsSize(&crashdump.cpus[0], version_spr, 0x5A8);
//
//     EXPECT_EQ(size, (uint32_t)0x5A8);
//
//     size = getTotalInputRegsSize(&crashdump.cpus[0], version_spr, 0x5A8);
//     EXPECT_EQ(size, (uint32_t)0x5A8);
// }
//
// TEST(BigCoreTest, getTotalInputRegsSize_correctSize_SPR_C0)
//{
//     TestCrashdump crashdump(cd_spr);
//     uint32_t version_spr = 0x4029003;
//     uint32_t size =
//         getTotalInputRegsSize(&crashdump.cpus[0], version_spr, 0x5A8);
//
//     EXPECT_EQ(size, (uint32_t)0x5B0);
// }
//
// TEST(BigCoreTest, getTotalInputRegsSize_correctSizeOptimizedReturn_SPR_C0)
//{
//     TestCrashdump crashdump(cd_spr);
//     uint32_t version_spr = 0x4029003;
//     uint32_t size =
//         getTotalInputRegsSize(&crashdump.cpus[0], version_spr, 0x5B0);
//
//     EXPECT_EQ(size, (uint32_t)0x5B0);
//
//     size = getTotalInputRegsSize(&crashdump.cpus[0], version_spr, 0x5B0);
//     EXPECT_EQ(size, (uint32_t)0x5B0);
// }
//
// TEST(BigCoreTest, logCrashdumpSPR_C0_testRegDumpWithSqDump_withDelay)
//{
//     int ret;
//     uint8_t cc = PECI_DEV_CC_SUCCESS;
//     RunTimeInfo runTimeInfo;
//     TestCrashdump crashdump(cd_spr, 700);
//     struct timespec globalStart;
//     clock_gettime(CLOCK_MONOTONIC, &globalStart);
//     runTimeInfo.globalRunTime = globalStart;
//     runTimeInfo.maxGlobalTime = 700;
//
//     crashdump.libPeciMock->DelegateForBigCore();
//
//     // SQ size = 0x0300, Reg Dump size = 0x05b0
//     uint8_t firstFrame[8] = {0x03, 0x90, 0x02, 0x04,
//                              0xb0, 0x05, 0x00, 0x03}; // 0x030005b004029003;
//     EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
//         .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
//                         SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
//         .WillRepeatedly(DoDefault());
//
//     std::chrono::steady_clock::time_point startTime =
//         std::chrono::steady_clock::now();
//     for (auto cpuinfo : crashdump.cpus)
//     {
//         if (!cpuinfo.clientAddr)
//             continue;
//         ret = logBigCoreSection(&cpuinfo, crashdump.root, runTimeInfo, 0, 0,
//         0); EXPECT_EQ(ret, PECI_CC_SUCCESS);
//     }
//
//     std::chrono::steady_clock::time_point endTime =
//         std::chrono::steady_clock::now();
//     ASSERT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
//                                                                     startTime)
//                   .count(),
//               8400); // 8400 ==> 11(getframe calls) * 700ms
//     validateCrashlogVersionAndSize(crashdump.root, "0x04029003",
//     "0x030005b0");
// }
//
// TEST(BigCoreTest,
// logCrashdumpSPR_C0_testRegDumpWithSqDump_withDelay_PerfTest)
//{
//     int ret;
//     uint8_t cc = PECI_DEV_CC_SUCCESS;
//     RunTimeInfo runTimeInfo;
//     TestCrashdump crashdump(cd_spr, 700);
//     struct timespec globalStart;
//     clock_gettime(CLOCK_MONOTONIC, &globalStart);
//     runTimeInfo.globalRunTime = globalStart;
//     runTimeInfo.maxGlobalTime = 700;
//     crashdump.libPeciMock->DelegateForBigCoreWithGoodReturnWithDelay();
//
//     // SQ size = 0x0300, Reg Dump size = 0x05b0
//     uint8_t firstFrame[8] = {0x03, 0x90, 0x02, 0x04,
//                              0xb0, 0x05, 0x00, 0x03}; // 0x030005b004029003;
//     EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
//         .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
//                         SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
//         .WillRepeatedly(DoDefault());
//
//     std::chrono::steady_clock::time_point startTime =
//         std::chrono::steady_clock::now();
//     for (auto cpuinfo : crashdump.cpus)
//     {
//         if (!cpuinfo.clientAddr)
//             continue;
//         ret = logBigCoreSection(&cpuinfo, crashdump.root, runTimeInfo, 0, 0,
//         0); EXPECT_EQ(ret, PECI_CC_SUCCESS);
//     }
//
//     std::chrono::steady_clock::time_point endTime =
//         std::chrono::steady_clock::now();
//
//     ASSERT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
//                                                                     startTime)
//                   .count(),
//               151000); // 151000 = 151000ms(30sec input file max) + other
//                        // processing time
//
//     cJSON* threadFromCore0Obj = crashdump.root->child->next->next->child;
//     cJSON* actual = NULL;
//     actual = cJSON_GetObjectItemCaseSensitive(threadFromCore0Obj,
//                                               "crashlog_version");
//     ASSERT_NE(actual, nullptr);
//     EXPECT_STREQ(actual->valuestring, "0x04029003");
//
//     actual = cJSON_GetObjectItemCaseSensitive(threadFromCore0Obj, "size");
//     ASSERT_NE(actual, nullptr);
//     EXPECT_STREQ(actual->valuestring, "0x030005b0");
//
//     actual = cJSON_GetObjectItemCaseSensitive(crashdump.root,
//     CD_ABORT_MSG_KEY); ASSERT_NE(actual, nullptr);
//     EXPECT_THAT(actual->valuestring, HasSubstr("Max time 150 sec exceeded"));
// }

TEST_F(AtomCoreTest, logCrashdumpSRF_C0_testRegDumpWithSqDump)
{
    TestCrashdump crashdump(cd_srf);
    for (int i = 0; i < numOfCPU; i++)
    {
        crashdump.cpus[i].cpuidRead.cpuidValid = true;
        initDieReadStructure(&crashdump.cpus[i].dieMaskInfo,
                             DieMaskRange.SRF.compute, DieMaskRange.SRF.io);
        initComputeDieStructure(&crashdump.cpus[i].dieMaskInfo,
                                &crashdump.cpus[i].computeDies);
        // Notes: In SRF this is module mask, two modules
        crashdump.cpus[i].computeDies[0].coreMask = 0x3;
    }

    // Mocks
    InSequence s;
    int numOfModules =
        __builtin_popcount(crashdump.cpus[0].computeDies[0].coreMask);

    for (int module = 0; module < numOfModules; module++)
    {
        for (int core = 0; core < CD_ATOM_CORES_PER_MODULE; core++)
        {
            EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
                .Times(totalNumOfFrames)
                .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                                SetArgPointee<7>(0x40),
                                Return(PECI_CC_SUCCESS)))
                .WillRepeatedly(DoAll(
                    SetArrayArgument<6>(allOtherFrames, allOtherFrames + 8),
                    SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)));
        }
    }

    // mock the rest of module reads as invalid
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
        .WillRepeatedly(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                              SetArgPointee<7>(0x90),
                              Return(PECI_CC_INVALID_REQ)));

    // Function to be tested
    ret = logBigCoreSection(&crashdump.cpus[0], crashdump.root, runTimeInfo, 0,
                            0, 0);

    outputDebugFile(crashdump.root, this->outputFilename.c_str(), true);

    // Test outputs
    EXPECT_EQ(ret, PECI_CC_SUCCESS);
    validateCrashlogVersionAndSize(crashdump.root, "0x4606c004", "0x02380438");

    char jsonPathStr[NAME_STR_LEN];
    cJSON* child;
    uint8_t coresWithData = 4;

    for (int module = 0; module < numOfModules; module++)
    {
        for (int core = 0; core < coresWithData; core++)
        {
            cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                          "compute0/atom_core/module%d/core%d", module, core);
            child = getJsonObjectFromPath(jsonPathStr, crashdump.root);
            ASSERT_FALSE(child == NULL)
                << "module:" << module << "core:" << core;
        }
    }
}

TEST_F(AtomCoreTest, logCrashdumpSRF_C0_OneBadCore_testRegDumpWithSqDump)
{
    TestCrashdump crashdump(cd_srf);
    for (int i = 0; i < numOfCPU; i++)
    {
        crashdump.cpus[i].cpuidRead.cpuidValid = true;
        initDieReadStructure(&crashdump.cpus[i].dieMaskInfo,
                             DieMaskRange.SRF.compute, DieMaskRange.SRF.io);
        initComputeDieStructure(&crashdump.cpus[i].dieMaskInfo,
                                &crashdump.cpus[i].computeDies);
        // Notes: In SRF this is module mask
        crashdump.cpus[i].computeDies[0].coreMask = 0x1;
    }

    // Mocks
    InSequence s;
    int numOfModules =
        __builtin_popcount(crashdump.cpus[0].computeDies[0].coreMask);

    // core 0 has no first frame
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
        .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                        SetArgPointee<7>(0x90), Return(PECI_CC_INVALID_REQ)));

    uint8_t coresWithData = 3;
    for (int module = 0; module < numOfModules; module++)
    {
        for (int core = 0; core < coresWithData; core++)
        {
            EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
                .Times(totalNumOfFrames)
                .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                                SetArgPointee<7>(0x40),
                                Return(PECI_CC_SUCCESS)))
                .WillRepeatedly(DoAll(
                    SetArrayArgument<6>(allOtherFrames, allOtherFrames + 8),
                    SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)));
        }
    }

    // mock the rest of module reads as invalid
    EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
        .WillRepeatedly(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                              SetArgPointee<7>(0x90),
                              Return(PECI_CC_INVALID_REQ)));

    // Function to be tested
    ret = logBigCoreSection(&crashdump.cpus[0], crashdump.root, runTimeInfo, 0,
                            0, 0);

    outputDebugFile(crashdump.root, this->outputFilename.c_str(), true);

    // Output Tests
    EXPECT_EQ(ret, PECI_CC_SUCCESS);
    validateCrashlogVersionAndSize(crashdump.root, "0x4606c004", "0x02380438");

    char jsonPathStr[NAME_STR_LEN];
    cJSON* child;
    for (int module = 0; module < numOfModules; module++)
    {
        for (int core = 0; core < MAX_CORE_PER_MODULE; core++)
        {
            cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                          "compute0/atom_core/module%d/core%d", module, core);
            child = getJsonObjectFromPath(jsonPathStr, crashdump.root);
            if (core == 0)
            {
                ASSERT_TRUE(child == NULL)
                    << "module:" << module << "core:" << core;
            }
            else
            {
                ASSERT_FALSE(child == NULL)
                    << "module:" << module << "core:" << core;
            }
        }
    }
}
