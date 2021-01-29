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
#include "CrashdumpSections/SqDump.hpp"
#include "crashdump.hpp"

#include <safe_mem_lib.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace ::testing;
using ::testing::Return;
using namespace crashdump;

void sqDumpJson(uint32_t u32CoreNum, SSqDump* psSqDump, cJSON* pJsonChild);

class SqDumpMOCK
{
  public:
    virtual ~SqDumpMOCK()
    {
    }
};

class SqDumpTestFixture : public Test
{
  public:
    SqDumpTestFixture()
    {
        SqDumpMock.reset(new NiceMock<SqDumpMOCK>());
    }

    ~SqDumpTestFixture()
    {
        FREE(psSqDump->pu32SqCtrlArray);
        FREE(psSqDump->pu32SqAddrArray);
        FREE(psSqDump->puSqAddrRet);
        FREE(psSqDump->pu8SqAddrCc);
        FREE(psSqDump->puSqCtrlRet);
        FREE(psSqDump->pu8SqCtrlCc);
        FREE(psSqDump);
        SqDumpMock.reset();
    }

    void SetUp() override
    {

        // Initialize json object
        root = cJSON_CreateObject();
        psSqDump = (SSqDump*)malloc(sizeof(SSqDump));
        psSqDump->pu32SqCtrlArray = (uint32_t*)malloc(8 * sizeof(uint32_t));
        psSqDump->pu32SqAddrArray = (uint32_t*)malloc(8 * sizeof(uint32_t));
        psSqDump->puSqAddrRet = (int*)malloc(8 * sizeof(int));
        psSqDump->pu8SqAddrCc = (uint8_t*)malloc(8 * sizeof(uint8_t));
        psSqDump->puSqCtrlRet = (int*)malloc(8 * sizeof(int));
        psSqDump->pu8SqCtrlCc = (uint8_t*)malloc(8 * sizeof(uint8_t));
    }

    cJSON* root = NULL;
    char* jsonStr = NULL;
    SSqDump* psSqDump = NULL;

    static std::unique_ptr<SqDumpMOCK> SqDumpMock;
};

std::unique_ptr<SqDumpMOCK> SqDumpTestFixture::SqDumpMock;

#ifdef MOCK

#endif // MOCK

TEST_F(SqDumpTestFixture, sqDumpJson_ctrl_bigger_addr)
{
    psSqDump->pu32SqAddrArray[0] = 0x1d;
    psSqDump->pu32SqAddrArray[1] = 0x0;
    psSqDump->pu32SqAddrArray[2] = 0x0;
    psSqDump->pu32SqAddrArray[3] = 0x0;
    psSqDump->pu32SqAddrArray[4] = 0xFFFFFFFF;
    psSqDump->pu32SqAddrArray[5] = 0xEEEEEEEE;
    psSqDump->pu32SqCtrlArray[0] = 0x400040;
    psSqDump->pu32SqCtrlArray[1] = 0x400003;
    psSqDump->pu32SqCtrlArray[2] = 0x400040;
    psSqDump->pu32SqCtrlArray[3] = 0x400003;
    psSqDump->pu32SqCtrlArray[4] = 0xAAAAAAAA;
    psSqDump->pu32SqCtrlArray[5] = 0xCCCCCCCC;
    psSqDump->pu32SqCtrlArray[6] = 0xCCCCCCCC;
    psSqDump->u32SqCtrlSize = 6;
    psSqDump->u32SqAddrSize = 5;
    uint32_t numcore = 1;

    sqDumpJson(numcore, psSqDump, root);
    jsonStr = cJSON_Print(root);
    printf("%s\n", jsonStr);
}

TEST_F(SqDumpTestFixture, sqDumpJson_addr_bigger_ctrl)
{
    psSqDump->pu32SqAddrArray[0] = 0x1d;
    psSqDump->pu32SqAddrArray[1] = 0x0;
    psSqDump->pu32SqAddrArray[2] = 0x0;
    psSqDump->pu32SqAddrArray[3] = 0x0;
    psSqDump->pu32SqAddrArray[4] = 0xFFFFFFFF;
    psSqDump->pu32SqAddrArray[5] = 0xEEEEEEEE;
    psSqDump->pu32SqCtrlArray[0] = 0x400040;
    psSqDump->pu32SqCtrlArray[1] = 0x400003;
    psSqDump->pu32SqCtrlArray[2] = 0x400040;
    psSqDump->pu32SqCtrlArray[3] = 0x400003;
    psSqDump->pu32SqCtrlArray[4] = 0xAAAAAAAA;
    psSqDump->pu32SqCtrlArray[5] = 0xBBBBBBBB;
    psSqDump->pu32SqCtrlArray[5] = 0xCCCCCCCC;
    psSqDump->u32SqCtrlSize = 4;
    psSqDump->u32SqAddrSize = 6;
    uint32_t numcore = 1;

    sqDumpJson(numcore, psSqDump, root);
    jsonStr = cJSON_Print(root);
    printf("%s\n", jsonStr);
}

TEST_F(SqDumpTestFixture, sqDumpJson_addr_same_ctrl)
{
    psSqDump->pu32SqAddrArray[0] = 0xAAAAAAAA;
    psSqDump->pu32SqAddrArray[1] = 0xBBBBBBBB;
    psSqDump->pu32SqAddrArray[2] = 0xCCCCCCCC;
    psSqDump->pu32SqAddrArray[3] = 0xDDDDDDDD;
    psSqDump->pu32SqAddrArray[4] = 0xEEEEEEEE;
    psSqDump->pu32SqAddrArray[5] = 0xFFFFFFFF;
    psSqDump->pu32SqAddrArray[6] = 0x99999999;
    psSqDump->pu32SqCtrlArray[0] = 0x88888888;
    psSqDump->pu32SqCtrlArray[1] = 0x77777777;
    psSqDump->pu32SqCtrlArray[2] = 0x66666666;
    psSqDump->pu32SqCtrlArray[3] = 0x55555555;
    psSqDump->pu32SqCtrlArray[4] = 0x44444444;
    psSqDump->pu32SqCtrlArray[5] = 0x33333333;
    psSqDump->pu32SqCtrlArray[6] = 0x22222222;
    psSqDump->u32SqCtrlSize = 5;
    psSqDump->u32SqAddrSize = 5;
    uint32_t numcore = 1;

    sqDumpJson(numcore, psSqDump, root);
    jsonStr = cJSON_Print(root);
    printf("%s\n", jsonStr);
}

TEST_F(SqDumpTestFixture, sqDumpJson_null_addr)
{
    uint32_t numcore = 1;

    sqDumpJson(numcore, psSqDump, root);
    jsonStr = cJSON_Print(root);
    printf("%s\n", jsonStr);
}

TEST_F(SqDumpTestFixture, sqDumpJson_null_ctrl)
{
    psSqDump->pu32SqAddrArray[0] = 0xAAAAAAAA;
    psSqDump->pu32SqAddrArray[1] = 0xBBBBBBBB;
    psSqDump->pu32SqAddrArray[2] = 0xCCCCCCCC;
    psSqDump->pu32SqAddrArray[3] = 0xDDDDDDDD;
    psSqDump->pu32SqAddrArray[4] = 0xEEEEEEEE;
    psSqDump->pu32SqAddrArray[5] = 0xFFFFFFFF;
    psSqDump->pu32SqAddrArray[6] = 0x99999999;
    psSqDump->u32SqAddrSize = 5;
    uint32_t numcore = 1;

    sqDumpJson(numcore, psSqDump, root);
    jsonStr = cJSON_Print(root);
    printf("%s\n", jsonStr);
}