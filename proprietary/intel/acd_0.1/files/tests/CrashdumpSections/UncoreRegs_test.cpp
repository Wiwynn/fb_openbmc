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
#include "CrashdumpSections/UncoreRegs.hpp"

#include "gtest/gtest.h"

TEST(UncoreRegs, sizeCheck)
{
    // Check Num of PCI registers
    int expected = 3212;
    int val = sizeof(sUncoreStatusPciICX1) / sizeof(SUncoreStatusRegPci);
    EXPECT_EQ(val, expected);

    // Check Num of MMIO registers
    val =
        sizeof(sUncoreStatusPciMmioICX1) / sizeof(SUncoreStatusRegPciMmioICX1);
    expected = 1326;
    EXPECT_EQ(val, expected);
}

TEST(UncoreRegs, read_and_compare_crashdump_input_icx_json_files)
{
    cJSON* root = NULL;
    cJSON* expected = NULL;
    cJSON* logSection = NULL;
    char* buffer = NULL;

    buffer = readTestFile("/usr/share/crashdump/crashdump_input_icx.json");
    root = cJSON_Parse(buffer);
    buffer = readTestFile("/tmp/crashdump_input/crashdump_input_icx.json");
    expected = cJSON_Parse(buffer);

    EXPECT_EQ(true, cJSON_Compare(expected, root, true));

    logSection = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(expected, "crash_data"), "uncore");

    EXPECT_TRUE(logSection != NULL);

    cJSON_Delete(root);
    cJSON_Delete(expected);
    FREE(buffer);
}
