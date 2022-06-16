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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(UtilsTestFixture, getCrashDataSection_positive_enable_true)
{
    cJSON* root = cJSON_Parse(" \
        { \
            \"crash_data\": { \
                \"MCA\": { \
                    \"_record_enable\" : true \
                } \
            } \
        } \
    ");

    bool enable;
    cJSON* child = getCrashDataSection(root, "MCA", &enable);

    EXPECT_EQ(enable, true);
    EXPECT_NE(child, nullptr);
}

TEST(UtilsTestFixture, getCrashDataSection_enable_false)
{
    cJSON* root = cJSON_Parse(" \
        { \
            \"crash_data\": { \
                \"MCA\": { \
                    \"_record_enable\" : false \
                } \
            } \
        } \
    ");

    bool enable;
    cJSON* child = getCrashDataSection(root, "MCA", &enable);

    EXPECT_EQ(enable, false);
    EXPECT_NE(child, nullptr);
}

TEST(UtilsTestFixture, getCrashDataSection_norecord_enable_enable_true)
{
    cJSON* root = cJSON_Parse(" \
        { \
            \"crash_data\": { \
                \"MCA\": { \
                } \
            } \
        } \
    ");

    bool enable;
    cJSON* child = getCrashDataSection(root, "MCA", &enable);

    EXPECT_EQ(enable, true);
    EXPECT_NE(child, nullptr);
}

TEST(UtilsTestFixture, getCrashDataSection_norecord_enable_false)
{
    cJSON* root = cJSON_Parse(" \
        { \
            \"crash_data\": { \
            } \
        } \
    ");

    bool enable;
    cJSON* child = getCrashDataSection(root, "MCA", &enable);

    EXPECT_EQ(enable, false);
    EXPECT_EQ(child, nullptr);
}

TEST(UtilsTestFixture, getCrashDataSectionRegList_enable_true)
{
    cJSON* root = cJSON_Parse(" \
        { \
            \"crash_data\": { \
                \"MCA\": { \
                    \"_record_enable\" : true, \
                    \"core\": { \
                        \"reg_list\": [ \
                        ] \
                    } \
                } \
            } \
        } \
    ");

    bool enable;
    cJSON* child = getCrashDataSectionRegList(root, "MCA", "core", &enable);

    EXPECT_EQ(enable, true);
    EXPECT_NE(child, nullptr);
}

TEST(UtilsTestFixture, getCrashDataSectionRegList_enable_false)
{
    cJSON* root = cJSON_Parse(" \
        { \
            \"crash_data\": { \
                \"MCA\": { \
                    \"_record_enable\" : false, \
                    \"core\": { \
                        \"reg_list\": [ \
                        ] \
                    } \
                } \
            } \
        } \
    ");

    bool enable;
    cJSON* child = getCrashDataSectionRegList(root, "MCA", "core", &enable);

    EXPECT_EQ(enable, false);
    EXPECT_NE(child, nullptr);
}

TEST(UtilsTestFixture, getCrashDataSectionRegList_noreglist_enable_true)
{
    cJSON* root = cJSON_Parse(" \
        { \
            \"crash_data\": { \
                \"MCA\": { \
                    \"_record_enable\" : true, \
                    \"core\": { \
                    } \
                } \
            } \
        } \
    ");

    bool enable;
    cJSON* child = getCrashDataSectionRegList(root, "MCA", "core", &enable);

    EXPECT_EQ(enable, true);
    EXPECT_EQ(child, nullptr);
}

TEST(UtilsTestFixture, getCrashDataSectionVersion_enable_true)
{
    cJSON* root = cJSON_Parse(" \
        { \
            \"crash_data\": { \
                \"uncore\": { \
                    \"_record_enable\": true, \
                    \"_version\": \"0x13\" \
                } \
            } \
        } \
    ");

    int version = getCrashDataSectionVersion(root, "uncore");

    EXPECT_EQ(version, 0x13);
}

TEST(UtilsTestFixture, getCrashDataSectionVersion_enable_false)
{
    cJSON* root = cJSON_Parse(" \
        { \
            \"crash_data\": { \
                \"uncore\": { \
                    \"_record_enable\": false, \
                    \"_version\": \"0x13\" \
                } \
            } \
        } \
    ");

    int version = getCrashDataSectionVersion(root, "uncore");

    EXPECT_EQ(version, 0x13);
}

TEST(UtilsTestFixture, getCrashDataSectionVersion_wrong_version_enable_true)
{
    cJSON* root = cJSON_Parse(" \
        { \
            \"crash_data\": { \
                \"uncore\": { \
                    \"_record_enable\": true, \
                    \"_version\": \"0x43M10\" \
                } \
            } \
        } \
    ");

    int version = getCrashDataSectionVersion(root, "uncore");

    EXPECT_EQ(version, 0x43);
}
