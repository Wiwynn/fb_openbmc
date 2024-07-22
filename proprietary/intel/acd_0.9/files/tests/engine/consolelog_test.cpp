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

#include "engine/crashdump.h"

extern "C" {
#include "engine/utils.h"
}

#include "tests/test_utils.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

// #define DEBUG_FLAG

// USE CRASHDUMP_PRINT() for consolelog testing.
TEST(ConsoleLogTestFixture, consolelog_limit_test)
{
    initConsoleLog();

    for (int i = 0; i < CONSOLE_MAX_LINE_COUNT + 5; i++)
    {
        CRASHDUMP_PRINT(INFO, stderr,
                        "Testing logging mechanism for consolelog: %d \n", i);
    }

    EXPECT_TRUE(console.lineCount == CONSOLE_MAX_LINE_COUNT);
#ifdef DEBUG_FLAG
    outputDebugFile(console.rootObject, "out-consolelog_limit.json", false);
#endif
}

TEST(ConsoleLogTestFixture, consolelog_event_test)
{
    // EVENT 1
    initConsoleLog();

    for (int i = 0; i < CONSOLE_MAX_LINE_COUNT; i++)
    {
        CRASHDUMP_PRINT(
            INFO, stderr,
            "Testing logging mechanism for consolelog Event 1:%d \n", i);
    }

    EXPECT_TRUE(console.lineCount == CONSOLE_MAX_LINE_COUNT);

    // EVENT 2
    initConsoleLog();

    // Ensure line Reset
    EXPECT_TRUE(console.lineCount == 0);
    // Ensure cjson object reset
    EXPECT_TRUE(console.rootObject->child == NULL);

    for (int i = 0; i < CONSOLE_MAX_LINE_COUNT; i++)
    {
        CRASHDUMP_PRINT(
            INFO, stderr,
            "Testing logging mechanism for consolelog Event 2:%d \n", i);
    }

    EXPECT_TRUE(console.lineCount == CONSOLE_MAX_LINE_COUNT);
}

TEST(ConsoleLogTestFixture, consolelog_line_limit_test)
{
    char extraLongLine[CONSOLE_MAX_STRING_LEN];
    initConsoleLog();

    std::fill_n(extraLongLine, CONSOLE_MAX_STRING_LEN - 1, 'a');

    // TimeStamp will be added internally; Should not find the original string
    // as it will be cutoff
    CRASHDUMP_PRINT(INFO, stderr, "%s\n", extraLongLine);
    size_t match = std::string(console.lineStr).find(extraLongLine);

    EXPECT_TRUE(match == std::string::npos);

#ifdef DEBUG_FLAG
    outputDebugFile(console.rootObject, "out-consolelog_line_limit.json",
                    false);
#endif
}
