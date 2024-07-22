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
 ******************************************************************************/

#include "../mock/test_crashdump.hpp"
#include "tests/test_utils.hpp"

#include "gtest/gtest.h"

extern "C" {
#include "engine/crashdump.h"
#include "engine/tpmi.h"
}

// Test fixture for TPMI tests
class TpmiTestFixture : public ::testing::Test
{
  protected:
    // You can remove any or all of the following functions if they're not
    // needed

    TpmiTestFixture()
    {
    }

    ~TpmiTestFixture() override
    {
    }

    // If the constructor and destructor aren't enough for setting up
    // and cleaning up each test, you can define the following methods
    void SetUp() override
    {
        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(cpus, sizeof(cpus), 0, sizeof(cpus));
        }
    }

    void TearDown() override
    {
    }

  public:
    CPUInfo cpus[MAX_CPUS];
};

TEST_F(TpmiTestFixture, ValidatePhy2LogTableWithNoDuplicatesTest_Core)
{
    PHY2LOGRead phy2logRead = {};
    uint8_t* phy2logBuffer = (uint8_t*)phy2logRead.table;
    for (int i = 0; i < GNR_MAX_ACTIVE_DIES; i++)
    {
        for (int j = 0; j < GNR_PHY2LOG_MAX_INDICES; j++)
        {
            for (int k = 0; k < MB_IDS_PER_INDEX; k++)
            {
                // Give every byte a unique number
                phy2logBuffer[(i * GNR_PHY2LOG_MAX_INDICES * MB_IDS_PER_INDEX +
                               j * MB_IDS_PER_INDEX) +
                              k] =
                    (i * GNR_PHY2LOG_MAX_INDICES * MB_IDS_PER_INDEX +
                     j * MB_IDS_PER_INDEX) +
                    k;
            }
        }
    }

    // Call the function with the test data
    bool result =
        ValidatePhy2LogTable(&phy2logRead, GNR_MAX_ACTIVE_DIES, CORE_TYPE);

    // The table is valid, so the result should be true
    EXPECT_TRUE(result);
}

TEST_F(TpmiTestFixture, ValidatePhy2LogTableWithDuplicatesTest_Core)
{
    // Initialize a PHY2LOGRead struct with duplicate values
    PHY2LOGRead phy2logRead = {};
    for (int i = 0; i < GNR_MAX_ACTIVE_DIES; i++)
    {
        for (int j = 0; j < GNR_PHY2LOG_MAX_INDICES; j++)
        {
            // fill with valid table
            phy2logRead.table[i][0] = 0x120f010e;
            phy2logRead.table[i][1] = 0x1614302c;
            phy2logRead.table[i][2] = 0x2d02101a;
            phy2logRead.table[i][3] = 0x6171531;
            phy2logRead.table[i][4] = 0x322e0300;
            phy2logRead.table[i][5] = 0x111b1804;
            phy2logRead.table[i][6] = 0x5332f13;
            phy2logRead.table[i][7] = 0x9071c19;
            phy2logRead.table[i][8] = 0x25233834;
            phy2logRead.table[i][9] = 0x35201d29;
            phy2logRead.table[i][10] = 0xd260b39;
            phy2logRead.table[i][11] = 0x3a36211e;
            phy2logRead.table[i][12] = 0x82a270c;
            phy2logRead.table[i][13] = 0x243b3722;
            phy2logRead.table[i][14] = 0xa1f2b28;
            phy2logRead.table[i][15] = 0x3f3e3d3c;
        }
    }

    // fill with one duplicate value
    phy2logRead.table[1][0] = 0;
    phy2logRead.table[1][6] = 0;

    // Call the function with the test data
    bool result =
        ValidatePhy2LogTable(&phy2logRead, GNR_MAX_ACTIVE_DIES, CORE_TYPE);

    // The table is not valid, so the result should be false
    EXPECT_FALSE(result);
}

TEST_F(TpmiTestFixture, ValidatePhy2LogTableWithDuplicatesTest_CHA)
{
    // Initialize a PHY2LOGRead struct with duplicate values
    PHY2LOGRead phy2logRead = {};
    for (int i = 0; i < GNR_MAX_ACTIVE_DIES; i++)
    {
        for (int j = 0; j < GNR_PHY2LOG_MAX_INDICES; j++)
        {
            // Fill the table with invalid values
            phy2logRead.table[i][j] = 0xFFFFFFFF;
        }
    }

    // Make a duplicate values
    phy2logRead.table[0][2] = 0xFF01FF02;
    phy2logRead.table[0][10] = 0xFF01FF03;

    // Call the function with the test data
    bool result =
        ValidatePhy2LogTable(&phy2logRead, GNR_MAX_ACTIVE_DIES, CHA_TYPE);

    // The table is not valid, so the result should be false
    EXPECT_FALSE(result);
}

TEST_F(TpmiTestFixture, ValidatePhy2LogTableNoResponseTest_CHA)
{
    PHY2LOGRead phy2logRead = {};
    for (int i = 0; i < GNR_MAX_ACTIVE_DIES; i++)
    {
        for (int j = 0; j < GNR_PHY2LOG_MAX_INDICES; j++)
        {
            // Fill the table with duplicate values
            phy2logRead.table[i][j] = 0x00000000;
        }
    }

    // Call the function with the test data
    bool result =
        ValidatePhy2LogTable(&phy2logRead, GNR_MAX_ACTIVE_DIES, CHA_TYPE);

    // The table is not valid, so the result should be false
    EXPECT_FALSE(result);
}

TEST_F(TpmiTestFixture, ValidatePhy2LogTableWithNoDuplicatesTest_CHA)
{
    PHY2LOGRead phy2logRead = {};
    uint8_t* phy2logBuffer = (uint8_t*)phy2logRead.table;
    for (int i = 0; i < GNR_MAX_ACTIVE_DIES; i++)
    {
        for (int j = 0; j < GNR_PHY2LOG_MAX_INDICES; j++)
        {
            for (int k = 0; k < MB_IDS_PER_INDEX; k++)
            {
                // Give every byte a unique number
                phy2logBuffer[(i * GNR_PHY2LOG_MAX_INDICES * MB_IDS_PER_INDEX +
                               j * MB_IDS_PER_INDEX) +
                              k] =
                    (i * GNR_PHY2LOG_MAX_INDICES * MB_IDS_PER_INDEX +
                     j * MB_IDS_PER_INDEX) +
                    k;
            }
        }
    }

    // Call the function with the test data
    bool result =
        ValidatePhy2LogTable(&phy2logRead, GNR_MAX_ACTIVE_DIES, CHA_TYPE);

    // The table is valid, so the result should be true
    EXPECT_TRUE(result);
}

// Test case for getBusEnumeration function
TEST_F(TpmiTestFixture, GetBusEnumerationTest)
{
    // Initialize variables
    BDFS bdfs;

    // These are don't care
    uint8_t clientAddr = 0;
    uint8_t busNumber = 0;
    uint8_t postEnumBus = 0;

    bdfs.bus = 8;
    bdfs.device = 3;
    bdfs.function = 0;
    bdfs.seg = 0;

    TestCrashdump crashdump(cd_gnr);

    int32_t busValid = 0xFFFFFFFF;    // all buses are valid
    int32_t busPostEnum = 0x04030201; // 4 buses; 1 byte for each

    // MAX_BUSNO
    bool result = getBusEnumeration(clientAddr, 30, &bdfs, &postEnumBus);
    EXPECT_FALSE(result);

    // Get bus 0
    manageBusEnum(crashdump, 1, busValid, busPostEnum);
    result = getBusEnumeration(clientAddr, 0, &bdfs, &postEnumBus);
    EXPECT_TRUE(result);
    EXPECT_TRUE(postEnumBus == 0x01);

    // Get bus 1
    manageBusEnum(crashdump, 1, busValid, busPostEnum);
    result = getBusEnumeration(clientAddr, 1, &bdfs, &postEnumBus);
    EXPECT_TRUE(result);
    EXPECT_TRUE(postEnumBus == 0x02);

    // Get bus 2
    manageBusEnum(crashdump, 1, busValid, busPostEnum);
    result = getBusEnumeration(clientAddr, 2, &bdfs, &postEnumBus);
    EXPECT_TRUE(result);
    EXPECT_TRUE(postEnumBus == 0x03);

    // Get bus 3
    manageBusEnum(crashdump, 1, busValid, busPostEnum);
    result = getBusEnumeration(clientAddr, 3, &bdfs, &postEnumBus);
    EXPECT_TRUE(result);
    EXPECT_TRUE(postEnumBus == 0x04);
}

// Test case for tpmi_getPHY2LOG Core functionality
TEST_F(TpmiTestFixture, TpmiGetPHY2LOGTest_Core)
{

    TestCrashdump crashdump(cd_gnr);

    // Point to core table
    PHY2LOGRead* phy2logRead = &cpus[0].phy2logReadCore;

    // Initialize a CPUInfo struct
    cpus[0].clientAddr = 0x30;
    cpus[0].dieMaskInfo.maskValid = true;
    phy2logRead->isValid = false;
    cpus[0].model = cd_gnr;

    PHY2LOG_Type type = CORE_TYPE;

    // Invalid Cpu addr; Can't detect model so its skipped to retry later
    cpus[0].clientAddr = 0;
    acdStatus result = tpmi_getPHY2LOG(cpus, STARTUP, type);
    EXPECT_TRUE(result == ACD_FAILURE);

    // Phy2log is Valid; Returns failure as it won't execute again
    cpus[0].clientAddr = 0x30;
    phy2logRead->isValid = true;
    result = tpmi_getPHY2LOG(cpus, STARTUP, type);
    EXPECT_TRUE(result == ACD_FAILURE);
    // Reset variable
    phy2logRead->isValid = false;

    // Diemask Invalid; Failure to retry if we get diemasks later
    cpus[0].dieMaskInfo.maskValid = false;
    result = tpmi_getPHY2LOG(cpus, STARTUP, type);
    EXPECT_TRUE(result == ACD_FAILURE);
    // Reset variable
    cpus[0].dieMaskInfo.maskValid = true;

    // Unsupported CPU; returns successful to avoid retries.
    cpus[0].model = cd_spr;
    result = tpmi_getPHY2LOG(cpus, STARTUP, type);
    EXPECT_TRUE(result == ACD_SUCCESS);
    // Reset variable
    cpus[0].model = cd_gnr;

    // Normal test case
    int32_t busValid = 0xFFFFFFFF;    // all buses are valid
    int32_t busPostEnum = 0x04030201; // 4 buses; 1 byte for each
    manageBusEnum(crashdump, 1, busValid, busPostEnum);
    managePHY2LOG(crashdump, 1, 3, 64);

    cpus[0].clientAddr = 0x30;
    cpus[0].dieMaskInfo.maskValid = true;
    phy2logRead->isValid = false;
    cpus[0].model = cd_gnr;
    cpus[0].dieMaskInfo.compute.effectiveMask = 0x07; // 3 compute
    result = tpmi_getPHY2LOG(cpus, STARTUP, type);
    EXPECT_TRUE(result == ACD_SUCCESS);
}

// Test case for tpmi_getPHY2LOG Cha functionality
TEST_F(TpmiTestFixture, TpmiGetPHY2LOGTest_CHA)
{

    TestCrashdump crashdump(cd_gnr);

    // Point to core table
    PHY2LOGRead* phy2logRead = &cpus[0].phy2logReadCHA;

    // Initialize a CPUInfo struct
    cpus[0].clientAddr = 0x30;
    cpus[0].dieMaskInfo.maskValid = true;
    phy2logRead->isValid = false;
    cpus[0].model = cd_gnr;

    PHY2LOG_Type type = CHA_TYPE;

    // Invalid Cpu addr; Can't detect model so its skipped to retry later
    cpus[0].clientAddr = 0;
    acdStatus result = tpmi_getPHY2LOG(cpus, STARTUP, type);
    EXPECT_TRUE(result == ACD_FAILURE);

    // Phy2log is Valid; Returns failure as it won't execute again
    cpus[0].clientAddr = 0x30;
    phy2logRead->isValid = true;
    result = tpmi_getPHY2LOG(cpus, STARTUP, type);
    EXPECT_TRUE(result == ACD_FAILURE);
    // Reset variable
    phy2logRead->isValid = false;

    // Diemask Invalid; Failure to retry if we get diemasks later
    cpus[0].dieMaskInfo.maskValid = false;
    result = tpmi_getPHY2LOG(cpus, STARTUP, type);
    EXPECT_TRUE(result == ACD_FAILURE);
    // Reset variable
    cpus[0].dieMaskInfo.maskValid = true;

    // Unsupported CPU; returns successful to avoid retries.
    cpus[0].model = cd_spr;
    result = tpmi_getPHY2LOG(cpus, STARTUP, type);
    EXPECT_TRUE(result == ACD_SUCCESS);
    // Reset variable
    cpus[0].model = cd_gnr;

    // Normal test case
    int32_t busValid = 0xFFFFFFFF;    // all buses are valid
    int32_t busPostEnum = 0x04030201; // 4 buses; 1 byte for each
    manageBusEnum(crashdump, 1, busValid, busPostEnum);
    managePHY2LOG_CHA(crashdump, 1, 3, 64);

    cpus[0].clientAddr = 0x30;
    cpus[0].dieMaskInfo.maskValid = true;
    phy2logRead->isValid = false;
    cpus[0].model = cd_gnr;
    cpus[0].dieMaskInfo.compute.effectiveMask = 0x07; // 3 compute
    result = tpmi_getPHY2LOG(cpus, STARTUP, type);
    EXPECT_TRUE(result == ACD_SUCCESS);
}
