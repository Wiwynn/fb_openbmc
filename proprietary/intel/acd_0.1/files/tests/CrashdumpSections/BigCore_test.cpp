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
#include "CrashdumpSections/BigCore.hpp"
#include "crashdump.hpp"

#include <safe_mem_lib.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace ::testing;
using ::testing::Return;
using namespace crashdump;

int logCrashdumpICX1(crashdump::CPUInfo& cpuInfo, cJSON* pJsonChild);
void crashdumpJsonICX1(uint32_t u32CoreNum, uint32_t u32ThreadNum,
                       uint32_t u32CrashSize, uint32_t u32NumReads,
                       uint8_t* pu8Crashdump, cJSON* pJsonChild, uint8_t cc,
                       int retval);

class BigCoreMOCK
{
  public:
    virtual ~BigCoreMOCK()
    {
    }

    MOCK_METHOD8(peci_CrashDump_Discovery,
                 EPECIStatus(uint8_t, uint8_t, uint8_t, uint16_t, uint8_t,
                             uint8_t, uint8_t*, uint8_t*));
};

class BigCoreTestFixture : public Test
{
  public:
    BigCoreTestFixture()
    {
        bigCoreMock = std::make_unique<NiceMock<BigCoreMOCK>>();
    }

    ~BigCoreTestFixture()
    {
        bigCoreMock.reset();
    }

    void SetUp() override
    {
        // Initialize cpuInfo
        cpuInfo.clientAddr = 48;
        cpuInfo.model = cpu::icx;
        cpuInfo.coreMask = 0x0000db7e;
        cpuInfo.chaCount = 0;
        cpuInfo.crashedCoreMask = 0x0;

        // Initialize json object
        root = cJSON_CreateObject();
    }

    CPUInfo cpuInfo = {};
    cJSON* root = NULL;
    char* jsonStr = NULL;

    static std::unique_ptr<BigCoreMOCK> bigCoreMock;
};

std::unique_ptr<BigCoreMOCK> BigCoreTestFixture::bigCoreMock;

#ifdef MOCK
EPECIStatus peci_CrashDump_Discovery(uint8_t target, uint8_t subopcode,
                                     uint8_t param0, uint16_t param1,
                                     uint8_t param2, uint8_t u8ReadLen,
                                     uint8_t* pData, uint8_t* cc)
{
    // minimal mocking
    uint8_t ccode = 0;
    uint8_t data[8] = {0};
    memcpy_s(pData, 8, data, 8);
    *cc = ccode;
    return BigCoreTestFixture::bigCoreMock->peci_CrashDump_Discovery(
        target, subopcode, param0, param1, param2, u8ReadLen, pData, cc);
}
#endif // MOCK

TEST_F(BigCoreTestFixture, logCrashdumpICX1_1st_return)
{
    int ret;
    int expected = 1;
    uint8_t u8CrashdumpDisabled = ICX_A0_CRASHDUMP_DISABLED;

#ifdef MOCK
    EXPECT_CALL(*bigCoreMock, peci_CrashDump_Discovery)
        .WillOnce(DoAll(SetArgPointee<6>(u8CrashdumpDisabled),
                        Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArgPointee<6>(u8CrashdumpDisabled),
                        Return(PECI_CC_DRIVER_ERR)));
#endif // MOCK

    NTIME(2)
    {
        ret = logCrashdumpICX1(cpuInfo, root);
        EXPECT_EQ(ret, expected);
    }
}

TEST_F(BigCoreTestFixture, logCrashdumpICX1_2nd_return)
{
    int ret;
    int expected = 1;

    uint8_t u8CrashdumpDisabled = ICX_A0_CRASHDUMP_ENABLED;
    uint16_t u16CrashdumpNumAgents = 15;
    uint8_t cc = 0x90;

#ifdef MOCK
    EXPECT_CALL(*bigCoreMock, peci_CrashDump_Discovery)
        .WillOnce(DoAll(SetArgPointee<6>(u8CrashdumpDisabled),
                        Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArgPointee<6>(u16CrashdumpNumAgents),
                        SetArgPointee<7>(cc), Return(PECI_CC_DRIVER_ERR)));
#endif // MOCK

    ret = logCrashdumpICX1(cpuInfo, root);
    EXPECT_EQ(ret, expected);
}

TEST_F(BigCoreTestFixture, crashdumpJsonICX1_print_json)
{
    uint64_t* pu64Crashdump = (uint64_t*)(calloc(5, 1));
    crashdumpJsonICX1(1, 1, 1, 5, (uint8_t*)pu64Crashdump, root, 0x40, 0);
    jsonStr = cJSON_Print(root);
    printf("%s\n", jsonStr);
    FREE(pu64Crashdump);
}
