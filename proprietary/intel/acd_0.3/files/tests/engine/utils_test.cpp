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
#include "engine/utils.h"
}

#include "tests/test_utils.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(UtilsTestFixture, snprintf_s_test)
{

    char jsonItemString[10];
    cd_snprintf_s(jsonItemString, 10, "%s", "testing");
    EXPECT_STREQ(jsonItemString, "testing");
}

TEST(UtilsTestFixture, readInputFileFlag_test)
{
    cJSON* root = NULL;
    bool status = readInputFileFlag(root, true, "ENABLE");
    EXPECT_TRUE(status);
    status = readInputFileFlag(root, false, "ENABLE");
    EXPECT_FALSE(status);
    static const char* rootstrtrue = R"({"ENABLE":true})";
    root = cJSON_Parse(rootstrtrue);
    status = readInputFileFlag(root, true, "ENABLE");
    EXPECT_TRUE(status);
    static const char* rootstrfalse = R"({"ENABLE":false})";
    root = cJSON_Parse(rootstrfalse);
    status = readInputFileFlag(root, true, "ENABLE");
    EXPECT_FALSE(status);
}

TEST(UtilsTestFixture, getNewCrashDataSection_test)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_mmio.json");
    cJSON* section = getNewCrashDataSection(inRoot, "Uncore_MMIO");
    EXPECT_TRUE(section != NULL);
}

TEST(UtilsTestFixture, getNewCrashDataSection_test_NULL)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_mmio.json");
    cJSON* section = getNewCrashDataSection(inRoot, "MMIO");
    EXPECT_TRUE(section == NULL);
}

TEST(UtilsTestFixture, getNewCrashDataSectionObjectOneLevel_test)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_mmio.json");
    cJSON* section =
        getNewCrashDataSectionObjectOneLevel(inRoot, "Uncore_MMIO", "Commands");
    EXPECT_TRUE(section != NULL);
}

TEST(UtilsTestFixture, getNewCrashDataSectionObjectOneLevel_test_NULL)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_mmio.json");
    cJSON* section = getNewCrashDataSectionObjectOneLevel(inRoot, "Uncore_MMIO",
                                                          "Commands2");
    EXPECT_TRUE(section == NULL);
}

TEST(UtilsTestFixture, isBigCoreRegVersionMatch_test)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_bigcore.json");
    bool status = isBigCoreRegVersionMatch(inRoot, 0x4029001);
    EXPECT_TRUE(status);
}

TEST(UtilsTestFixture, isBigCoreRegVersionMatch_test_NULL)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_bigcore.json");
    bool status = isBigCoreRegVersionMatch(inRoot, 0x4029004);
    EXPECT_FALSE(status);
}

TEST(UtilsTestFixture, getCrashDataSectionBigCoreRegList_test)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_bigcore.json");
    cJSON* output = getCrashDataSectionBigCoreRegList(inRoot, "0x4029001", 0);
    EXPECT_TRUE(output != NULL);
}

TEST(UtilsTestFixture, getCrashDataSectionBigCoreRegList_test_NULL)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_bigcore.json");
    cJSON* output = getCrashDataSectionBigCoreRegList(inRoot, "0x53", 0);
    EXPECT_TRUE(output == NULL);
}

TEST(UtilsTestFixture, getCrashDataSectionBigCoreSize_test)
{
    uint32_t value = 0;
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_bigcore.json");
    uint32_t output = getCrashDataSectionBigCoreSize(inRoot, "0x4029001", 0);
    EXPECT_EQ(output, value);
}

TEST(UtilsTestFixture, loadInputFiles_test)
{
    char* InputFile = "Testing";
    Model cpu = cd_sprhbm;
    cJSON* inRoot = selectAndReadInputFile(cpu, &InputFile, false);
    EXPECT_TRUE(inRoot == NULL);
}

TEST(UtilsTestFixture, loadInputFiles_telemetry_test)
{
    char* InputFile = "Testing";
    Model cpu = cd_sprhbm;
    cJSON* inRoot = selectAndReadInputFile(cpu, &InputFile, true);
    EXPECT_TRUE(inRoot == NULL);
}

// TODO: Fix this test
TEST(UtilsTestFixture, DISABLED_getNVDSectionRegList_test)
{
    bool testing = true;
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_nvd.json");
    cJSON* outRoot = getNVDSectionRegList(inRoot, "csr", &testing);
    EXPECT_TRUE(outRoot != NULL);
}

// TODO: Fix this test
TEST(UtilsTestFixture, DISABLED_getPMEMSectionErrLogList_test)
{
    bool testing = true;
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_nvd.json");
    cJSON* outRoot = getPMEMSectionErrLogList(inRoot, "error_log", &testing);
    EXPECT_TRUE(outRoot != NULL);
}

TEST(UtilsTestFixture, logResetDetected_negative_test)
{
    static PlatformState platformState = {false, DEFAULT_VALUE, DEFAULT_VALUE,
                                          DEFAULT_VALUE, DEFAULT_VALUE};
    cJSON* metadata = cJSON_CreateObject();
    CPUInfo cpuInfo;
    logResetDetected(metadata, cpuInfo, platformState);
    EXPECT_STREQ(metadata->child->string, "_reset_detected");
    EXPECT_STREQ(metadata->child->valuestring, "NONE");
}

// TODO: fix this test
TEST(UtilsTestFixture, DISABLED_logResetDetected_postive_test)
{
    static PlatformState platformState = {false, DEFAULT_VALUE, DEFAULT_VALUE,
                                          DEFAULT_VALUE, DEFAULT_VALUE};
    cJSON* metadata = cJSON_CreateObject();
    CPUInfo cpuInfo;
    cJSON* inRoot = readInputFile("../crashdump_input_gnr.json");
    cpuInfo.inputFile.bufferPtr = inRoot;
    platformState.resetDetected = true;
    platformState.resetCpu = 1;
    platformState.resetSection = 1;
    logResetDetected(metadata, cpuInfo, platformState);
    EXPECT_STREQ(metadata->child->string, "_reset_detected");
    EXPECT_STREQ(metadata->child->valuestring,
                 "cpu1.Metadata_Cpu_Compute_Early");
}

TEST(UtilsTestFixture, fillMeVersion_test)
{
    cJSON* metaData = cJSON_CreateObject();
    int status = fillMeVersion("me_fw_ver", metaData);
    EXPECT_EQ(status, ACD_SUCCESS);
}

TEST(UtilsTestFixture, fillCrashdumpVersion_test)
{
    cJSON* metaData = cJSON_CreateObject();
    int status = fillCrashdumpVersion("crashdump_ver", metaData);
    EXPECT_EQ(status, ACD_SUCCESS);
}

TEST(UtilsTestFixture, cJSONToInt_PASS_test)
{
    cJSON* object = cJSON_CreateObject();
    cJSON_AddStringToObject(object, "Register", "0x16");
    int value = cJSONToInt(object->child, 16);
    EXPECT_EQ(value, 22);
    value = cJSONToInt(object, 16);
    EXPECT_EQ(value, 0);
    cJSON* object2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(object, "Register", 2);
    value = cJSONToInt(object2->child, 16);
    EXPECT_EQ(value, 0);
    cJSON* object3 = NULL;
    value = cJSONToInt(object3, 16);
    EXPECT_EQ(value, 0);
}

TEST(UtilsTestFixture, isPostResetFlow)
{
    bool val;
    val = isPostResetFlow("IERR.PostReset");
    ASSERT_TRUE(val == true);
    val = isPostResetFlow("ERR2.PostReset");
    ASSERT_TRUE(val == true);
    val = isPostResetFlow("abc");
    ASSERT_TRUE(val == false);
    val = isPostResetFlow("IERR.PostReset.Extra");
    ASSERT_TRUE(val == true);
    val = isPostResetFlow("IERR.Post");
    ASSERT_TRUE(val == false);
    val = isPostResetFlow("ERR2.PostReset.Extra");
    ASSERT_TRUE(val == true);
    val = isPostResetFlow("ERR2.Post");
    ASSERT_TRUE(val == false);
    val = isPostResetFlow(NULL);
    ASSERT_TRUE(val == false);
}

TEST(UtilsTestFixture, isErrorFlow)
{
    bool val;
    // Ensure we catch PostReset before errors
    val = isErrorFlow("IERR.PostReset");
    ASSERT_TRUE(val == false);
    val = isErrorFlow("ERR2.PostReset");
    ASSERT_TRUE(val == false);

    val = isErrorFlow("abc");
    ASSERT_TRUE(val == false);
    val = isErrorFlow("IERR.PostReset.Extra");
    ASSERT_TRUE(val == false);
    val = isErrorFlow("IERR.Post");
    ASSERT_TRUE(val == true);
    val = isErrorFlow("ERR2.Post.Extra");
    ASSERT_TRUE(val == true);
    val = isErrorFlow("ERR2");
    ASSERT_TRUE(val == true);
    val = isErrorFlow(NULL);
    ASSERT_TRUE(val == false);
}

// TODO: fix this test
TEST(UtilsTestFixture, DISABLED_getTriggerType)
{
    STriggerType trig;
    // Ensure we catch PostReset before errors
    trig = getTriggerType("IERR.PostReset");
    ASSERT_TRUE(trig == TRIGGER_POST_RESET);
    trig = getTriggerType("ERR2.PostReset");
    ASSERT_TRUE(trig == TRIGGER_POST_RESET);

    trig = getTriggerType("abc");
    ASSERT_TRUE(trig == TRIGGER_ONDEMAND_TELEMETRY);
    trig = getTriggerType("IERR.PostReset.Extra");
    ASSERT_TRUE(trig == TRIGGER_POST_RESET);
    trig = getTriggerType("IERR.Post");
    ASSERT_TRUE(trig == TRIGGER_IERR);
    trig = getTriggerType("ERR2.Post.Extra");
    ASSERT_TRUE(trig == TRIGGER_ERR2);
    trig = getTriggerType("ERR2");
    ASSERT_TRUE(trig == TRIGGER_ERR2);
    trig = getTriggerType(NULL);
    ASSERT_TRUE(trig == TRIGGER_UNKNOWN);
}

// TODO: Fix this test
TEST(UtilsTestFixture, DISABLED_GNR_getFlagFromSection_LoopOnCompute_test)
{
    cJSON* content = readInputFile("../crashdump_input_gnr.json");
    char* flagStr = "LoopOnCompute";
    if (content == NULL)
    {
        ASSERT_TRUE(false) << "crashdump_input_gnr.json not found";
    }

    bool val = getFlagFromSection(content, 0, flagStr);
    ASSERT_TRUE(val == false);
    val = getFlagFromSection(content, 1, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 2, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 3, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 4, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 5, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 6, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 7, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 8, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 9, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 10, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 11, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 12, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 13, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 14, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 15, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 16, flagStr);
    ASSERT_TRUE(val == true);

    // Delete LoopOnCompute from first section and val should return true
    cJSON* obj = getJsonObjectFromPath("Sections", content);
    cJSON* aSection = cJSON_GetArrayItem(obj, 0);
    cJSON_DeleteItemFromObject(aSection->child, flagStr);
    val = getFlagFromSection(content, 0, flagStr);
    ASSERT_TRUE(val == true);

    aSection = cJSON_GetArrayItem(obj, 1);
    cJSON* cjsonFlag = cJSON_Parse(R"({"LoopOnCompute": false})");
    cJSON_ReplaceItemInObject(aSection->child, flagStr, cjsonFlag);
    val = getFlagFromSection(content, 1, flagStr);
    ASSERT_TRUE(val == false);
}
