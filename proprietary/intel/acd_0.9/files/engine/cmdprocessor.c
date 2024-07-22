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

#include "cmdprocessor.h"

#include <search.h>

#include "BigCore.h"
#include "Crashlog.h"
#include "TorDump.h"
#include "inputparser.h"
#include "logger.h"
#include "modules.h"
#include "utils.h"
#include "validator.h"
#ifdef NVD_SECTION
#include "nvd.h"
#endif

void UpdateInternalVar(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    if (cmdInOut->internalVarName != NULL)
    {
        // Peci command for variable failed.
        if (cmdInOut->out.ret != PECI_CC_SUCCESS ||
            PECI_CC_UA(cmdInOut->out.cc))
        {
            char jsonItemString[JSON_VAL_LEN];
            cd_snprintf_s(jsonItemString, JSON_VAL_LEN, CMD_ERROR_VALUE);
            cJSON_AddItemToObject(cmdInOut->internalVarsTracker,
                                  cmdInOut->internalVarName,
                                  cJSON_CreateString(jsonItemString));
            CRASHDUMP_PRINT(ERR, stderr,
                            "InternalVar: %s, did not get a valid value.\n",
                            cmdInOut->internalVarName);
        }
        else
        {
            char jsonItemString[JSON_VAL_LEN];
            if (cmdInOut->out.size == sizeof(uint64_t))
            {
                cd_snprintf_s(jsonItemString, JSON_VAL_LEN, "0x%" PRIx64 "",
                              cmdInOut->out.val.u64);
            }
            else
            {
                cd_snprintf_s(jsonItemString, JSON_VAL_LEN, "0x%" PRIx32 "",
                              cmdInOut->out.val.u32[0]);
            }
            cJSON_AddItemToObject(cmdInOut->internalVarsTracker,
                                  cmdInOut->internalVarName,
                                  cJSON_CreateString(jsonItemString));
        }
    }
}

static acdStatus CrashDump_Discovery(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_crashdump_disc_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsCrashDump_DiscoveryParamsValid(cmdInOut->in.params,
                                          cmdInOut->validatorParams))
    {
        return ACD_INVALID_CRASHDUMP_DISCOVERY_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.subopcode = it->valueint;
                break;
            case 2:
                params.param0 = it->valueint;
                break;
            case 3:
                params.param1 = it->valueint;
                break;
            case 4:
                params.param2 = it->valueint;
                break;
            case 5:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_CRASHDUMP_DISCOVERY_PARAMS;
        }
        position++;
    }
    cmdInOut->out.ret = peci_CrashDump_Discovery(
        params.addr, params.subopcode, params.param0, params.param1,
        params.param2, params.rx_len, (uint8_t*)&cmdInOut->out.val.u64,
        &cmdInOut->out.cc);

    UpdateInternalVar(cmdInOut, cpuInfo);

    return ACD_SUCCESS;
}

static acdStatus CrashDump_Discovery_Dom(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_crashdump_disc_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsCrashDump_DiscoveryDomParamsValid(cmdInOut->in.params,
                                             cmdInOut->validatorParams))
    {
        return ACD_INVALID_CRASHDUMP_DISCOVERY_DOM_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.domain_id = it->valueint;
                break;
            case 2:
                params.subopcode = it->valueint;
                break;
            case 3:
                params.param0 = it->valueint;
                break;
            case 4:
                params.param1 = it->valueint;
                break;
            case 5:
                params.param2 = it->valueint;
                break;
            case 6:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_CRASHDUMP_DISCOVERY_DOM_PARAMS;
        }
        position++;
    }

    // Execute discovery command on a restricted valid domain and input file
    // variable names. Check is necessary due to input file superset setup.
    if (CHECK_BIT(cpuInfo->dieMaskInfo.dieMask, params.domain_id))
    {
        cmdInOut->out.ret = peci_CrashDump_Discovery_dom(
            params.addr, params.domain_id, params.subopcode, params.param0,
            params.param1, params.param2, params.rx_len,
            (uint8_t*)&cmdInOut->out.val.u64, &cmdInOut->out.cc);
        UpdateInternalVar(cmdInOut, cpuInfo);
    }

    return ACD_SUCCESS;
}

static acdStatus CrashDump_GetFrame(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_crashdump_disc_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsCrashDump_GetFrameParamsValid(cmdInOut->in.params,
                                         cmdInOut->validatorParams))
    {
        return ACD_INVALID_CRASHDUMP_GETFRAME_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.param0 = it->valueint;
                break;
            case 2:
                params.param1 = it->valueint;
                break;
            case 3:
                params.param2 = it->valueint;
                break;
            case 4:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_CRASHDUMP_GETFRAME_PARAMS;
        }
        position++;
    }

    cmdInOut->out.ret = peci_CrashDump_GetFrame(
        params.addr, params.param0, params.param1, params.param2, params.rx_len,
        (uint8_t*)&cmdInOut->out.val.u64, &cmdInOut->out.cc);
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus CrashDump_GetFrame_Dom(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_crashdump_disc_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsCrashDump_GetFrameDomParamsValid(cmdInOut->in.params,
                                            cmdInOut->validatorParams))
    {
        return ACD_INVALID_CRASHDUMP_GETFRAME_DOM_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.domain_id = it->valueint;
                break;
            case 2:
                params.param0 = it->valueint;
                break;
            case 3:
                params.param1 = it->valueint;
                break;
            case 4:
                params.param2 = it->valueint;
                break;
            case 5:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_CRASHDUMP_GETFRAME_DOM_PARAMS;
        }
        position++;
    }

    cmdInOut->out.ret = peci_CrashDump_GetFrame_dom(
        params.addr, params.domain_id, params.param0, params.param1,
        params.param2, params.rx_len, (uint8_t*)&cmdInOut->out.val.u64,
        &cmdInOut->out.cc);
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus Ping(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    if (!IsPingParamsValid(cmdInOut->in.params, cmdInOut->validatorParams))
    {
        return ACD_INVALID_PING_PARAMS;
    }

    cmdInOut->out.ret = peci_Ping(cmdInOut->in.params->valueint);
    return ACD_SUCCESS;
}

static acdStatus GetCPUID(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    if (!IsGetCPUIDParamsValid(cmdInOut->in.params, cmdInOut->validatorParams))
    {
        return ACD_INVALID_GETCPUID_PARAMS;
    }

    cmdInOut->out.ret = peci_GetCPUID(
        cmdInOut->in.params->valueint, &cmdInOut->out.cpuID.cpuModel,
        &cmdInOut->out.cpuID.stepping, &cmdInOut->out.cc);
    return ACD_SUCCESS;
}

static acdStatus RdIAMSR(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_rd_ia_msr_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsRdIAMSRParamsValid(cmdInOut->in.params, cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDIAMSR_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.thread_id = it->valueint;
                break;
            case 2:
                params.address = it->valueint;
                break;
            default:
                return ACD_INVALID_RDIAMSR_PARAMS;
        }
        position++;
    }
    cmdInOut->out.size = sizeof(uint64_t);
    cmdInOut->out.ret =
        peci_RdIAMSR(params.addr, params.thread_id, params.address,
                     (uint64_t*)&cmdInOut->out.val.u64, &cmdInOut->out.cc);
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus RdIAMSR_Dom(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_rd_ia_msr_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsRdIAMSRDomParamsValid(cmdInOut->in.params,
                                 cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDIAMSR_DOM_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.domain_id = it->valueint;
                break;
            case 2:
                params.thread_id = it->valueint;
                break;
            case 3:
                params.address = it->valueint;
                break;
            default:
                return ACD_INVALID_RDIAMSR_DOM_PARAMS;
        }
        position++;
    }
    cmdInOut->out.size = sizeof(uint64_t);
    cmdInOut->out.ret = peci_RdIAMSR_dom(
        params.addr, params.domain_id, params.thread_id, params.address,
        (uint64_t*)&cmdInOut->out.val.u64, &cmdInOut->out.cc);
    UpdateInternalVar(cmdInOut, cpuInfo);

    if (cmdInOut->out.ret != PECI_CC_SUCCESS)
    {
        return ACD_FAILURE;
    }
    else
    {
        return ACD_SUCCESS;
    }
}

static acdStatus RdPkgConfig(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_rd_pkg_cfg_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsRdPkgConfigParamsValid(cmdInOut->in.params,
                                  cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDPKGCONFIG_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.index = it->valueint;
                break;
            case 2:
                params.param = it->valueint;
                break;
            case 3:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_RDPKGCONFIG_PARAMS;
        }
        position++;
    }

    cmdInOut->out.ret =
        peci_RdPkgConfig(params.addr, params.index, params.param, params.rx_len,
                         (uint8_t*)&cmdInOut->out.val.u64, &cmdInOut->out.cc);
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus RdPkgConfig_Dom(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_rd_pkg_cfg_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsRdPkgConfigDomParamsValid(cmdInOut->in.params,
                                     cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDPKGCONFIG_DOM_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.domain_id = it->valueint;
                break;
            case 2:
                params.index = it->valueint;
                break;
            case 3:
                params.param = it->valueint;
                break;
            case 4:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_RDPKGCONFIG_DOM_PARAMS;
        }
        position++;
    }

    cmdInOut->out.ret = peci_RdPkgConfig_dom(
        params.addr, params.domain_id, params.index, params.param,
        params.rx_len, (uint8_t*)&cmdInOut->out.val.u64, &cmdInOut->out.cc);

    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus RdPkgConfigCore(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_rd_pkg_cfg_msg params = {0};
    int position = 0;
    uint8_t core = 0;
    cJSON* it = NULL;

    if (!IsRdPkgConfigCoreParamsValid(cmdInOut->in.params,
                                      cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDPKGCONFIGCORE_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.index = it->valueint;
                break;
            case 2:
                core = it->valueint;
                break;
            case 3:
                params.param = it->valueint;
                break;
            case 4:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_RDPKGCONFIGCORE_PARAMS;
        }
        position++;
    }
    params.param = (core << 8 | params.param);

    cmdInOut->out.ret =
        peci_RdPkgConfig(params.addr, params.index, params.param, params.rx_len,
                         (uint8_t*)&cmdInOut->out.val.u64, &cmdInOut->out.cc);

    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus RdPkgConfigCore_Dom(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_rd_pkg_cfg_msg params = {0};
    int position = 0;
    uint8_t core = 0;
    cJSON* it = NULL;

    if (!IsRdPkgConfigCoreDomParamsValid(cmdInOut->in.params,
                                         cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDPKGCONFIGCORE_DOM_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.domain_id = it->valueint;
                break;
            case 2:
                params.index = it->valueint;
                break;
            case 3:
                core = it->valueint;
                break;
            case 4:
                params.param = it->valueint;
                break;
            case 5:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_RDPKGCONFIGCORE_DOM_PARAMS;
        }
        position++;
    }
    params.param = (core << 8 | params.param);

    cmdInOut->out.ret = peci_RdPkgConfig_dom(
        params.addr, params.domain_id, params.index, params.param,
        params.rx_len, (uint8_t*)&cmdInOut->out.val.u64, &cmdInOut->out.cc);

    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus RdPCIConfigLocal(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_rd_pci_cfg_local_msg params = {0};
    int position = 0;
    cJSON* it = NULL;
    const int wordFrameSize = (sizeof(uint64_t) / sizeof(uint16_t));

    if (!IsRdPciConfigLocalParamsValid(cmdInOut->in.params,
                                       cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDPCICONFIGLOCAL_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.bus = it->valueint;
                break;
            case 2:
                params.device = it->valueint;
                break;
            case 3:
                params.function = it->valueint;
                break;
            case 4:
                params.reg = it->valueint;
                break;
            case 5:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_RDPCICONFIGLOCAL_PARAMS;
        }
        position++;
    }
    switch (params.rx_len)
    {
        case sizeof(uint8_t):
        case sizeof(uint16_t):
        case sizeof(uint32_t):
            cmdInOut->out.ret = peci_RdPCIConfigLocal(
                params.addr, params.bus, params.device, params.function,
                params.reg, params.rx_len, (uint8_t*)&cmdInOut->out.val.u64,
                &cmdInOut->out.cc);
            break;
        case sizeof(uint64_t):
            for (uint8_t u8Dword = 0; u8Dword < 2; u8Dword++)
            {
                cmdInOut->out.ret = peci_RdPCIConfigLocal(
                    params.addr, params.bus, params.device, params.function,
                    params.reg + (u8Dword * wordFrameSize), sizeof(uint32_t),
                    (uint8_t*)&cmdInOut->out.val.u32[u8Dword],
                    &cmdInOut->out.cc);
            }
            break;
    }
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus RdPCIConfigLocal_Dom(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_rd_pci_cfg_local_msg params = {0};
    int position = 0;
    cJSON* it = NULL;
    const int wordFrameSize = (sizeof(uint64_t) / sizeof(uint16_t));

    if (!IsRdPciConfigLocalDomParamsValid(cmdInOut->in.params,
                                          cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDPCICONFIGLOCAL_DOM_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.domain_id = it->valueint;
                break;
            case 2:
                params.bus = it->valueint;
                break;
            case 3:
                params.device = it->valueint;
                break;
            case 4:
                params.function = it->valueint;
                break;
            case 5:
                params.reg = it->valueint;
                break;
            case 6:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_RDPCICONFIGLOCAL_DOM_PARAMS;
        }
        position++;
    }
    switch (params.rx_len)
    {
        case sizeof(uint8_t):
        case sizeof(uint16_t):
        case sizeof(uint32_t):
            cmdInOut->out.ret = peci_RdPCIConfigLocal_dom(
                params.addr, params.domain_id, params.bus, params.device,
                params.function, params.reg, params.rx_len,
                (uint8_t*)&cmdInOut->out.val.u64, &cmdInOut->out.cc);
            break;
        case sizeof(uint64_t):
            for (uint8_t u8Dword = 0; u8Dword < 2; u8Dword++)
            {
                cmdInOut->out.ret = peci_RdPCIConfigLocal_dom(
                    params.addr, params.domain_id, params.bus, params.device,
                    params.function, params.reg + (u8Dword * wordFrameSize),
                    sizeof(uint32_t), (uint8_t*)&cmdInOut->out.val.u32[u8Dword],
                    &cmdInOut->out.cc);
            }
            break;
    }
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus RdEndPointConfigPciLocal(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_rd_end_pt_cfg_msg params = {0};
    int position = 0;
    cJSON* it = NULL;
    const int wordFrameSize = (sizeof(uint64_t) / sizeof(uint16_t));

    if (!IsRdEndPointConfigPciLocalParamsValid(cmdInOut->in.params,
                                               cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDENDPOINTCONFIGPCILOCAL_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.params.pci_cfg.seg = it->valueint;
                break;
            case 2:
                params.params.pci_cfg.bus = it->valueint;
                break;
            case 3:
                params.params.pci_cfg.device = it->valueint;
                break;
            case 4:
                params.params.pci_cfg.function = it->valueint;
                break;
            case 5:
                params.params.pci_cfg.reg = it->valueint;
                break;
            case 6:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_RDENDPOINTCONFIGPCILOCAL_PARAMS;
        }
        position++;
    }
    switch (params.rx_len)
    {
        case sizeof(uint8_t):
        case sizeof(uint16_t):
        case sizeof(uint32_t):
            cmdInOut->out.ret = peci_RdEndPointConfigPciLocal(
                params.addr, params.params.pci_cfg.seg,
                params.params.pci_cfg.bus, params.params.pci_cfg.device,
                params.params.pci_cfg.function, params.params.pci_cfg.reg,
                params.rx_len, (uint8_t*)&cmdInOut->out.val.u64,
                &cmdInOut->out.cc);
            break;
        case sizeof(uint64_t):
            for (uint8_t u8Dword = 0; u8Dword < 2; u8Dword++)
            {
                cmdInOut->out.ret = peci_RdEndPointConfigPciLocal(
                    params.addr, params.params.pci_cfg.seg,
                    params.params.pci_cfg.bus, params.params.pci_cfg.device,
                    params.params.pci_cfg.function,
                    params.params.pci_cfg.reg + (u8Dword * wordFrameSize),
                    sizeof(uint32_t), (uint8_t*)&cmdInOut->out.val.u32[u8Dword],
                    &cmdInOut->out.cc);
            }
            break;
    }
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus RdEndPointConfigPciLocal_Dom(CmdInOut* cmdInOut,
                                              CPUInfo* cpuInfo)
{
    struct peci_rd_end_pt_cfg_msg params = {0};
    int position = 0;
    cJSON* it = NULL;
    const int wordFrameSize = (sizeof(uint64_t) / sizeof(uint16_t));

    if (!IsRdEndPointConfigPciLocalDomParamsValid(cmdInOut->in.params,
                                                  cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDENDPOINTCONFIGPCILOCAL_DOM_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.domain_id = it->valueint;
                break;
            case 2:
                params.params.pci_cfg.seg = it->valueint;
                break;
            case 3:
                params.params.pci_cfg.bus = it->valueint;
                break;
            case 4:
                params.params.pci_cfg.device = it->valueint;
                break;
            case 5:
                params.params.pci_cfg.function = it->valueint;
                break;
            case 6:
                params.params.pci_cfg.reg = it->valueint;
                break;
            case 7:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_RDENDPOINTCONFIGPCILOCAL_DOM_PARAMS;
        }
        position++;
    }
    switch (params.rx_len)
    {
        case sizeof(uint8_t):
        case sizeof(uint16_t):
        case sizeof(uint32_t):
            cmdInOut->out.ret = peci_RdEndPointConfigPciLocal_dom(
                params.addr, params.domain_id, params.params.pci_cfg.seg,
                params.params.pci_cfg.bus, params.params.pci_cfg.device,
                params.params.pci_cfg.function, params.params.pci_cfg.reg,
                params.rx_len, (uint8_t*)&cmdInOut->out.val.u64,
                &cmdInOut->out.cc);
            break;
        case sizeof(uint64_t):
            for (uint8_t u8Dword = 0; u8Dword < 2; u8Dword++)
            {
                cmdInOut->out.ret = peci_RdEndPointConfigPciLocal_dom(
                    params.addr, params.domain_id, params.params.pci_cfg.seg,
                    params.params.pci_cfg.bus, params.params.pci_cfg.device,
                    params.params.pci_cfg.function,
                    params.params.pci_cfg.reg + (u8Dword * wordFrameSize),
                    sizeof(uint32_t), (uint8_t*)&cmdInOut->out.val.u32[u8Dword],
                    &cmdInOut->out.cc);
            }
            break;
    }
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus WrEndPointConfigPciLocal(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_wr_end_pt_cfg_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsWrEndPointConfigPciLocalParamsValid(cmdInOut->in.params,
                                               cmdInOut->validatorParams))
    {
        return ACD_INVALID_WRENDPOINTCONFIGPCILOCAL_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.params.pci_cfg.seg = it->valueint;
                break;
            case 2:
                params.params.pci_cfg.bus = it->valueint;
                break;
            case 3:
                params.params.pci_cfg.device = it->valueint;
                break;
            case 4:
                params.params.pci_cfg.function = it->valueint;
                break;
            case 5:
                params.params.pci_cfg.reg = it->valueint;
                break;
            case 6:
                params.tx_len = it->valueint;
                cmdInOut->out.size = params.tx_len;
                break;
            case 7:
                params.value = it->valueint;
                break;
            default:
                return ACD_INVALID_WRENDPOINTCONFIGPCILOCAL_PARAMS;
        }
        position++;
    }
    cmdInOut->out.ret = peci_WrEndPointPCIConfigLocal(
        params.addr, params.params.pci_cfg.seg, params.params.pci_cfg.bus,
        params.params.pci_cfg.device, params.params.pci_cfg.function,
        params.params.pci_cfg.reg, params.tx_len, params.value,
        &cmdInOut->out.cc);
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus WrEndPointConfigPciLocal_Dom(CmdInOut* cmdInOut,
                                              CPUInfo* cpuInfo)
{
    struct peci_wr_end_pt_cfg_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsWrEndPointConfigPciLocalDomParamsValid(cmdInOut->in.params,
                                                  cmdInOut->validatorParams))
    {
        return ACD_INVALID_WRENDPOINTCONFIGPCILOCAL_DOM_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.domain_id = it->valueint;
                break;
            case 2:
                params.params.pci_cfg.seg = it->valueint;
                break;
            case 3:
                params.params.pci_cfg.bus = it->valueint;
                break;
            case 4:
                params.params.pci_cfg.device = it->valueint;
                break;
            case 5:
                params.params.pci_cfg.function = it->valueint;
                break;
            case 6:
                params.params.pci_cfg.reg = it->valueint;
                break;
            case 7:
                params.tx_len = it->valueint;
                cmdInOut->out.size = params.tx_len;
                break;
            case 8:
                params.value = it->valueint;
                break;
            default:
                return ACD_INVALID_WRENDPOINTCONFIGPCILOCAL_DOM_PARAMS;
        }
        position++;
    }
    cmdInOut->out.ret = peci_WrEndPointPCIConfigLocal_dom(
        params.addr, params.domain_id, params.params.pci_cfg.seg,
        params.params.pci_cfg.bus, params.params.pci_cfg.device,
        params.params.pci_cfg.function, params.params.pci_cfg.reg,
        params.tx_len, params.value, &cmdInOut->out.cc);
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus RdEndPointConfigMmio(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_rd_end_pt_cfg_msg params = {0};
    int position = 0;
    cJSON* it = NULL;
    if (!IsRdEndPointConfigMmioParamsValid(cmdInOut->in.params,
                                           cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDENDPOINTCONFIGMMIO_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.params.mmio.seg = it->valueint;
                break;
            case 2:
                params.params.mmio.bus = it->valueint;
                break;
            case 3:
                params.params.mmio.device = it->valueint;
                break;
            case 4:
                params.params.mmio.function = it->valueint;
                break;
            case 5:
                params.params.mmio.bar = it->valueint;
                break;
            case 6:
                params.params.mmio.addr_type = it->valueint;
                break;
            case 7:
                params.params.mmio.offset = (uint64_t)it->valueint;
                break;
            case 8:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_RDENDPOINTCONFIGMMIO_PARAMS;
        }
        position++;
    }
    cmdInOut->out.ret = peci_RdEndPointConfigMmio(
        params.addr, params.params.mmio.seg, params.params.mmio.bus,
        params.params.mmio.device, params.params.mmio.function,
        params.params.mmio.bar, params.params.mmio.addr_type,
        params.params.mmio.offset, params.rx_len,
        (uint8_t*)&cmdInOut->out.val.u64, &cmdInOut->out.cc);
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus RdEndPointConfigMmio_Dom(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_rd_end_pt_cfg_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsRdEndPointConfigMmioDomParamsValid(cmdInOut->in.params,
                                              cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDENDPOINTCONFIGMMIO_DOM_PARAMS;
    }
    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.domain_id = it->valueint;
                break;
            case 2:
                params.params.mmio.seg = it->valueint;
                break;
            case 3:
                /*
                 * This check verifies the valueString to handle cases when
                 * the enumerated bus number is invalid or disabled. Due to
                 * numerous instances in the process flow where variable
                 * values are assigned to valueString, it's essential to
                 * treat a null valueString as valid. Otherwise, it indicates
                 * an invalid or disabled case, In such case, valueint will
                 * be always a 0 and valueString will have the invalid
                 * statement. For any invalid or disabled scenario, we will
                 * return an PECI_CC_INVALID_REQ to prevent calling
                 * RdEndPointConfigMmio_Dom.
                 */
                if (it->valuestring == NULL)
                {
                    params.params.mmio.bus = it->valueint;
                }
                else
                {
                    cmdInOut->out.ret = PECI_CC_INVALID_REQ;
                    cmdInOut->out.stringVal = "N/A";
                    cmdInOut->out.printString = true;
                    return ACD_INVALID_RDENDPOINTCONFIGMMIO_DOM_PARAMS;
                }
                break;
            case 4:
                params.params.mmio.device = it->valueint;
                break;
            case 5:
                params.params.mmio.function = it->valueint;
                break;
            case 6:
                params.params.mmio.bar = it->valueint;
                break;
            case 7:
                params.params.mmio.addr_type = it->valueint;
                break;
            case 8:
                params.params.mmio.offset = (uint64_t)it->valueint;
                break;
            case 9:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_RDENDPOINTCONFIGMMIO_DOM_PARAMS;
        }
        position++;
    }
    cmdInOut->out.ret = peci_RdEndPointConfigMmio_dom(
        params.addr, params.domain_id, params.params.mmio.seg,
        params.params.mmio.bus, params.params.mmio.device,
        params.params.mmio.function, params.params.mmio.bar,
        params.params.mmio.addr_type, params.params.mmio.offset, params.rx_len,
        (uint8_t*)&cmdInOut->out.val.u64, &cmdInOut->out.cc);
    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus RdPostEnumBus(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    PostEnumBus params = {0};
    cJSON* it = NULL;
    int position = 0;

    // Set ret & cc to success for logger
    cmdInOut->out.ret = PECI_CC_SUCCESS;
    cmdInOut->out.cc = PECI_DEV_CC_SUCCESS;

    if (!IsRdPostEnumBusParamsValid(cmdInOut->in.params,
                                    cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDPOSTENUMBUS_PARAMS;
    }
    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                /*
                 * This check verifies the valueString to handle cases when
                 * the enumerated bus number is invalid or disabled. Due to
                 * numerous instances in the process flow where variable
                 * values are assigned to valueString, it's essential to
                 * treat a null valueString as valid. Otherwise, it indicates
                 * an invalid or disabled case, In such case, valueint will
                 * be always a 0 and valueString will have the invalid
                 * statement.
                 */
                if (it->valuestring == NULL)
                {
                    params.cpubusno_valid = (uint32_t)it->valuedouble;
                }
                else
                {
                    cmdInOut->out.ret = PECI_CC_INVALID_REQ;
                    cmdInOut->out.stringVal = "Invalid/Disabled";
                    cmdInOut->out.printString = true;
                    UpdateInternalVar(cmdInOut, cpuInfo);
                    return ACD_INVALID_RDPOSTENUMBUS_PARAMS;
                }
                break;
            case 1:
                params.cpubusno = (uint32_t)it->valuedouble;
                break;
            case 2:
                params.bitToCheck = it->valueint;
                break;
            case 3:
                params.shift = (uint8_t)it->valueint;
                break;
            default:
                return ACD_INVALID_RDPOSTENUMBUS_PARAMS;
        }
        position++;
    }
    cmdInOut->out.size = sizeof(uint8_t);
    if (0 == CHECK_BIT(params.cpubusno_valid, params.bitToCheck))
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "RdPostEnumBus: Bit %d not enable in post"
                        " enumerated bus 0x%x.\n",
                        params.bitToCheck, params.cpubusno_valid);
        cmdInOut->out.ret = PECI_CC_INVALID_REQ;
        cmdInOut->out.stringVal = "Invalid/Disabled";
        cmdInOut->out.printString = true;
    }
    else
    {
        cmdInOut->out.val.u32[0] = ((params.cpubusno >> params.shift) & 0xff);
    }
    UpdateInternalVar(cmdInOut, cpuInfo);

    return ACD_SUCCESS;
}

static acdStatus RdChaCount(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    ChaCount params = {0};
    cJSON* it = NULL;
    int position = 0;

    if (!IsRdChaCountParamsValid(cmdInOut->in.params,
                                 cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDCHACOUNT_PARAMS;
    }
    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.chaMask0 = (uint32_t)it->valuedouble;
                break;
            case 1:
                params.chaMask1 = (uint32_t)it->valuedouble;
                break;
            default:
                return ACD_INVALID_RDCHACOUNT_PARAMS;
        }
        position++;
    }

    cmdInOut->out.size = sizeof(uint8_t);
    cmdInOut->out.val.u32[0] = __builtin_popcount((int)params.chaMask0) +
                               __builtin_popcount((int)params.chaMask1);
    UpdateInternalVar(cmdInOut, cpuInfo);

    // Set ret & cc to success for logger
    cmdInOut->out.ret = PECI_CC_SUCCESS;
    cmdInOut->out.cc = PECI_DEV_CC_SUCCESS;
    return ACD_SUCCESS;
}

static acdStatus Telemetry_Discovery(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_telemetry_disc_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsTelemetry_DiscoveryParamsValid(cmdInOut->in.params,
                                          cmdInOut->validatorParams))
    {
        return ACD_INVALID_TELEMETRY_DISCOVERY_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.subopcode = it->valueint;
                break;
            case 2:
                params.param0 = it->valueint;
                break;
            case 3:
                params.param1 = it->valueint;
                break;
            case 4:
                params.param2 = it->valueint;
                break;
            case 5:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_TELEMETRY_DISCOVERY_PARAMS;
        }
        position++;
    }
    cmdInOut->out.ret = peci_Telemetry_Discovery(
        params.addr, params.subopcode, params.param0, params.param1,
        params.param2, params.rx_len, (uint8_t*)&cmdInOut->out.val,
        &cmdInOut->out.cc);

    UpdateInternalVar(cmdInOut, cpuInfo);

    return ACD_SUCCESS;
}

static acdStatus Telemetry_Discovery_Dom(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    struct peci_telemetry_disc_msg params = {0};
    int position = 0;
    cJSON* it = NULL;

    if (!IsTelemetry_DiscoveryDomParamsValid(cmdInOut->in.params,
                                             cmdInOut->validatorParams))
    {
        return ACD_INVALID_TELEMETRY_DISCOVERY_DOM_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                params.addr = it->valueint;
                break;
            case 1:
                params.domain_id = it->valueint;
                break;
            case 2:
                params.subopcode = it->valueint;
                break;
            case 3:
                params.param0 = it->valueint;
                break;
            case 4:
                params.param1 = it->valueint;
                break;
            case 5:
                params.param2 = it->valueint;
                break;
            case 6:
                params.rx_len = it->valueint;
                cmdInOut->out.size = params.rx_len;
                break;
            default:
                return ACD_INVALID_TELEMETRY_DISCOVERY_DOM_PARAMS;
        }
        position++;
    }
    cmdInOut->out.ret = peci_Telemetry_Discovery_dom(
        params.addr, params.domain_id, params.subopcode, params.param0,
        params.param1, params.param2, params.rx_len,
        (uint8_t*)&cmdInOut->out.val, &cmdInOut->out.cc);

    UpdateInternalVar(cmdInOut, cpuInfo);

    return ACD_SUCCESS;
}

uint8_t getIsTelemetrySupportedResult(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    cJSON* isTelemetryEnabledNode = cJSON_GetObjectItemCaseSensitive(
        cmdInOut->internalVarsTracker, "IsEnable");

    if (isTelemetryEnabledNode != NULL)
    {

        int mismatchTelemetryErrorVal = 1;
        strcmp_s(CMD_ERROR_VALUE,
                 strnlen_s(CMD_ERROR_VALUE, sizeof(CMD_ERROR_VALUE)),
                 isTelemetryEnabledNode->valuestring,
                 &mismatchTelemetryErrorVal);
        if (mismatchTelemetryErrorVal == 0)
        {
            return TELEMETRY_UNSUPPORTED;
        }

        return (uint8_t)strtoul(isTelemetryEnabledNode->valuestring, NULL, 16);
    }

    return TELEMETRY_UNSUPPORTED;
}

uint8_t getNumberofCrashlogAgents(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    cJSON* numberofCrashlogAgentsNode = cJSON_GetObjectItemCaseSensitive(
        cmdInOut->internalVarsTracker, "NumAgents");

    if (numberofCrashlogAgentsNode != NULL)
    {
        int mismatchPeciErrorVal = 1;
        strcmp_s(CMD_ERROR_VALUE,
                 strnlen_s(CMD_ERROR_VALUE, sizeof(CMD_ERROR_VALUE)),
                 numberofCrashlogAgentsNode->valuestring,
                 &mismatchPeciErrorVal);
        if (mismatchPeciErrorVal == 0)
        {
            return NO_CRASHLOG_AGENTS;
        }

        return (uint8_t)strtoul(numberofCrashlogAgentsNode->valuestring, NULL,
                                16);
    }

    return NO_CRASHLOG_AGENTS;
}

static acdStatus LogCrashlog(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    cmdInOut->out.size = sizeof(uint8_t);
    if (GenerateJsonPath(cmdInOut, cmdInOut->root, cmdInOut->logger, false) ==
        ACD_SUCCESS)
    {
        if (getIsTelemetrySupportedResult(cmdInOut, cpuInfo))
        {
            return logCrashlogSection(
                cpuInfo, cmdInOut->logger->nameProcessing.jsonOutput,
                getNumberofCrashlogAgents(cmdInOut, cpuInfo),
                cmdInOut->logger->contextLogger.domainID,
                *cmdInOut->runTimeInfo);
        }
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "Could not log Crashlog, path is Null.\n");
    }

    return ACD_FAILURE;
}

static bool getIsCrashdumpEnableResult(CmdInOut* cmdInOut, CPUInfo* cpuInfo,
                                       char* varStr)
{
    cJSON* isCrashdumpEnableNode =
        cJSON_GetObjectItemCaseSensitive(cmdInOut->internalVarsTracker, varStr);

    if (isCrashdumpEnableNode != NULL)
    {
        int mismatchCrashdumpEnableErrorVal = 1;
        strcmp_s(CMD_ERROR_VALUE,
                 strnlen_s(CMD_ERROR_VALUE, sizeof(CMD_ERROR_VALUE)),
                 isCrashdumpEnableNode->valuestring,
                 &mismatchCrashdumpEnableErrorVal);
        if (mismatchCrashdumpEnableErrorVal == 0)
        {
            return false;
        }

        uint8_t isEnabled =
            (uint8_t)strtoul(isCrashdumpEnableNode->valuestring, NULL, 16);

        if (isEnabled == CRASHDUMP_DISABLED)
        {
            CRASHDUMP_PRINT(ERR, stderr,
                            "Crashdump is disabled (%d) during discovery "
                            "(disabled:%d)\n",
                            cmdInOut->out.ret, isEnabled);
            return false;
        }
        return true;
    }
    return false;
}

static bool getNumCrashdumpAgentsResult(CmdInOut* cmdInOut, CPUInfo* cpuInfo,
                                        char* varStr)
{
    cJSON* numCrashdumpAgentsNode =
        cJSON_GetObjectItemCaseSensitive(cmdInOut->internalVarsTracker, varStr);

    if (numCrashdumpAgentsNode != NULL)
    {
        int mismatchNumCrashdumpAgentsErrorVal = 1;
        strcmp_s(CMD_ERROR_VALUE,
                 strnlen_s(CMD_ERROR_VALUE, sizeof(CMD_ERROR_VALUE)),
                 numCrashdumpAgentsNode->valuestring,
                 &mismatchNumCrashdumpAgentsErrorVal);
        if (mismatchNumCrashdumpAgentsErrorVal == 0)
        {
            return false;
        }

        uint16_t numCrashdumpAgents =
            (uint16_t)strtoul(numCrashdumpAgentsNode->valuestring, NULL, 16);
        if (numCrashdumpAgents <= PECI_CRASHDUMP_CORE)
        {
            CRASHDUMP_PRINT(
                ERR, stderr,
                "Error occurred (%d) during discovery (num of agents:%d)\n",
                cmdInOut->out.ret, numCrashdumpAgents);
            return false;
        }
        return true;
    }
    return false;
}

static bool getCrashdumpGUIDResult(CmdInOut* cmdInOut, CPUInfo* cpuInfo,
                                   char* varStr)
{
    cJSON* crashdumpGUIDNode =
        cJSON_GetObjectItemCaseSensitive(cmdInOut->internalVarsTracker, varStr);

    if (crashdumpGUIDNode != NULL)
    {
        int mismatchCrashdumpGUIDErrorVal = 1;
        strcmp_s(CMD_ERROR_VALUE,
                 strnlen_s(CMD_ERROR_VALUE, sizeof(CMD_ERROR_VALUE)),
                 crashdumpGUIDNode->valuestring,
                 &mismatchCrashdumpGUIDErrorVal);
        if (mismatchCrashdumpGUIDErrorVal == 0)
        {
            return false;
        }

        return true;
    }
    return false;
}

static bool getCrashdumpPayloadSizeResult(CmdInOut* cmdInOut, CPUInfo* cpuInfo,
                                          char* varStr)
{
    cJSON* crashdumpPayloadSizeNode =
        cJSON_GetObjectItemCaseSensitive(cmdInOut->internalVarsTracker, varStr);

    if (crashdumpPayloadSizeNode != NULL)
    {
        int mismatchCrashdumpPayloadSizeErrorVal = 1;
        strcmp_s(CMD_ERROR_VALUE,
                 strnlen_s(CMD_ERROR_VALUE, sizeof(CMD_ERROR_VALUE)),
                 crashdumpPayloadSizeNode->valuestring,
                 &mismatchCrashdumpPayloadSizeErrorVal);
        if (mismatchCrashdumpPayloadSizeErrorVal == 0)
        {
            return false;
        }
        return true;
    }
    return false;
}

static acdStatus LogBigCore(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    cmdInOut->out.size = sizeof(uint8_t);
    if (GenerateJsonPath(cmdInOut, cmdInOut->root, cmdInOut->logger, false) ==
        ACD_SUCCESS)
    {
        bool discoveryCheck;
        cpuInfo->crashdumpDiscoveryMask = 0x0;
        bool discoveryOneCheck =
            getIsCrashdumpEnableResult(cmdInOut, cpuInfo,
                                       "IsCrashdumpEnable") &&
            getNumCrashdumpAgentsResult(cmdInOut, cpuInfo,
                                        "NumCrashdumpAgents") &&
            getCrashdumpGUIDResult(cmdInOut, cpuInfo, "CrashdumpGUID") &&
            getCrashdumpPayloadSizeResult(cmdInOut, cpuInfo,
                                          "CrashdumpPayloadSize");
        if (discoveryOneCheck)
        {
            SET_BIT(cpuInfo->crashdumpDiscoveryMask, 0);
        }
        else
        {
            CRASHDUMP_PRINT(ERR, stderr, "1st CrashDump Discovery failed.\n");
        }

        uint8_t computeDieCount =
            __builtin_popcount(cpuInfo->dieMaskInfo.compute.effectiveMask);

        bool discoveryTwoCheck = false;
        bool discoveryThreeCheck = false;
        if (computeDieCount > 1)
        {
            if (cpuInfo->coreType == ACD_BIG_CORE &&
                cpuInfo->dieMaskInfo.dieMaskSupported)
            {
                discoveryTwoCheck =
                    getIsCrashdumpEnableResult(cmdInOut, cpuInfo,
                                               "IsCrashdumpEnable2") &&
                    getNumCrashdumpAgentsResult(cmdInOut, cpuInfo,
                                                "NumCrashdumpAgents2") &&
                    getCrashdumpGUIDResult(cmdInOut, cpuInfo,
                                           "CrashdumpGUID2") &&
                    getCrashdumpPayloadSizeResult(cmdInOut, cpuInfo,
                                                  "CrashdumpPayloadSize2");
                if (discoveryTwoCheck)
                {
                    SET_BIT(cpuInfo->crashdumpDiscoveryMask, 1);
                }
                else
                {
                    CRASHDUMP_PRINT(ERR, stderr,
                                    "2nd CrashDump Discovery failed.\n");
                }
                if (computeDieCount == 3)
                {
                    discoveryThreeCheck =
                        getIsCrashdumpEnableResult(cmdInOut, cpuInfo,
                                                   "IsCrashdumpEnable3") &&
                        getNumCrashdumpAgentsResult(cmdInOut, cpuInfo,
                                                    "NumCrashdumpAgents3") &&
                        getCrashdumpGUIDResult(cmdInOut, cpuInfo,
                                               "CrashdumpGUID3") &&
                        getCrashdumpPayloadSizeResult(cmdInOut, cpuInfo,
                                                      "CrashdumpPayloadSize3");

                    if (discoveryThreeCheck)
                    {
                        SET_BIT(cpuInfo->crashdumpDiscoveryMask, 2);
                    }
                    else
                    {
                        CRASHDUMP_PRINT(ERR, stderr,
                                        "3rd CrashDump Discovery failed.\n");
                    }
                }
            }
        }

        discoveryCheck =
            (discoveryOneCheck || discoveryTwoCheck || discoveryThreeCheck);

        if (discoveryCheck)
        {
            return logBigCoreSection(
                cpuInfo, cmdInOut->logger->nameProcessing.jsonOutput,
                *cmdInOut->runTimeInfo,
                cmdInOut->logger->contextLogger.domainID,
                cmdInOut->logger->contextLogger.compute,
                cmdInOut->logger->contextLogger.version);
        }
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "Could not log BigCore, path is Null.\n");
    }

    return ACD_FAILURE;
}

static acdStatus LogAtomCore(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    cmdInOut->out.size = sizeof(uint8_t);
    if (GenerateJsonPath(cmdInOut, cmdInOut->root, cmdInOut->logger, false) ==
        ACD_SUCCESS)
    {
        cpuInfo->crashdumpDiscoveryMask = 0x0;
        bool discoveryOneCheck =
            getIsCrashdumpEnableResult(cmdInOut, cpuInfo,
                                       "IsCrashdumpEnable") &&
            getNumCrashdumpAgentsResult(cmdInOut, cpuInfo,
                                        "NumCrashdumpAgents") &&
            getCrashdumpGUIDResult(cmdInOut, cpuInfo, "CrashdumpGUID") &&
            getCrashdumpPayloadSizeResult(cmdInOut, cpuInfo,
                                          "CrashdumpPayloadSize");
        if (discoveryOneCheck)
        {
            SET_BIT(cpuInfo->crashdumpDiscoveryMask, 0);
        }
        else
        {
            CRASHDUMP_PRINT(ERR, stderr, "1st CrashDump Discovery failed.\n");
        }

        bool discoveryTwoCheck = false;
        uint8_t computeDieCount =
            __builtin_popcount(cpuInfo->dieMaskInfo.compute.effectiveMask);

        if (computeDieCount > 1)
        {
            discoveryTwoCheck =
                getIsCrashdumpEnableResult(cmdInOut, cpuInfo,
                                           "IsCrashdumpEnable2") &&
                getNumCrashdumpAgentsResult(cmdInOut, cpuInfo,
                                            "NumCrashdumpAgents2") &&
                getCrashdumpGUIDResult(cmdInOut, cpuInfo, "CrashdumpGUID2") &&
                getCrashdumpPayloadSizeResult(cmdInOut, cpuInfo,
                                              "CrashdumpPayloadSize2");
            if (discoveryTwoCheck)
            {
                SET_BIT(cpuInfo->crashdumpDiscoveryMask, 1);
            }
            else
            {
                CRASHDUMP_PRINT(ERR, stderr,
                                "2nd CrashDump Discovery failed.\n");
            }
        }
        if (discoveryOneCheck || discoveryTwoCheck)
        {
            return logBigCoreSection(
                cpuInfo, cmdInOut->logger->nameProcessing.jsonOutput,
                *cmdInOut->runTimeInfo,
                cmdInOut->logger->contextLogger.domainID,
                cmdInOut->logger->contextLogger.compute,
                cmdInOut->logger->contextLogger.version);
        }
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "Could not log AtomCore, path is Null.\n");
    }

    return ACD_FAILURE;
}

static acdStatus RdAndConcatenate(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    cJSON* it = NULL;
    int position = 0;
    uint32_t high32BitValue = 0;
    uint32_t low32BitValue = 0;

    if (!IsRdAndConcatenateParamsValid(cmdInOut->in.params,
                                       cmdInOut->validatorParams))
    {
        return ACD_INVALID_RD_CONCATENATE_PARAMS;
    }
    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                low32BitValue = (uint32_t)it->valuedouble;
                break;
            case 1:
                high32BitValue = (uint32_t)it->valuedouble;
                break;
            default:
                return ACD_INVALID_RD_CONCATENATE_PARAMS;
        }
        position++;
    }

    cmdInOut->out.size = sizeof(uint64_t);
    cmdInOut->out.val.u32[1] = high32BitValue;
    cmdInOut->out.val.u32[0] = low32BitValue;

    UpdateInternalVar(cmdInOut, cpuInfo);

    // Set ret & cc to success for logger
    cmdInOut->out.ret = PECI_CC_SUCCESS;
    cmdInOut->out.cc = PECI_DEV_CC_SUCCESS;
    return ACD_SUCCESS;
}

static acdStatus RdGlobalVars(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    if (!IsRdGlobalVarsValid(cmdInOut->in.params, cmdInOut->validatorParams))
    {
        return ACD_INVALID_GLOBAL_VARS_PARAMS;
    }
    cmdInOut->out.ret = PECI_CC_SUCCESS;
    cmdInOut->out.cc = PECI_DEV_CC_SUCCESS;
    int mismatchclientaddr = 1;
    int mismatchcpuid = 1;
    int mismatchcpuidsource = 1;
    int mismatchcoremasksource = 1;
    int mismatchchacountsource = 1;
    int mismatchcoremask = 1;
    int mismatchchamask = 1;
    int mismatchchacount = 1;
    int mismatchchacounttotal = 1;
    int mismatchcorecount = 1;
    int mismatchcrashcorecount = 1;
    int mismatchcrashcoremask = 1;
    int mismatchbmcfwver = 1;
    int mismatchbiosid = 1;
    int mismatchtimestamp = 1;
    int mismatchtriggertype = 1;
    int mismatchplatformname = 1;
    int mismatchcrashdumpver = 1;
    int mismatchinputfilever = 1;
    int mismatchinputfile = 1;
    int mismatchdiemask = 1;
    int mismatchplatformid = 1;
    int mismatchucodepatchver = 1;
    int mismatchcjsonver = 1;

    strcmp_s(PECI_ADDR_VAR, strnlen_s(PECI_ADDR_VAR, sizeof(PECI_ADDR_VAR)),
             cmdInOut->out.stringVal, &mismatchclientaddr);
    if (mismatchclientaddr == 0)
    {
        cmdInOut->out.size = sizeof(uint8_t);
        cmdInOut->out.val.u64 = (uint64_t)cpuInfo->clientAddr;
        return ACD_SUCCESS;
    }
    strcmp_s(CPUID_VAR, strnlen_s(CPUID_VAR, sizeof(CPUID_VAR)),
             cmdInOut->out.stringVal, &mismatchcpuid);
    if (mismatchcpuid == 0)
    {
        cmdInOut->out.size = sizeof(uint32_t);
        cmdInOut->out.val.u64 = (uint64_t)(cpuInfo->cpuidRead.cpuModel |
                                           cpuInfo->cpuidRead.stepping);
        return ACD_SUCCESS;
    }
    strcmp_s(CPUID_SOURCE_VAR,
             strnlen_s(CPUID_SOURCE_VAR, sizeof(CPUID_SOURCE_VAR)),
             cmdInOut->out.stringVal, &mismatchcpuidsource);
    if (mismatchcpuidsource == 0)
    {
        cmdInOut->out.printString = true;
        cmdInOut->out.stringVal = decodeState((int)cpuInfo->cpuidRead.source);
        return ACD_SUCCESS;
    }
    strcmp_s(COREMASK_SOURCE_VAR,
             strnlen_s(COREMASK_SOURCE_VAR, sizeof(COREMASK_SOURCE_VAR)),
             cmdInOut->out.stringVal, &mismatchcoremasksource);
    if (mismatchcoremasksource == 0)
    {
        cmdInOut->out.printString = true;

        if (cpuInfo->dieMaskInfo.dieMaskSupported)
        {
            if (CHECK_BIT(cpuInfo->dieMaskInfo.compute.range,
                          cmdInOut->logger->contextLogger.domainID))
            {
                uint8_t logicalDieNum = cmdInOut->logger->contextLogger.compute;
                cmdInOut->out.stringVal =
                    decodeState((int)cpuInfo->computeDies[logicalDieNum]
                                    .coreMaskRead.source);
            }
        }
        else
        {
            cmdInOut->out.stringVal =
                decodeState((int)cpuInfo->coreMaskRead.source);
        }
        return ACD_SUCCESS;
    }
    strcmp_s(CHACOUNT_SOURCE_VAR,
             strnlen_s(CHACOUNT_SOURCE_VAR, sizeof(CHACOUNT_SOURCE_VAR)),
             cmdInOut->out.stringVal, &mismatchchacountsource);
    if (mismatchchacountsource == 0)
    {
        cmdInOut->out.printString = true;

        if (cpuInfo->dieMaskInfo.dieMaskSupported)
        {
            if CHECK_BIT (cpuInfo->dieMaskInfo.compute.range,
                          cmdInOut->logger->contextLogger.domainID)
            {
                uint8_t logicalDieNum = cmdInOut->logger->contextLogger.compute;
                cmdInOut->out.stringVal =
                    decodeState((int)cpuInfo->computeDies[logicalDieNum]
                                    .chaCountRead.source);
            }
        }
        else
        {
            cmdInOut->out.stringVal =
                decodeState((int)cpuInfo->chaCountRead.source);
        }
        return ACD_SUCCESS;
    }
    strcmp_s(CHACOUNTTOTAL_VAR,
             strnlen_s(CHACOUNTTOTAL_VAR, sizeof(CHACOUNTTOTAL_VAR)),
             cmdInOut->out.stringVal, &mismatchchacounttotal);
    if (mismatchchacounttotal == 0)
    {
        cmdInOut->out.size = sizeof(uint8_t);
        cmdInOut->out.val.u64 = (uint64_t)(cpuInfo->chaCount);
        return ACD_SUCCESS;
    }
    strcmp_s(COREMASK_VAR, strnlen_s(COREMASK_VAR, sizeof(COREMASK_VAR)),
             cmdInOut->out.stringVal, &mismatchcoremask);
    if (mismatchcoremask == 0)
    {
        cmdInOut->out.size = sizeof(uint64_t);
        if (cpuInfo->dieMaskInfo.dieMaskSupported)
        {
            if CHECK_BIT (cpuInfo->dieMaskInfo.compute.range,
                          cmdInOut->logger->contextLogger.domainID)
            {
                uint8_t logicalDieNum = cmdInOut->logger->contextLogger.compute;
                cmdInOut->out.val.u64 =
                    (uint64_t)(cpuInfo->computeDies[logicalDieNum].coreMask);
            }
        }
        else
        {
            cmdInOut->out.val.u64 = (uint64_t)(cpuInfo->coreMask);
        }
        return ACD_SUCCESS;
    }
    strcmp_s(CHAMASK_VAR, strnlen_s(CHAMASK_VAR, sizeof(CHAMASK_VAR)),
             cmdInOut->out.stringVal, &mismatchchamask);
    if (mismatchchamask == 0)
    {
        cmdInOut->out.size = sizeof(uint64_t);
        if (cpuInfo->dieMaskInfo.dieMaskSupported)
        {
            if CHECK_BIT (cpuInfo->dieMaskInfo.compute.range,
                          cmdInOut->logger->contextLogger.domainID)
            {
                uint8_t logicalDieNum = cmdInOut->logger->contextLogger.compute;
                cmdInOut->out.val.u64 =
                    (uint64_t)(cpuInfo->computeDies[logicalDieNum].chaMask);
            }
        }
        else
        {
            cmdInOut->out.val.u64 = (uint64_t)(cpuInfo->coreMask);
        }
        return ACD_SUCCESS;
    }
    strcmp_s(CHACOUNT_VAR, strnlen_s(CHACOUNT_VAR, sizeof(CHACOUNT_VAR)),
             cmdInOut->out.stringVal, &mismatchchacount);
    if (mismatchchacount == 0)
    {
        cmdInOut->out.size = sizeof(uint8_t);
        if (cpuInfo->dieMaskInfo.dieMaskSupported)
        {
            if CHECK_BIT (cpuInfo->dieMaskInfo.compute.range,
                          cmdInOut->logger->contextLogger.domainID)
            {
                uint8_t logicalDieNum = cmdInOut->logger->contextLogger.compute;
                cmdInOut->out.val.u64 =
                    (uint64_t)(cpuInfo->computeDies[logicalDieNum].chaCount);
            }
        }
        else
        {
            cmdInOut->out.val.u64 = (uint64_t)(cpuInfo->chaCount);
        }
        return ACD_SUCCESS;
    }
    strcmp_s(CORECOUNT_VAR, strnlen_s(CORECOUNT_VAR, sizeof(CORECOUNT_VAR)),
             cmdInOut->out.stringVal, &mismatchcorecount);
    if (mismatchcorecount == 0)
    {
        cmdInOut->out.size = sizeof(uint8_t);
        if (cpuInfo->dieMaskInfo.dieMaskSupported)
        {
            if CHECK_BIT (cpuInfo->dieMaskInfo.compute.range,
                          cmdInOut->logger->contextLogger.domainID)
            {
                uint8_t logicalDieNum = cmdInOut->logger->contextLogger.compute;
                cmdInOut->out.val.u64 = (uint64_t)(__builtin_popcountll(
                    cpuInfo->computeDies[logicalDieNum].coreMask));
            }
        }
        else
        {
            cmdInOut->out.val.u64 =
                (uint64_t)(__builtin_popcountll(cpuInfo->coreMask));
        }
        return ACD_SUCCESS;
    }
    strcmp_s(CRASHCORECOUNT_VAR,
             strnlen_s(CRASHCORECOUNT_VAR, sizeof(CRASHCORECOUNT_VAR)),
             cmdInOut->out.stringVal, &mismatchcrashcorecount);
    if (mismatchcrashcorecount == 0)
    {
        cmdInOut->out.size = sizeof(uint8_t);
        if (cpuInfo->dieMaskInfo.dieMaskSupported)
        {
            if CHECK_BIT (cpuInfo->dieMaskInfo.compute.range,
                          cmdInOut->logger->contextLogger.domainID)
            {
                uint8_t logicalDieNum = cmdInOut->logger->contextLogger.compute;
                cmdInOut->out.val.u64 = (uint64_t)(__builtin_popcountll(
                    cpuInfo->computeDies[logicalDieNum].crashedCoreMask));
            }
        }
        else
        {
            cmdInOut->out.val.u64 =
                (uint64_t)(__builtin_popcountll(cpuInfo->crashedCoreMask));
        }
        return ACD_SUCCESS;
    }
    strcmp_s(CRASHCOREMASK_VAR,
             strnlen_s(CRASHCOREMASK_VAR, sizeof(CRASHCOREMASK_VAR)),
             cmdInOut->out.stringVal, &mismatchcrashcoremask);
    if (mismatchcrashcoremask == 0)
    {
        cmdInOut->out.size = sizeof(uint64_t);
        if (cpuInfo->dieMaskInfo.dieMaskSupported)
        {
            if CHECK_BIT (cpuInfo->dieMaskInfo.compute.range,
                          cmdInOut->logger->contextLogger.domainID)
            {
                uint8_t logicalDieNum = cmdInOut->logger->contextLogger.compute;
                cmdInOut->out.val.u64 =
                    (uint64_t)(cpuInfo->computeDies[logicalDieNum]
                                   .crashedCoreMask);
            }
        }
        else
        {
            cmdInOut->out.val.u64 = (uint64_t)(cpuInfo->crashedCoreMask);
        }
        return ACD_SUCCESS;
    }
    strcmp_s(BMC_FW_VER_VAR, strnlen_s(BMC_FW_VER_VAR, sizeof(BMC_FW_VER_VAR)),
             cmdInOut->out.stringVal, &mismatchbmcfwver);
    if (mismatchbmcfwver == 0)
    {
        cmdInOut->out.printString = true;
        cmdInOut->out.stringVal = cmdInOut->platformInfo->bmc_fw_ver;
        return ACD_SUCCESS;
    }
    strcmp_s(BIOS_ID_VAR, strnlen_s(BIOS_ID_VAR, sizeof(BIOS_ID_VAR)),
             cmdInOut->out.stringVal, &mismatchbiosid);
    if (mismatchbiosid == 0)
    {
        cmdInOut->out.printString = true;
        cmdInOut->out.stringVal = cmdInOut->platformInfo->bios_id;
        return ACD_SUCCESS;
    }
    strcmp_s(_TIMESTAMP_VAR, strnlen_s(_TIMESTAMP_VAR, sizeof(_TIMESTAMP_VAR)),
             cmdInOut->out.stringVal, &mismatchtimestamp);
    if (mismatchtimestamp == 0)
    {
        cmdInOut->out.printString = true;
        cmdInOut->out.stringVal = cmdInOut->platformInfo->time_stamp;
        return ACD_SUCCESS;
    }
    strcmp_s(TRIGGER_TYPE_VAR,
             strnlen_s(TRIGGER_TYPE_VAR, sizeof(TRIGGER_TYPE_VAR)),
             cmdInOut->out.stringVal, &mismatchtriggertype);
    if (mismatchtriggertype == 0)
    {
        cmdInOut->out.printString = true;
        cmdInOut->out.stringVal = cmdInOut->platformInfo->trigger_type;
        return ACD_SUCCESS;
    }
    strcmp_s(PLATFORM_NAME_VAR,
             strnlen_s(PLATFORM_NAME_VAR, sizeof(PLATFORM_NAME_VAR)),
             cmdInOut->out.stringVal, &mismatchplatformname);
    if (mismatchplatformname == 0)
    {
        cmdInOut->out.printString = true;
        cmdInOut->out.stringVal = cmdInOut->platformInfo->platform_name;
        return ACD_SUCCESS;
    }
    strcmp_s(CRASHDUMP_VER_VAR,
             strnlen_s(CRASHDUMP_VER_VAR, sizeof(CRASHDUMP_VER_VAR)),
             cmdInOut->out.stringVal, &mismatchcrashdumpver);
    if (mismatchcrashdumpver == 0)
    {
        cmdInOut->out.printString = true;
        cmdInOut->out.stringVal = cmdInOut->platformInfo->crashdump_ver;
        return ACD_SUCCESS;
    }
    strcmp_s(INPUT_FILE_VER_VAR,
             strnlen_s(INPUT_FILE_VER_VAR, sizeof(INPUT_FILE_VER_VAR)),
             cmdInOut->out.stringVal, &mismatchinputfilever);
    if (mismatchinputfilever == 0)
    {
        cmdInOut->out.printString = true;
        cmdInOut->out.stringVal = cmdInOut->platformInfo->inputfile_ver;
        return ACD_SUCCESS;
    }
    strcmp_s(_INPUT_FILE_VAR,
             strnlen_s(_INPUT_FILE_VAR, sizeof(_INPUT_FILE_VAR)),
             cmdInOut->out.stringVal, &mismatchinputfile);
    if (mismatchinputfile == 0)
    {
        cmdInOut->out.printString = true;
        cmdInOut->out.stringVal = cmdInOut->platformInfo->input_file;
        return ACD_SUCCESS;
    }
    strcmp_s(DIEMASK_VAR, strnlen_s(DIEMASK_VAR, sizeof(DIEMASK_VAR)),
             cmdInOut->out.stringVal, &mismatchdiemask);
    if (mismatchdiemask == 0)
    {
        cmdInOut->out.size = sizeof(uint32_t);
        cmdInOut->out.val.u64 = (uint32_t)cpuInfo->dieMaskInfo.dieMask;
        return ACD_SUCCESS;
    }
    strcmp_s(PLATFORM_ID_VAR,
             strnlen_s(PLATFORM_ID_VAR, sizeof(PLATFORM_ID_VAR)),
             cmdInOut->out.stringVal, &mismatchplatformid);
    if (mismatchplatformid == 0)
    {
        cmdInOut->out.size = sizeof(uint32_t);
        cmdInOut->out.val.u64 = (uint32_t)cpuInfo->platformID;
        return ACD_SUCCESS;
    }
    strcmp_s(UCODE_PATCH_VER_VAR,
             strnlen_s(UCODE_PATCH_VER_VAR, sizeof(UCODE_PATCH_VER_VAR)),
             cmdInOut->out.stringVal, &mismatchucodepatchver);
    if (mismatchucodepatchver == 0)
    {
        cmdInOut->out.size = sizeof(uint32_t);
        cmdInOut->out.val.u64 = (uint32_t)cpuInfo->ucodePatch;
        return ACD_SUCCESS;
    }
    strcmp_s(CJSON_VER_VAR, strnlen_s(CJSON_VER_VAR, sizeof(CJSON_VER_VAR)),
             cmdInOut->out.stringVal, &mismatchcjsonver);
    if (mismatchcjsonver == 0)
    {
        cmdInOut->out.printString = true;
        cmdInOut->out.stringVal = (char*)cJSON_Version();
        return ACD_SUCCESS;
    }
    cmdInOut->out.printString = false;
    cmdInOut->out.val.u64 = 0;
    CRASHDUMP_PRINT(ERR, stderr, "Unknown RdGlobalVar command.\n");
    return ACD_FAILURE;
}

static acdStatus SaveStrVars(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    if (!IsSaveStrVarsValid(cmdInOut->in.params, cmdInOut->validatorParams))
    {
        return ACD_INVALID_SAVE_STR_VARS_PARAMS;
    }
    cmdInOut->out.ret = PECI_CC_SUCCESS;
    cmdInOut->out.cc = PECI_DEV_CC_SUCCESS;
    cmdInOut->out.printString = true;
    return ACD_SUCCESS;
}

static uint64_t getPayloadExp(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    cJSON* payloadSize = cJSON_GetObjectItemCaseSensitive(
        cmdInOut->internalVarsTracker, "CrashdumpPayloadSize");
    if (payloadSize == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Could not read CrashdumpPayloadSize\n");
        return 0;
    }
    char* pCrashdumpPayloadSize = payloadSize->valuestring;
    uint8_t u8PayloadExp = (uint8_t)strtol(pCrashdumpPayloadSize, NULL, 16);
    return u8PayloadExp;
}

static acdStatus LogTor(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    cmdInOut->out.size = sizeof(uint8_t);
    if (GenerateJsonPath(cmdInOut, cmdInOut->root, cmdInOut->logger, false) ==
        ACD_SUCCESS)
    {
        if (getIsCrashdumpEnableResult(cmdInOut, cpuInfo,
                                       "IsCrashdumpEnable") &&
            getNumCrashdumpAgentsResult(cmdInOut, cpuInfo,
                                        "NumCrashdumpAgents") &&
            getCrashdumpGUIDResult(cmdInOut, cpuInfo, "CrashdumpGUID") &&
            getCrashdumpPayloadSizeResult(cmdInOut, cpuInfo,
                                          "CrashdumpPayloadSize"))
        {
            uint8_t u8PayloadExp = getPayloadExp(cmdInOut, cpuInfo);
            if (u8PayloadExp > TOR_MAX_PAYLOAD_EXP ||
                u8PayloadExp == TOR_INVALID_PAYLOAD_EXP)
            {
                CRASHDUMP_PRINT(
                    ERR, stderr,
                    "Error during discovery (Invalid exponent value: %d)\n",
                    u8PayloadExp);
                return ACD_FAILURE;
            }
            uint8_t u8PayloadBytes = (1 << u8PayloadExp);
            return logTorSection(
                cpuInfo, cmdInOut->logger->nameProcessing.jsonOutput,
                u8PayloadBytes, cmdInOut->logger->contextLogger.domainID,
                cmdInOut->logger->contextLogger.compute, *cmdInOut->runTimeInfo,
                cmdInOut->logger);
        }
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "Could not log Tor, path is Null.\n");
    }
    return ACD_FAILURE;
}

static acdStatus LogNVD(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
#ifdef NVD_SECTION
    if (GenerateJsonPath(cmdInOut, cmdInOut->root, cmdInOut->logger, false) ==
        ACD_SUCCESS)
    {
        acdStatus nvdStatus = fillDimmMasks(cpuInfo);
        if (nvdStatus == ACD_SUCCESS)
        {
            for (size_t cpu = 0; cpu < getNumberOfCpus(cpuInfo); cpu++)
            {
                nvdStatus = fillNVDSection(
                    cpuInfo, cpu, cmdInOut->logger->nameProcessing.jsonOutput,
                    cmdInOut->runTimeInfo);
            }
        }
        return ACD_SUCCESS;
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "Could not log NVD, path is Null.\n");
    }
    return ACD_FAILURE;
#else
    return ACD_SUCCESS;
#endif
}

static acdStatus LogNACCrashdump(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
#ifdef PMT_NAC_CRASHLOG
    logNACPMTCrashlog(cmdInOut->logger->nameProcessing.jsonOutput);
#endif
    return ACD_SUCCESS;
}

void LogPhy2LogCore(CmdInOut* cmdInOut, CPUInfo* cpuInfo, uint8_t computeToLog)
{
    int IndexCount = 0;
    uint32_t* coreTable;
    char keyStr[10];
    // Clear out any peci info
    cmdInOut->out.ret = PECI_CC_SUCCESS;
    cmdInOut->out.cc = PECI_DEV_CC_SUCCESS;

    // Return for specific compute specified in inputfile
    coreTable = cpuInfo->phy2logReadCore.table[computeToLog];
    IndexCount =
        sizeof(cpuInfo->phy2logReadCore.table[computeToLog]) / sizeof(uint32_t);

    // Log Core ID Data
    for (int i = 0; i < IndexCount; i++)
    {
        // Update Name
        cd_snprintf_s(keyStr, sizeof(keyStr), "index%d", i);
        cmdInOut->logger->nameProcessing.registerName = keyStr;

        // Update Value
        cmdInOut->out.size = sizeof(uint32_t);
        cmdInOut->out.val.u64 = (uint32_t)coreTable[i];
        LogRegister(cmdInOut, cmdInOut->root, cmdInOut->logger);
    }

    // No more logging through flow.
    cmdInOut->logger->nameProcessing.logRegister = false;
}

void LogPhy2LogCHA(CmdInOut* cmdInOut, CPUInfo* cpuInfo, uint8_t computeToLog)
{
    int IndexCount = 0;
    uint32_t* coreTable;
    char keyStr[24];
    // Clear out any peci info
    cmdInOut->out.ret = PECI_CC_SUCCESS;
    cmdInOut->out.cc = PECI_DEV_CC_SUCCESS;

    // Return for specific compute specified in inputfile
    coreTable = cpuInfo->phy2logReadCHA.table[computeToLog];
    IndexCount =
        sizeof(cpuInfo->phy2logReadCHA.table[computeToLog]) / sizeof(uint32_t);

    // Log CHA ID Data
    for (int i = 0; i < IndexCount; i++)
    {
        // Update Name
        cd_snprintf_s(keyStr, sizeof(keyStr), "index_0x%x%x", computeToLog, i);
        cmdInOut->logger->nameProcessing.registerName = keyStr;

        // Update Value
        cmdInOut->out.size = sizeof(uint32_t);
        cmdInOut->out.val.u64 = (uint32_t)coreTable[i];
        LogRegister(cmdInOut, cmdInOut->root, cmdInOut->logger);
    }

    // No more logging through flow.
    cmdInOut->logger->nameProcessing.logRegister = false;
}

static acdStatus RdPhy2LogTranslation(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    uint position = 0;
    cJSON* it = NULL;
    uint8_t computeIndex = 0;
    uint8_t domainId = 0;
    char* type = NULL;

    if (!IsRdPhy2LogVarsValid(cmdInOut->in.params, cmdInOut->validatorParams))
    {
        return ACD_INVALID_RDPHY2LOG_PARAMS;
    }

    cJSON_ArrayForEach(it, cmdInOut->in.params)
    {
        switch (position)
        {
            case 0:
                domainId = it->valueint;
                break;
            case 1:
                type = it->valuestring;
                break;
        }
        position++;
    }

    if (CHECK_BIT(cpuInfo->dieMaskInfo.compute.range, domainId))
    {
        computeIndex = cmdInOut->logger->contextLogger.compute;
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Incorrect domainID for phy2log translation\n");
        return ACD_INVALID_RDPHY2LOG_PARAMS;
    }

    // Determine the type
    int mismatchType = 1;
    strcmp_s(PHY2LOG_CORE_ID, sizeof(PHY2LOG_CORE_ID), type, &mismatchType);
    if (mismatchType == 0)
    {
        // Log full table for compute die
        LogPhy2LogCore(cmdInOut, cpuInfo, computeIndex);
    }
    mismatchType = 1;
    strcmp_s(PHY2LOG_CHA_ID, sizeof(PHY2LOG_CHA_ID), type, &mismatchType);
    if (mismatchType == 0)
    {
        // Log full table for compute die
        LogPhy2LogCHA(cmdInOut, cpuInfo, computeIndex);
    }

    UpdateInternalVar(cmdInOut, cpuInfo);
    return ACD_SUCCESS;
}

static acdStatus ParseModuleResponse(RemoteModuleResponse* rmResponse,
                                     LoggerStruct* loggerStruct,
                                     CmdInOut* cmdInOut)
{
    cJSON* response;

    if (rmResponse->jsonString != NULL)
    {
        // Validate JSON from module
        response = cJSON_Parse(rmResponse->jsonString);
        if (response == NULL)
        {
            CRASHDUMP_PRINT(ERR, stderr,
                            "Module response json is invalid: %s\n",
                            rmResponse->jsonString);
            return ACD_FAILURE;
        }
        else
        {
            /* We can't add the full object; We need to individually
               add items for the correct structure */
            if (cJSON_IsBool(response->child))
            {
                cJSON_AddBoolToObject(loggerStruct->nameProcessing.jsonOutput,
                                      response->child->string,
                                      cJSON_IsTrue(response->child));
            }
            else if (cJSON_IsNumber(response->child))
            {
                cJSON_AddNumberToObject(loggerStruct->nameProcessing.jsonOutput,
                                        response->child->string,
                                        response->child->valueint);
            }
            else if (cJSON_IsString(response->child))
            {
                cJSON_AddStringToObject(loggerStruct->nameProcessing.jsonOutput,
                                        response->child->string,
                                        response->child->valuestring);
            }
            else if (cJSON_IsObject(response->child))
            {
                // TODO: How to handle a nested object
            }
        }
        return ACD_SUCCESS;
    }
    return ACD_FAILURE;
}
static acdStatus LogModuleData(RemoteModuleResponse* rmResponse,
                               LoggerStruct* loggerStruct, CmdInOut* cmdInOut)
{
    // Update Logger Hierarchy via OutputPath
    uint32_t validLevels = 0;

    acdStatus result = ACD_SUCCESS;

    // Copy Outputpath Prefix
    strcpy_s(loggerStruct->pathParsing.pathString, LOGGER_JSON_PATH_STRING_LEN,
             loggerStruct->pathParsing.pathStringRaw);

    // Add valid nodes to Output Path Prefix
    for (int i = 0; i < MAX_NODE_LEVELS; i++)
    {
        if (rmResponse->nodeLevel[i] == NULL)
        {
            break;
        }
        validLevels++;
        strcat_s(loggerStruct->pathParsing.pathString,
                 LOGGER_JSON_PATH_STRING_LEN, "/");

        // TODO: check string size isn't large
        strcat_s(loggerStruct->pathParsing.pathString,
                 LOGGER_JSON_PATH_STRING_LEN, rmResponse->nodeLevel[i]);
    }

    // Update new outputpath tokens
    ParsePath(loggerStruct);

    if (GenerateJsonPath(cmdInOut, cmdInOut->root, cmdInOut->logger, false) !=
        ACD_SUCCESS)
    {
        return ACD_ALLOCATE_FAILURE;
    }

    // Parse and Log
    result = ParseModuleResponse(rmResponse, loggerStruct, cmdInOut);
    return result;
}

static acdStatus InitRemoteModule(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    acdStatus result = ACD_SUCCESS;

    // Load Module
    if (cmdInOut->rmSectionInfo->moduleHandle == NULL)
    {
        result = loadModule(&cmdInOut->rmSectionInfo->moduleHandle,
                            cmdInOut->rmSectionInfo->modulePath);
        if (result != ACD_SUCCESS)
        {
            CRASHDUMP_PRINT(ERR, stderr, "Module Failure: Can't load.\n");
            cmdInOut->rmSectionInfo->instanceLoops = 0;
            cmdInOut->rmSectionInfo->moduleHandle = NULL;
            return result;
        }

        // Validate Module
        VERBOSE_PRINT(ERR, stderr, "Validate Module\n");
        result = validateModule(cmdInOut->rmSectionInfo->moduleHandle);
        if (result != ACD_SUCCESS)
        {
            CRASHDUMP_PRINT(ERR, stderr, "Module Failure: API Invalid.\n");
            cmdInOut->rmSectionInfo->moduleHandle = NULL;
            return result;
        }
    }

    // Execute module rm_init()
    VERBOSE_PRINT(ERR, stderr, "Calling rm_init()\n");
    cmdInOut->rmSectionInfo->rmModuleInfo->iteration = 0;
    result = moduleCallInit(cmdInOut->rmSectionInfo->rmModuleInfo);
    if (result != ACD_SUCCESS)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Module Response: Status %d\n", result);
        // TODO: Do we want to continue here ?
    }

    return result;
}

static acdStatus ConfigRemoteModule(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    RemoteModuleConfig* rmModuleConfig =
        cmdInOut->rmSectionInfo->rmModuleConfig;
    RemoteModuleInfo* rmInfo = cmdInOut->rmSectionInfo->rmModuleInfo;
    if (cmdInOut->rmSectionInfo->moduleHandle != NULL)
    {
        // Execute module rm_config()
        VERBOSE_PRINT(ERR, stderr, "Calling Config\n");
        rmStatus result = moduleCallConfig(rmInfo, rmModuleConfig);

        if (result != STATUS_SUCCESS)
        {
            VERBOSE_PRINT(ERR, stderr, "Module Response: Status %d\n", result);
        }
        // Verify API version
        if (rmModuleConfig->apiVersion != VERSION_1)
        {
            CRASHDUMP_PRINT(ERR, stderr,
                            "Module API version is not supported: %d\n",
                            rmModuleConfig->apiVersion);
            return ACD_MODULE_INVALID_API;
        }
        // Verify Execution Count is within limits
        if (rmModuleConfig->executionCount > MAX_EXECUTION_COUNT)
        {
            CRASHDUMP_PRINT(
                ERR, stderr,
                "Module execution count is overlimit and capped at %d: %d\n",
                MAX_EXECUTION_COUNT, rmModuleConfig->executionCount);
            rmModuleConfig->executionCount = MAX_EXECUTION_COUNT;
        }
    }
    else
    {
        VERBOSE_PRINT(ERR, stderr,
                      "No Module found. Skipping ConfigRemoteModule\n");
    }
    return ACD_SUCCESS;
}

static acdStatus ExecuteRemoteModule(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    acdStatus result = ACD_FAILURE;
    RemoteModuleResponse* rmResponse =
        cmdInOut->rmSectionInfo->rmModuleResponse;
    RemoteModuleInfo* rmInfo = cmdInOut->rmSectionInfo->rmModuleInfo;
    RemoteModuleConfig* rmConfig = cmdInOut->rmSectionInfo->rmModuleConfig;
    RemoteModuleSectionInfo* sectionInfo = cmdInOut->rmSectionInfo;

    // Call Config
    VERBOSE_PRINT(ERR, stderr, "Starting ExecuteRemoteModule\n");
    if ((result = ConfigRemoteModule(cmdInOut, cpuInfo)) != ACD_SUCCESS)
    {
        return result;
    }

    if (sectionInfo->moduleHandle != NULL)
    {
        // Check if Instance is in params
        if (sectionInfo->instanceLoops > 1 &&
            sectionInfo->instanceArgumentIndex == INVALID_VALUE)
        {
            CRASHDUMP_PRINT(ERR, stderr,
                            "Instance was not found in params but "
                            "LoopOnInstance is enabled (%d)\n",
                            rmConfig->executionCount);
            sectionInfo->instanceLoops = 0;
        }

        for (int instance = 0; instance < sectionInfo->instanceLoops;
             instance++)
        {
            // Update instance argument
            if (sectionInfo->instanceArgumentIndex != INVALID_VALUE)
            {
                rmInfo->intArg[sectionInfo->instanceArgumentIndex] = instance;
            }

            VERBOSE_PRINT(
                ERR, stderr,
                "Executing instance %d at requested iterations: %d \n",
                instance, rmConfig->executionCount);

            rmInfo->iteration = 0;

            for (int i = 0; i < rmConfig->executionCount; i++)
            {
                // Execute module rm_execute()
                result = moduleCallExecute(rmInfo, rmResponse);
                if (result != ACD_SUCCESS)
                {
                    CRASHDUMP_PRINT(ERR, stderr,
                                    "Module status (%d) at iteration %d\n",
                                    result, i);
                }

                // Log data
                LogModuleData(rmResponse, cmdInOut->logger, cmdInOut);
                rmInfo->iteration++;
            }
        }
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "No Module found. Skipping ExecuteRemoteModule\n");
    }

    return ACD_SUCCESS;
}

static acdStatus DeinitRemoteModule(CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    acdStatus result = ACD_SUCCESS;
    RemoteModuleInfo* rmInfo = cmdInOut->rmSectionInfo->rmModuleInfo;

    // call deInit Module
    if (cmdInOut->rmSectionInfo->moduleHandle != NULL)
    {
        result = moduleCallDeinit(rmInfo);
        VERBOSE_PRINT(ERR, stderr, "Deinitializing Module\n");
    }

    return result;
}
static acdStatus (*cmds[CD_NUM_OF_PECI_CMDS])() = {
    (acdStatus(*)())CrashDump_Discovery,
    (acdStatus(*)())CrashDump_Discovery_Dom,
    (acdStatus(*)())CrashDump_GetFrame,
    (acdStatus(*)())CrashDump_GetFrame_Dom,
    (acdStatus(*)())Ping,
    (acdStatus(*)())GetCPUID,
    (acdStatus(*)())RdEndPointConfigMmio,
    (acdStatus(*)())RdEndPointConfigMmio_Dom,
    (acdStatus(*)())RdPCIConfigLocal,
    (acdStatus(*)())RdPCIConfigLocal_Dom,
    (acdStatus(*)())RdEndPointConfigPciLocal,
    (acdStatus(*)())RdEndPointConfigPciLocal_Dom,
    (acdStatus(*)())WrEndPointConfigPciLocal,
    (acdStatus(*)())WrEndPointConfigPciLocal_Dom,
    (acdStatus(*)())RdIAMSR,
    (acdStatus(*)())RdIAMSR_Dom,
    (acdStatus(*)())RdPkgConfig,
    (acdStatus(*)())RdPkgConfig_Dom,
    (acdStatus(*)())RdPkgConfigCore,
    (acdStatus(*)())RdPkgConfigCore_Dom,
    (acdStatus(*)())RdPostEnumBus,
    (acdStatus(*)())RdChaCount,
    (acdStatus(*)())Telemetry_Discovery,
    (acdStatus(*)())Telemetry_Discovery_Dom,
    (acdStatus(*)())LogCrashlog,
    (acdStatus(*)())LogBigCore,
    (acdStatus(*)())LogAtomCore,
    (acdStatus(*)())RdAndConcatenate,
    (acdStatus(*)())RdGlobalVars,
    (acdStatus(*)())SaveStrVars,
    (acdStatus(*)())LogTor,
    (acdStatus(*)())LogNVD,
    (acdStatus(*)())LogNACCrashdump,
    (acdStatus(*)())RdPhy2LogTranslation,
    (acdStatus(*)())InitRemoteModule,
    (acdStatus(*)())ExecuteRemoteModule,
    (acdStatus(*)())DeinitRemoteModule,
};

static char* inputCMDs[CD_NUM_OF_PECI_CMDS] = {"CrashDump_Discovery",
                                               "CrashDump_Discovery_dom",
                                               "CrashDump_GetFrame",
                                               "CrashDump_GetFrame_dom",
                                               "Ping",
                                               "GetCPUID",
                                               "RdEndpointConfigMMIO",
                                               "RdEndpointConfigMMIO_dom",
                                               "RdPCIConfigLocal",
                                               "RdPCIConfigLocal_dom",
                                               "RdEndpointConfigPCILocal",
                                               "RdEndpointConfigPCILocal_dom",
                                               "WrEndPointPCIConfigLocal",
                                               "WrEndPointPCIConfigLocal_dom",
                                               "RdIAMSR",
                                               "RdIAMSR_dom",
                                               "RdPkgConfig",
                                               "RdPkgConfig_dom",
                                               "RdPkgConfigCore",
                                               "RdPkgConfigCore_dom",
                                               "RdPostEnumBus",
                                               "RdChaCount",
                                               "Telemetry_Discovery",
                                               "Telemetry_Discovery_dom",
                                               "LogCrashlog",
                                               "LogBigCore",
                                               "LogAtomCore",
                                               "RdAndConcatenate",
                                               "RdGlobalVars",
                                               "SaveStrVars",
                                               "LogTor",
                                               "LogNVD",
                                               "LogNACCrashdump",
                                               "RdPhy2LogTranslation",
                                               "InitRemoteModule",
                                               "ExecuteRemoteModule",
                                               "DeinitRemoteModule"};

acdStatus BuildCmdsTable(ENTRY* entry)
{
    ENTRY* ep;

    hcreate(CD_NUM_OF_PECI_CMDS);
    for (int i = 0; i < CD_NUM_OF_PECI_CMDS; i++)
    {
        entry->key = inputCMDs[i];
        entry->data = (void*)(size_t)i;
        ep = hsearch(*entry, ENTER);
        if (ep == NULL)
        {
            CRASHDUMP_PRINT(ERR, stderr, "Fail adding (%s) to commands table\n",
                            entry->key);
            return ACD_FAILURE_CMD_TABLE;
        }
    }
    return ACD_SUCCESS;
}

acdStatus Execute(ENTRY* entry, CmdInOut* cmdInOut, CPUInfo* cpuInfo)
{
    ENTRY* ep;

    ep = hsearch(*entry, FIND);
    if (ep == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Invalid PECICmd:(%s)\n", entry->key);
        return ACD_INVALID_CMD;
    }
    return cmds[(size_t)(ep->data)](cmdInOut, cpuInfo);
}
