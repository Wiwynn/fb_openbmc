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

#include "PowerManagement.hpp"

extern "C" {
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
}

#include "crashdump.hpp"

/******************************************************************************
 *
 *   powerManagementJson
 *
 *   This function formats the Power Management log into a JSON object
 *
 ******************************************************************************/
static void powerManagementJson(uint32_t u32CoreNum,
                                SCpuPowerState* sCpuPowerState,
                                cJSON* pJsonChild)
{
    cJSON* core;
    char jsonItemString[PM_JSON_STRING_LEN];

    // Add the core number item to the Power Management JSON structure
    cd_snprintf_s(jsonItemString, PM_JSON_STRING_LEN, PM_JSON_CORE_NAME,
                  u32CoreNum);
    cJSON_AddItemToObject(pJsonChild, jsonItemString,
                          core = cJSON_CreateObject());

    // Add the register data for this core to the Power Management JSON
    // structure
    if (sCpuPowerState->retCstate != PECI_CC_SUCCESS ||
        sCpuPowerState->retVratio != PECI_CC_SUCCESS)
    {
        cd_snprintf_s(jsonItemString, PM_JSON_STRING_LEN, PM_UA_DF,
                      sCpuPowerState->ccCstate, sCpuPowerState->retCstate);
        cJSON_AddStringToObject(core, PM_JSON_CSTATE_REG_NAME, jsonItemString);
        cd_snprintf_s(jsonItemString, PM_JSON_STRING_LEN, PM_UA_DF,
                      sCpuPowerState->ccVratio, sCpuPowerState->retVratio);
        cJSON_AddStringToObject(core, PM_JSON_VID_REG_NAME, jsonItemString);
    }
    else if (PECI_CC_UA(sCpuPowerState->ccCstate) ||
             PECI_CC_UA(sCpuPowerState->ccVratio))
    {
        cd_snprintf_s(jsonItemString, PM_JSON_STRING_LEN, PM_UA,
                      sCpuPowerState->ccCstate);
        cJSON_AddStringToObject(core, PM_JSON_CSTATE_REG_NAME, jsonItemString);
        cd_snprintf_s(jsonItemString, PM_JSON_STRING_LEN, PM_UA,
                      sCpuPowerState->ccVratio);
        cJSON_AddStringToObject(core, PM_JSON_VID_REG_NAME, jsonItemString);
    }
    else
    {
        cd_snprintf_s(jsonItemString, PM_JSON_STRING_LEN, "0x%x",
                      sCpuPowerState->u32CState);
        cJSON_AddStringToObject(core, PM_JSON_CSTATE_REG_NAME, jsonItemString);
        cd_snprintf_s(jsonItemString, PM_JSON_STRING_LEN, "0x%x",
                      sCpuPowerState->u32VidRatio);
        cJSON_AddStringToObject(core, PM_JSON_VID_REG_NAME, jsonItemString);
    }
}

/******************************************************************************
 *
 *   powerManagementReadReg
 *
 *   This function executes the PECI sequence to read the provided power
 *management register.
 *
 *   NOTE: As of now, this sequence requires DEBUG_ENABLE, so it may not work in
 *   production systems.
 *
 ******************************************************************************/
static int powerManagementReadReg(crashdump::CPUInfo& cpuInfo,
                                  uint32_t u32CoreNum, uint32_t u32RegParam,
                                  uint32_t* u32RegData, int peci_fd,
                                  uint8_t* ccData, int* retData)
{
    uint8_t cc = 0;
    int ret = 0;

    // Open the register read sequence
    ret = peci_WrPkgConfig_seq(cpuInfo.clientAddr, MBX_INDEX_VCU, VCU_OPEN_SEQ,
                               VCU_PWR_MGT_SEQ, sizeof(uint32_t), peci_fd, &cc);
    *ccData = cc;
    *retData = ret;
    if (ret != PECI_CC_SUCCESS)
    {
        // Reg read sequence failed, abort the sequence
        peci_WrPkgConfig_seq(cpuInfo.clientAddr, MBX_INDEX_VCU, VCU_ABORT_SEQ,
                             VCU_PWR_MGT_SEQ, sizeof(uint32_t), peci_fd, &cc);
        return ret;
    }

    // Set register number
    ret = peci_WrPkgConfig_seq(cpuInfo.clientAddr, MBX_INDEX_VCU, VCU_SET_PARAM,
                               (u32CoreNum << PM_CORE_OFFSET) | u32RegParam,
                               sizeof(uint32_t), peci_fd, &cc);
    *ccData = cc;
    *retData = ret;
    if (ret != PECI_CC_SUCCESS)
    {
        // Reg read sequence failed, abort the sequence
        peci_WrPkgConfig_seq(cpuInfo.clientAddr, MBX_INDEX_VCU, VCU_ABORT_SEQ,
                             VCU_PWR_MGT_SEQ, sizeof(uint32_t), peci_fd, &cc);
        return ret;
    }

    // Get the register data
    ret = peci_RdPkgConfig_seq(cpuInfo.clientAddr, MBX_INDEX_VCU, PM_READ_PARAM,
                               sizeof(uint32_t), (uint8_t*)u32RegData, peci_fd,
                               &cc);
    *ccData = cc;
    *retData = ret;
    if (ret != PECI_CC_SUCCESS)
    {
        // Reg read sequence failed, abort the sequence
        peci_WrPkgConfig_seq(cpuInfo.clientAddr, MBX_INDEX_VCU, VCU_ABORT_SEQ,
                             VCU_PWR_MGT_SEQ, sizeof(uint32_t), peci_fd, &cc);
        return ret;
    }

    // Close the register read sequence
    peci_WrPkgConfig_seq(cpuInfo.clientAddr, MBX_INDEX_VCU, VCU_CLOSE_SEQ,
                         VCU_PWR_MGT_SEQ, sizeof(uint32_t), peci_fd, &cc);

    return ret;
}

/******************************************************************************
 *
 *   logPowerManagementCPX1
 *
 *   This function gathers the Power Management log and adds it to the debug log
 *   The PECI flow is listed below to dump the core state registers
 *
 *    WrPkgConfig() -
 *         0x80 0x0003 0x0001002a
 *         Open register read sequence
 *
 *    WrPkgConfig() -
 *         0x80 0x1 [29:24] Core Number, [23:0] Register
 *         Select the core and register
 *
 *    RdPkgConfig() -
 *         0x80 0x1019
 *         Read register data
 *
 *    WrPkgConfig() -
 *         0x80 0x0004 0x0001002a
 *         Close register read sequence.
 *
 ******************************************************************************/
int logPowerManagementCPX1(crashdump::CPUInfo& cpuInfo, cJSON* pJsonChild)
{
    int ret = 0;
    int retval = 0;
    int peci_fd = -1;

    if (pJsonChild == NULL)
    {
        return JSON_FAILURE;
    }

    ret = peci_Lock(&peci_fd, PECI_WAIT_FOREVER);
    if (ret != PECI_CC_SUCCESS)
    {
        return ret;
    }

    // Get the power state for each enabled core in the CPU
    for (uint32_t u32CoreNum = 0; (cpuInfo.coreMask >> u32CoreNum) != 0;
         u32CoreNum++)
    {
        if (!(cpuInfo.coreMask & (1 << u32CoreNum)))
        {
            continue;
        }

        SCpuPowerState sCpuPowerState = {};

        // First get the C-State register for this core
        ret = powerManagementReadReg(
            cpuInfo, u32CoreNum, PM_CSTATE_PARAM, &sCpuPowerState.u32CState,
            peci_fd, &sCpuPowerState.ccCstate, &sCpuPowerState.retCstate);
        if (ret != PECI_CC_SUCCESS)
        {
            retval = ret;
        }
        // Then get the VID Ratio register for this core
        ret = powerManagementReadReg(
            cpuInfo, u32CoreNum, PM_VID_PARAM, &sCpuPowerState.u32VidRatio,
            peci_fd, &sCpuPowerState.ccVratio, &sCpuPowerState.retVratio);
        if (ret != PECI_CC_SUCCESS)
        {
            retval = ret;
        }
        // If we hit a failure, assume that register reads are not allowed and
        // bail
        if (retval != 0)
        {
            break;
        }
        // Log the Power Management for this core
        powerManagementJson(u32CoreNum, &sCpuPowerState, pJsonChild);
    }

    peci_Unlock(peci_fd);
    return retval;
}

/******************************************************************************
 *
 *   powerManagementJsonICX1
 *
 *   This function formats the Power Management log into a JSON object
 *
 ******************************************************************************/
static void powerManagementJsonICX1(const char* regName,
                                    SPmRegRawData* sRegData, cJSON* pJsonChild,
                                    uint8_t cc, int ret)
{
    char jsonItemString[PM_JSON_STRING_LEN] = {0};

    if (sRegData->bInvalid)
    {
        cd_snprintf_s(jsonItemString, PM_JSON_STRING_LEN, PM_UA_DF, cc, ret);
    }
    else if (PECI_CC_UA(cc))
    {
        cd_snprintf_s(jsonItemString, PM_JSON_STRING_LEN, PM_UA, cc);
    }
    else
    {
        cd_snprintf_s(jsonItemString, PM_JSON_STRING_LEN, PM_UINT32_FMT,
                      sRegData->uValue, cc);
    }

    cJSON_AddStringToObject(pJsonChild, regName, jsonItemString);
}

/******************************************************************************
 *
 *   powerManagementJsonPerCoreICX1
 *
 *   This function formats Power Management Per Core log into a JSON object
 *
 ******************************************************************************/
static void powerManagementJsonPerCoreICX1(const char* regName,
                                           uint32_t u32CoreNum,
                                           SPmRegRawData* sRegData,
                                           cJSON* pJsonChild, uint8_t cc,
                                           int ret)
{
    char jsonItemName[PM_JSON_STRING_LEN] = {0};
    cJSON* core = NULL;

    // Add the core number item to the Power Management JSON structure
    cd_snprintf_s(jsonItemName, PM_JSON_STRING_LEN, PM_JSON_CORE_NAME,
                  u32CoreNum);

    if ((core = cJSON_GetObjectItemCaseSensitive(pJsonChild, jsonItemName)) ==
        NULL)
    {
        cJSON_AddItemToObject(pJsonChild, jsonItemName,
                              core = cJSON_CreateObject());
    }

    powerManagementJsonICX1(regName, sRegData, core, cc, ret);
}

/******************************************************************************
 *
 *   logPowerManagementICX1
 *
 *   This function gathers the Power Management log and adds it to the debug log
 *   The PECI flow is listed below to dump the core state registers
 *
 ******************************************************************************/
int logPowerManagementICX1(crashdump::CPUInfo& cpuInfo, cJSON* pJsonChild)
{
    int ret = 0;

    if (ret != PECI_CC_SUCCESS)
    {
        return ret;
    }

    for (uint32_t i = 0; i < (sizeof(sPmEntries) / sizeof(SPmEntry)); i++)
    {
        uint8_t cc = 0;
        SPmRegRawData sRegData = {};

        if (sPmEntries[i].isPerCore)
        {
            // Go through each enabled core
            for (uint32_t u32CoreNum = 0; (cpuInfo.coreMask >> u32CoreNum) != 0;
                 u32CoreNum++)
            {
                if (!CHECK_BIT(cpuInfo.coreMask, u32CoreNum))
                {
                    continue;
                }

                uint16_t param =
                    (u32CoreNum << 8) | (uint8_t)sPmEntries[i].param;

                ret = peci_RdPkgConfig(cpuInfo.clientAddr, PM_PCS_88, param,
                                       sizeof(uint32_t),
                                       (uint8_t*)&sRegData.uValue, &cc);
                if (ret != PECI_CC_SUCCESS)
                {
                    sRegData.bInvalid = true;
                }
                powerManagementJsonPerCoreICX1(sPmEntries[i].regName,
                                               u32CoreNum, &sRegData,
                                               pJsonChild, cc, ret);
            }
        }
        else
        {
            ret = peci_RdPkgConfig(cpuInfo.clientAddr, PM_PCS_88,
                                   sPmEntries[i].param, sizeof(uint32_t),
                                   (uint8_t*)&sRegData.uValue, &cc);
            if (ret != PECI_CC_SUCCESS)
            {
                sRegData.bInvalid = true;
            }
            powerManagementJsonICX1(sPmEntries[i].regName, &sRegData,
                                    pJsonChild, cc, ret);
        }
    }

    return ret;
}

static const SPowerManagementVx sPowerManagementVx[] = {
    {crashdump::cpu::clx, logPowerManagementCPX1},
    {crashdump::cpu::cpx, logPowerManagementCPX1},
    {crashdump::cpu::skx, logPowerManagementCPX1},
    {crashdump::cpu::icx, logPowerManagementICX1},
    {crashdump::cpu::icx2, logPowerManagementICX1},
};

/******************************************************************************
 *
 *   logPowerManagement
 *
 *   This function gathers the PowerManagement log and adds it to the debug log
 *
 ******************************************************************************/
int logPowerManagement(crashdump::CPUInfo& cpuInfo, cJSON* pJsonChild)
{
    if (pJsonChild == NULL)
    {
        return 1;
    }

    if (!CHECK_BIT(cpuInfo.sectionMask, crashdump::PM_INFO))
    {
        updateRecordEnable(pJsonChild, false);
        return 0;
    }

    for (uint32_t i = 0;
         i < (sizeof(sPowerManagementVx) / sizeof(SPowerManagementVx)); i++)
    {
        if (cpuInfo.model == sPowerManagementVx[i].cpuModel)
        {
            return sPowerManagementVx[i].logPowerManagementVx(cpuInfo,
                                                              pJsonChild);
        }
    }

    fprintf(stderr, "Cannot find version for %s\n", __FUNCTION__);
    return 1;
}
