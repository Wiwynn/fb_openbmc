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

#include "../test_utils.hpp"
#include "CrashdumpSections/Uncore.hpp"
#include "crashdump.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace ::testing;
using ::testing::Return;
using namespace crashdump;

int uncoreStatusPciICX(crashdump::CPUInfo& cpuInfo, cJSON* pJsonChild);
int uncoreStatusPciMmioICX(crashdump::CPUInfo& cpuInfo, cJSON* pJsonChild);
int uncoreStatusPciCPX(crashdump::CPUInfo& cpuInfo, cJSON* pJsonChild);
int uncoreStatusPciMmioCPX(crashdump::CPUInfo& cpuInfo, cJSON* pJsonChild);
cJSON* readInputFile(const char* filename);
cJSON* selectAndReadInputFile(crashdump::cpu::Model cpuModel, char** filename);

// change to ifdef or adjust CMakeList.txt to test against hardware
#ifndef MOCK

class UncoreTestFixture : public Test
{
  public:
    void SetUp() override
    {
        // Build a list of cpus
        info.model = crashdump::cpu::icx;
        cpus.push_back(info);
        info.model = crashdump::cpu::icx;
        cpus.push_back(info);
        info.model = crashdump::cpu::cpx;
        cpus.push_back(info);

        // Initialize json object
        root = cJSON_CreateObject();
    }

    void TearDown() override
    {
        FREE(jsonStr);
        cJSON_Delete(root);
        cJSON_Delete(ut);
    }

    CPUInfo info = {};
    std::vector<CPUInfo> cpus;
    InputFileInfo inputFileInfo = {
        .unique = true, .filenames = {NULL}, .buffers = {NULL}};
    cJSON* root = NULL;
    cJSON* ut = NULL;
    char* jsonStr = NULL;
    char* defaultFile = NULL;
    char* overrideFile = NULL;
};

TEST_F(UncoreTestFixture, getCrashDataSection)
{
    bool enable = false;
    inputFileInfo.buffers[0] =
        selectAndReadInputFile(cpus[0].model, inputFileInfo.filenames);
    cJSON* section =
        getCrashDataSection(inputFileInfo.buffers[0], "uncore", &enable);
    EXPECT_TRUE(enable);
}

TEST_F(UncoreTestFixture, getCrashDataSectionVersion)
{
    inputFileInfo.buffers[0] =
        selectAndReadInputFile(cpus[0].model, inputFileInfo.filenames);
    int version =
        getCrashDataSectionVersion(inputFileInfo.buffers[0], "uncore");
    EXPECT_EQ(version, 0x21);
}

TEST_F(UncoreTestFixture, uncoreStatusPciICX)
{
    ut = readInputFile("/tmp/crashdump_input/ut_uncore_sample_icx.json");
    inputFileInfo.buffers[0] =
        selectAndReadInputFile(cpus[0].model, inputFileInfo.filenames);

    uncoreStatusPciICX(cpus[0], root);

    cJSON* pci = cJSON_GetObjectItemCaseSensitive(ut, "pci");
    cJSON* expected = NULL;
    cJSON* actual = NULL;

    cJSON_ArrayForEach(expected, pci)
    {
        actual = cJSON_GetObjectItemCaseSensitive(root, expected->string);
        EXPECT_TRUE(cJSON_Compare(actual, expected, true))
            << "Not match!\n"
            << "expected - " << expected->string << ":" << expected->valuestring
            << "\n"
            << "actual - " << actual->string << ":" << actual->valuestring;
    }
}

TEST_F(UncoreTestFixture, uncoreStatusPciMmioICX)
{
    ut = readInputFile("/tmp/crashdump_input/ut_uncore_sample_icx.json");
    inputFileInfo.buffers[0] =
        selectAndReadInputFile(cpus[0].model, inputFileInfo.filenames);

    uncoreStatusPciMmioICX(cpus[0], root);

    cJSON* mmio = cJSON_GetObjectItemCaseSensitive(ut, "mmio");
    cJSON* expected = NULL;
    cJSON* actual = NULL;

    cJSON_ArrayForEach(expected, mmio)
    {
        actual = cJSON_GetObjectItemCaseSensitive(root, expected->string);
        EXPECT_TRUE(cJSON_Compare(actual, expected, true))
            << "Not match!\n"
            << "expected - " << expected->string << ":" << expected->valuestring
            << "\n"
            << "actual - " << actual->string << ":" << actual->valuestring;
    }
}

TEST_F(UncoreTestFixture, uncoreStatusPciCPX)
{
    ut = readInputFile("/tmp/crashdump_input/ut_uncore_sample_cpx.json");
    inputFileInfo.buffers[0] =
        selectAndReadInputFile(cpus[0].model, inputFileInfo.filenames);

    uncoreStatusPciCPX(cpus[0], root);

    cJSON* pci = cJSON_GetObjectItemCaseSensitive(ut, "pci");
    cJSON* expected = NULL;
    cJSON* actual = NULL;

    cJSON_ArrayForEach(expected, pci)
    {
        actual = cJSON_GetObjectItemCaseSensitive(root, expected->string);
        EXPECT_TRUE(cJSON_Compare(actual, expected, true))
            << "Not match!\n"
            << "expected - " << expected->string << ":" << expected->valuestring
            << "\n"
            << "actual - " << actual->string << ":" << actual->valuestring;
    }
}

TEST_F(UncoreTestFixture, uncoreStatusPciMmioCPX)
{
    ut = readInputFile("/tmp/crashdump_input/ut_uncore_sample_cpx.json");
    inputFileInfo.buffers[0] =
        selectAndReadInputFile(cpus[0].model, inputFileInfo.filenames);

    uncoreStatusPciMmioCPX(cpus[0], root);

    cJSON* mmio = cJSON_GetObjectItemCaseSensitive(ut, "mmio");
    cJSON* expected = NULL;
    cJSON* actual = NULL;

    cJSON_ArrayForEach(expected, mmio)
    {
        actual = cJSON_GetObjectItemCaseSensitive(root, expected->string);
        EXPECT_TRUE(cJSON_Compare(actual, expected, true))
            << "Not match!\n"
            << "expected - " << expected->string << ":" << expected->valuestring
            << "\n"
            << "actual - " << actual->string << ":" << actual->valuestring;
    }
}

#endif
