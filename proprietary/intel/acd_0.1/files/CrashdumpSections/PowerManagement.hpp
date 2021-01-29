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

#pragma once

#include "crashdump.hpp"

#include <cjson/cJSON.h>
#include <stdint.h>

// PECI sequence
#define PM_CSTATE_PARAM 0x4660B4
#define PM_VID_PARAM 0x488004
#define PM_READ_PARAM 0x1019
#define PM_CORE_OFFSET 24

#define PM_JSON_STRING_LEN 32
#define PM_JSON_CORE_NAME "core%d"
#define PM_JSON_CSTATE_REG_NAME "c_state_reg"
#define PM_JSON_VID_REG_NAME "vid_ratio_reg"

#define PM_NA "N/A"
#define PM_UA "UA:0x%x"
#define PM_DF "DF:0x%x"
#define PM_UA_DF "UA:0x%x,DF:0x%x"
#define PM_UINT32_FMT "0x%" PRIx32 ""
#define PM_PCS_88 88
#define JSON_FAILURE 8

/******************************************************************************
 *
 *   Structures
 *
 ******************************************************************************/
typedef struct
{
    uint32_t u32CState;
    uint32_t u32VidRatio;
    uint8_t ccCstate;
    int retCstate;
    uint8_t ccVratio;
    int retVratio;
} SCpuPowerState;

typedef struct
{
    uint32_t uValue;
    bool bInvalid;

} SPmRegRawData;

typedef struct
{
    char regName[PM_JSON_STRING_LEN];
    bool isPerCore;
    uint16_t param;
} SPmEntry;

static const SPmEntry sPmEntries[] = {
    // Register, is per core, parameter
    {"pkgs_master_fsm", false, 0x1},
    {"pkgs_slave_fsm", false, 0x101},
    {"pkgs_misc", false, 0x201},
    {"pkgc_master_fsm", false, 0x2},
    {"pkgc_slave_fsm", false, 0x102},
    {"pkgc_misc", false, 0x202},
    {"llc_config", false, 0x3},
    {"current_state", true, 0x4},
    {"target_state", true, 0x5},
    {"request_state", true, 0x6},
    {"IO_UNCORE_PERF_STATUS", false, 0xa},
    {"IO_FIRMWARE_MCA_COMMAND", false, 0x8},
    {"RESOLVED_ICCP_MIN_LICENSE_LEVEL", false, 0x9},
    {"IO_WP_SVID0_CV_PS_MBVR0", false, 0x07},
    {"IO_WP_SVID1_CV_PS_MBVR0", false, 0x107},
    {"IO_WP_SVID2_CV_PS_MBVR0", false, 0x207},
    {"IO_WP_SVID3_CV_PS_MBVR0", false, 0x307},
    {"IO_WP_SVID4_CV_PS_MBVR0", false, 0x407},
    {"IO_WP_SVID5_CV_PS_MBVR0", false, 0x507},
    {"IO_WP_SVID6_CV_PS_MBVR0", false, 0x607},
};

typedef struct
{
    crashdump::cpu::Model cpuModel;
    int (*logPowerManagementVx)(crashdump::CPUInfo& cpuInfo, cJSON* pJsonChild);
} SPowerManagementVx;

int logPowerManagement(crashdump::CPUInfo& cpuInfo, cJSON* pJsonChild);
