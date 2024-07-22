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

#include "crashdump.h"

#include "BigCore.h"
#include "Crashlog.h"
#include "TorDump.h"
#include "flow.h"
#include "safe_mem_lib.h"
#include "tpmi.h"
#include "utils.h"
#include "utils_common.h"

int const vcuPeciWake = 5;

int revision_uncore = 0;
bool commonMetaDataEnabled = true;
const DieMaskBits DieMaskRange = {0xFFFFFFFF};

void fillMetaDataCommon(PlatformInfo* platformInfo, CPUInfo* cpuInfo,
                        char* platformName, char* triggerType, char* timestamp,
                        char* biosVersion, char* bmcVersion)
{
    platformInfo->trigger_type = triggerType;
    platformInfo->time_stamp = timestamp;
    platformInfo->platform_name = platformName;
    fillBiosId(biosVersion, SI_BMC_VER_LEN);
    platformInfo->bios_id = biosVersion;
    fillBmcVersion(bmcVersion, SI_BMC_VER_LEN);
    platformInfo->bmc_fw_ver = bmcVersion;
    platformInfo->crashdump_ver = CRASHDUMP_VER;
    fillInputFile(cpuInfo, platformInfo);
    logInputFileVersion(cpuInfo, platformInfo);
}

void setResetDetected(PlatformState* platformState)
{
    if (!platformState->resetDetected)
    {
        platformState->resetDetected = true;
    }
}

void clearResetDetected(PlatformState* platformState)
{
    platformState->resetDetected = false;
    platformState->resetCpu = DEFAULT_VALUE;
    errno_t err = strcpy_s(platformState->resetSectionName,
                           sizeof(platformState->resetSectionName), MD_NA);
    if (err != EOK)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Error copying string: %s\n", MD_NA);
    }
}

void updateResetInfo(PlatformState* platformState, uint8_t cpu,
                     char* sectionName)
{
    if (!platformState->resetDetected)
    {
        errno_t err =
            strcpy_s(platformState->resetSectionName,
                     sizeof(platformState->resetSectionName), sectionName);
        if (err != EOK)
        {
            CRASHDUMP_PRINT(ERR, stderr, "Error copying section name: %s\n",
                            sectionName);
        }
        platformState->resetCpu = cpu;
    }
}

void logCrashdumpVersion(cJSON* parent, CPUInfo* cpuInfo, int recordType)
{
    VersionInfo cpuProduct[cd_numberOfModels] = {
        {cd_spr, PRODUCT_TYPE_SPR},    {cd_emr, PRODUCT_TYPE_EMR},
        {cd_gnr, PRODUCT_TYPE_GNR_SP}, {cd_gnrd, PRODUCT_TYPE_GNR_D},
        {cd_srf, PRODUCT_TYPE_SRF_SP},
    };

    VersionInfo cpuRevision[cd_numberOfModels] = {
        {cd_spr, REVISION_1},  {cd_emr, REVISION_1}, {cd_gnr, REVISION_1},
        {cd_gnrd, REVISION_1}, {cd_srf, REVISION_1},
    };

    int productType = 0;
    for (int i = 0; i < cd_numberOfModels; i++)
    {
        if (cpuInfo->model == cpuProduct[i].cpuModel)
        {
            productType = cpuProduct[i].data;
        }
    }

    int revisionNum = 0;
    for (int i = 0; i < cd_numberOfModels; i++)
    {
        if (cpuInfo->model == cpuRevision[i].cpuModel)
        {
            revisionNum = cpuRevision[i].data;
        }
    }

    if (recordType == RECORD_TYPE_UNCORESTATUSLOG)
    {
        revisionNum = revision_uncore;
    }

    // Build the version number:
    //  [31:30] Reserved
    //  [29:24] Crash Record Type
    //  [23:12] Product
    //  [11:8] Reserved
    //  [7:0] Revision
    int version = recordType << RECORD_TYPE_OFFSET |
                  productType << PRODUCT_TYPE_OFFSET |
                  revisionNum << REVISION_OFFSET;
    char versionString[64];
    cd_snprintf_s(versionString, sizeof(versionString), "0x%x", version);
    cJSON_AddStringToObject(parent, "_version", versionString);
}

void getDieMaskGNR(CPUInfo* cpu)
{
    uint8_t cc = 0;
    uint32_t peciRdValue = 0;
    EPECIStatus retval;

    // PCS 0, Parameter 7 contains die mask information
    retval = peci_RdPkgConfig(cpu->clientAddr, 0, 7, sizeof(uint32_t),
                              (uint8_t*)&peciRdValue, &cc);
    if (retval != PECI_CC_SUCCESS || PECI_CC_UA(cc))
    {
        cpu->dieMaskInfo.dieMask = 0x0;
        CRASHDUMP_PRINT(
            ERR, stderr,
            "Get die mask failed! ret: (0x%x), cc: (0x%x) on PECI address %d\n",
            retval, cc, cpu->clientAddr);
    }
    else
    {
        cpu->dieMaskInfo.dieMask = peciRdValue;
    }
}

void initComputeDieStructure(DieMaskInfo* dieMaskInfo,
                             ComputeDieInfo** computeDies)
{
    uint8_t computeDieCount =
        __builtin_popcount(dieMaskInfo->compute.effectiveMask);
    if (computeDieCount == 0)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Compute die count is 0! dieMask: (0x%x)\n",
                        dieMaskInfo->dieMask);
        return;
    }

    *computeDies = calloc(computeDieCount, sizeof(ComputeDieInfo));
    CRASHDUMP_PRINT(
        ERR, stderr,
        "Initialized computeDieInfo - (dieMask:0x%x) (computeDieCount:%d)\n",
        dieMaskInfo->dieMask, computeDieCount);
    if (*computeDies == NULL)
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "Cannot allocate memory for compute die! computeDieCount: (%d)\n",
            computeDieCount);
    }
}

void initDieReadStructure(DieMaskInfo* dieMaskInfo, uint32_t computeRange,
                          uint32_t ioRange)
{
    if (dieMaskInfo->dieMask == 0)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Can't initialize DieRead on invalid diemask!\n");
        dieMaskInfo->maskValid = false;
        return;
    }
    dieMaskInfo->maskValid = true;
    /*
        Calculate Offsets.
        NOTE: Assumption is that IO die space resides in the LSB
        and compute resides in the MSB of the diemask.
    */
    dieMaskInfo->compute.offset = __builtin_popcount(ioRange);
    dieMaskInfo->io.offset = 0;

    dieMaskInfo->compute.range = computeRange << dieMaskInfo->compute.offset;
    dieMaskInfo->io.range = ioRange;

    dieMaskInfo->compute.effectiveMask =
        ((dieMaskInfo->dieMask >> dieMaskInfo->compute.offset) &
         (dieMaskInfo->compute.range >> dieMaskInfo->compute.offset));
    dieMaskInfo->io.effectiveMask =
        ((dieMaskInfo->dieMask >> dieMaskInfo->io.offset) &
         (dieMaskInfo->io.range >> dieMaskInfo->io.offset));

    dieMaskInfo->compute.maxDies =
        __builtin_popcount(dieMaskInfo->compute.range);
    dieMaskInfo->io.maxDies = __builtin_popcount(dieMaskInfo->io.range);
}
void getDieMasks(CPUInfo* cpus, cpuidState cpuState)
{
    uint32_t ioRange = 0;
    uint32_t computeRange = 0;
    uint32_t maxDieMask = 0;

    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        else if (cpu->dieMaskInfo.maskValid)
        {
            CRASHDUMP_PRINT(INFO, stderr,
                            "Using stored (dieMask:0x%x) on PECI address %d\n",
                            cpu->dieMaskInfo.dieMask, cpu->clientAddr);
            continue;
        }

        switch (cpu->model)
        {
            case cd_srf:
                ioRange = DieMaskRange.SRF.io;
                computeRange = DieMaskRange.SRF.compute;
                maxDieMask = SRF_MAX_DIEMASK;
                cpu->dieMaskInfo.dieMaskSupported = true;
                // Same command as GNR
                getDieMaskGNR(cpu);
                break;
            case cd_gnr:
            case cd_gnrd:
                ioRange = DieMaskRange.GNR.io;
                computeRange = DieMaskRange.GNR.compute;
                maxDieMask = GNR_MAX_DIEMASK;
                cpu->dieMaskInfo.dieMaskSupported = true;
                getDieMaskGNR(cpu);
                break;
            default:
                // Diemask isn't supported
                cpu->dieMaskInfo.dieMaskSupported = false;
                continue;
                break;
        }

        if (cpu->dieMaskInfo.dieMaskSupported)
        {
            // Only override invalid dieMask on EVENT
            if (cpu->dieMaskInfo.dieMask == 0 && (cpuState == EVENT))
            {
                cpu->dieMaskInfo.dieMask = maxDieMask;

                CRASHDUMP_PRINT(ERR, stderr,
                                "Using override (dieMask:0x%x) on PECI "
                                "address %d\n",
                                cpu->dieMaskInfo.dieMask, cpu->clientAddr);
            }
            // Initialize structures once
            if (cpu->computeDies == NULL && (cpu->dieMaskInfo.dieMask != 0))
            {
                initDieReadStructure(&cpu->dieMaskInfo, computeRange, ioRange);
                initComputeDieStructure(&cpu->dieMaskInfo, &cpu->computeDies);
                cpu->dieMaskInfo.maskValid = true;
            }
        }
        else
        {
            // Do nothing
        }
    }
}
void getComputeDieCoreMasks(uint8_t clientAddr, DieMaskInfo* dieMaskInfo,
                            uint8_t domainID, ComputeDieInfo* computeDie,
                            cpuidState cpuState)
{
    uint8_t cc = 0;
    uint32_t coreMask0 = 0x0;
    uint32_t coreMask1 = 0x0;
    computeDie->coreMask = 0;

    EPECIStatus retval;

    int device = UpdateGnrPcuDeviceNum(dieMaskInfo,
                                       domainID + dieMaskInfo->compute.offset);
    domainID = domainID + dieMaskInfo->compute.offset;
    retval = peci_RdEndPointConfigPciLocal_dom(
        clientAddr, domainID, 0, 30, device, 0, 0x488, sizeof(coreMask0),
        (uint8_t*)&coreMask0, &cc);
    computeDie->coreMaskRead.coreMaskCc = cc;
    computeDie->coreMaskRead.coreMaskRet = retval;
    if (retval != PECI_CC_SUCCESS || PECI_CC_UA(cc))
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Cannot find compute die coreMask0! ret: (0x%x), cc: "
                        "(0x%x), domainID: (%d)\n",
                        retval, cc, domainID);
        return;
    }
    retval = peci_RdEndPointConfigPciLocal_dom(
        clientAddr, domainID, 0, 30, device, 0, 0x48c, sizeof(coreMask1),
        (uint8_t*)&coreMask1, &cc);
    computeDie->coreMaskRead.coreMaskCc = cc;
    computeDie->coreMaskRead.coreMaskRet = retval;
    if (retval != PECI_CC_SUCCESS || PECI_CC_UA(cc))
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Cannot find compute die coreMask1! ret: (0x%x), cc: "
                        "(0x%x), domainID: (%d)\n",
                        retval, cc, domainID);
        return;
    }
    computeDie->coreMaskRead.coreMaskValid = true;
    if (computeDie->coreMaskRead.coreMaskValid)
    {
        computeDie->coreMask = (uint64_t)coreMask1;
        computeDie->coreMask <<= 32;
        computeDie->coreMask |= (uint64_t)coreMask0;
        computeDie->coreMaskRead.source = cpuState;
    }
}

void getComputeDieCHAMasks(uint8_t clientAddr, DieMaskInfo* dieMaskInfo,
                           uint8_t domainID, ComputeDieInfo* computeDie,
                           cpuidState cpuState)
{
    uint8_t cc = 0;
    uint32_t chaMask0 = 0x0;
    uint32_t chaMask1 = 0x0;

    EPECIStatus retval;

    int device = UpdateGnrPcuDeviceNum(dieMaskInfo,
                                       domainID + dieMaskInfo->compute.offset);
    domainID = domainID + dieMaskInfo->compute.offset;

    retval = peci_RdEndPointConfigPciLocal_dom(
        clientAddr, domainID, 0, 30, device, 0, 0x29c, sizeof(chaMask0),
        (uint8_t*)&chaMask0, &cc);
    computeDie->chaCountRead.chaCountCc = cc;
    computeDie->chaCountRead.chaCountRet = retval;
    if (retval != PECI_CC_SUCCESS || PECI_CC_UA(cc))
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Cannot find compute die chaMask0! ret: (0x%x), cc: "
                        "(0x%x), domainID: (%d)\n",
                        retval, cc, domainID);
        return;
    }
    retval = peci_RdEndPointConfigPciLocal_dom(
        clientAddr, domainID, 0, 30, device, 0, 0x2a0, sizeof(chaMask1),
        (uint8_t*)&chaMask1, &cc);
    computeDie->chaCountRead.chaCountCc = cc;
    computeDie->chaCountRead.chaCountRet = retval;
    if (retval != PECI_CC_SUCCESS || PECI_CC_UA(cc))
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Cannot find compute die chaMask1! ret: (0x%x), cc: "
                        "(0x%x), domainID: (%d)\n",
                        retval, cc, domainID);
        return;
    }
    computeDie->chaCountRead.chaCountValid = true;
    if (computeDie->chaCountRead.chaCountValid)
    {
        computeDie->chaCount =
            __builtin_popcount(chaMask0) + __builtin_popcount(chaMask1);
        computeDie->chaMask = (uint64_t)chaMask1;
        computeDie->chaMask <<= 32;
        computeDie->chaMask |= (uint64_t)chaMask0;
        computeDie->chaCountRead.source = cpuState;
    }
}

bool getCoreMasksGNR(CPUInfo* cpu, cpuidState cpuState)
{
    uint8_t idx = 0;

    for (uint8_t domainID = 0; domainID < cpu->dieMaskInfo.compute.maxDies;
         domainID++)
    {
        if (!CHECK_BIT(cpu->dieMaskInfo.compute.effectiveMask, domainID))
        {
            continue;
        }
        if (cpu->computeDies[idx].coreMaskRead.coreMaskValid)
        {
            continue;
        }
        getComputeDieCoreMasks(cpu->clientAddr, &cpu->dieMaskInfo, domainID,
                               &cpu->computeDies[idx], cpuState);
        idx++;
    }
    return true;
}

static bool getCoreMasksSPR(CPUInfo* cpu, cpuidState cpuState)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;

    uint32_t coreMask0 = 0x0;
    uint32_t coreMask1 = 0x0;
    cpu->coreMask = 0;
    switch (cpu->model)
    {
        case cd_emr:
        case cd_spr:
        case cd_sprhbm:
            // RESOLVED_CORES EP Local PCI B31:D30:F6 Reg 0x80 and 0x84
            retval = peci_RdEndPointConfigPciLocal(cpu->clientAddr, 0, 31, 30,
                                                   6, 0x80, sizeof(coreMask0),
                                                   (uint8_t*)&coreMask0, &cc);
            cpu->coreMaskRead.coreMaskCc = cc;
            cpu->coreMaskRead.coreMaskRet = retval;
            if (retval != PECI_CC_SUCCESS || PECI_CC_UA(cc))
            {
                CRASHDUMP_PRINT(ERR, stderr,
                                "Cannot find coreMask0! ret: (0x%x), cc: "
                                "(0x%x) addr: (%d)\n",
                                retval, cc, cpu->clientAddr);
                break;
            }
            retval = peci_RdEndPointConfigPciLocal(cpu->clientAddr, 0, 31, 30,
                                                   6, 0x84, sizeof(coreMask1),
                                                   (uint8_t*)&coreMask1, &cc);
            cpu->coreMaskRead.coreMaskCc = cc;
            cpu->coreMaskRead.coreMaskRet = retval;
            if (retval != PECI_CC_SUCCESS || PECI_CC_UA(cc))
            {
                CRASHDUMP_PRINT(ERR, stderr,
                                "Cannot find coreMask1! ret: (0x%x), cc: "
                                "(0x%x) addr: (%d)\n",
                                retval, cc, cpu->clientAddr);
                break;
            }
            cpu->coreMaskRead.coreMaskValid = true;
            break;
        default:
            return false;
    }
    if (cpu->coreMaskRead.coreMaskValid)
    {
        cpu->coreMask = (uint64_t)coreMask1;
        cpu->coreMask <<= 32;
        cpu->coreMask |= (uint64_t)coreMask0;
        cpu->coreMaskRead.source = cpuState;
    }
    return true;
}

static bool getCHACountsGNR(CPUInfo* cpu, cpuidState cpuState)
{
    uint8_t idx = 0;
    for (uint8_t domainID = 0; domainID < cpu->dieMaskInfo.compute.maxDies;
         domainID++)
    {
        if (!CHECK_BIT(cpu->dieMaskInfo.compute.effectiveMask, domainID))
        {
            continue;
        }
        if (cpu->computeDies[idx].chaCountRead.chaCountValid)
        {
            continue;
        }
        getComputeDieCHAMasks(cpu->clientAddr, &cpu->dieMaskInfo, domainID,
                              &cpu->computeDies[idx], cpuState);
        idx++;
    }

    for (int i = 0; i < idx; i++)
    {
        cpu->chaCount += cpu->computeDies[i].chaCount;
    }

    return true;
}

static bool getCHACountsSPR(CPUInfo* cpu, cpuidState cpuState)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;

    uint32_t chaMask0 = 0x0;
    uint32_t chaMask1 = 0x0;
    cpu->chaCount = 0;
    switch (cpu->model)
    {
        case cd_emr:
        case cd_spr:
        case cd_sprhbm:
            // LLC_SLICE_EN EP Local PCI B31:D30:F3 Reg 0x9C and 0xA0
            retval = peci_RdEndPointConfigPciLocal(cpu->clientAddr, 0, 31, 30,
                                                   3, 0x9c, sizeof(chaMask0),
                                                   (uint8_t*)&chaMask0, &cc);
            cpu->chaCountRead.chaCountCc = cc;
            cpu->chaCountRead.chaCountRet = retval;
            if (retval != PECI_CC_SUCCESS || PECI_CC_UA(cc))
            {
                CRASHDUMP_PRINT(ERR, stderr,
                                "Cannot find chaMask0! ret: (0x%x),  cc: "
                                "(0x%x) addr: (%d)\n",
                                retval, cc, cpu->clientAddr);
                break;
            }
            retval = peci_RdEndPointConfigPciLocal(cpu->clientAddr, 0, 31, 30,
                                                   3, 0xa0, sizeof(chaMask1),
                                                   (uint8_t*)&chaMask1, &cc);
            cpu->chaCountRead.chaCountCc = cc;
            cpu->chaCountRead.chaCountRet = retval;
            if (retval != PECI_CC_SUCCESS || PECI_CC_UA(cc))
            {
                CRASHDUMP_PRINT(ERR, stderr,
                                "Cannot find chaMask1! ret: (0x%x), cc: (0x%x) "
                                "addr: (%d)\n",
                                retval, cc, cpu->clientAddr);
                break;
            }
            cpu->chaCountRead.chaCountValid = true;
            break;
        default:
            return false;
    }
    if (cpu->chaCountRead.chaCountValid)
    {
        cpu->chaCount =
            __builtin_popcount(chaMask0) + __builtin_popcount(chaMask1);
        cpu->chaCountRead.source = cpuState;
    }

    return true;
}

bool getCoreMasks(CPUInfo* cpus, cpuidState cpuState)
{
    bool ret = false;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        switch (cpu->model)
        {
            case cd_gnr:
            case cd_gnrd:
                cpu->coreType = ACD_BIG_CORE;
                if (cpu->computeDies == NULL)
                {
                    ret = false;
                    CRASHDUMP_PRINT(ERR, stderr, "Cannot find computeDies!\n");
                    break;
                }
                ret = getCoreMasksGNR(cpu, cpuState);
                break;
            case cd_srf:
                cpu->coreType = ACD_ATOM_CORE;
                if (cpu->computeDies == NULL)
                {
                    ret = false;
                    CRASHDUMP_PRINT(ERR, stderr, "Cannot find computeDies!\n");
                    break;
                }
                ret = getCoreMasksGNR(cpu, cpuState);
                break;
            default:
                if (cpu->coreMaskRead.coreMaskValid)
                {
                    continue;
                }
                ret = getCoreMasksSPR(cpu, cpuState);
        }
    }
    return ret;
}

bool getCHACounts(CPUInfo* cpus, cpuidState cpuState)
{
    bool ret = false;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        switch (cpu->model)
        {
            case cd_gnr:
            case cd_gnrd:
            case cd_srf:
                ret = getCHACountsGNR(cpu, cpuState);
                break;
            default:
                if (cpu->chaCountRead.chaCountValid)
                {
                    continue;
                }
                ret = getCHACountsSPR(cpu, cpuState);
        }
    }
    return ret;
}

void getPlatformIDs(CPUInfo* cpus)
{
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        uint32_t val = 0;
        uint8_t cc = 0;

        // CHECK GNR EDS Volume 1 PCS Service table for details
        uint8_t ret = peci_RdPkgConfig(cpu->clientAddr, 0, 1, sizeof(val),
                                       (uint8_t*)&val, &cc);

        if (ret != PECI_CC_SUCCESS || PECI_CC_UA(cc))
        {
            CRASHDUMP_PRINT(
                ERR, stderr,
                "Cannot find PlatformID! ret: (0x%x), cc: (0x%x) addr: "
                "(%d)\n",
                ret, cc, cpu->clientAddr);
            continue;
        }
        else
        {
            CRASHDUMP_PRINT(INFO, stderr,
                            "Stored PlatformID! val: (0x%x), addr: (%d)\n", val,
                            cpu->clientAddr);
        }
        cpu->platformID = val;
    }
}

void getUcodePatchVers(CPUInfo* cpus)
{
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        uint32_t val = 0;
        uint8_t cc = 0;

        // CHECK GNR EDS Volume 1 PCS Service table for details
        uint8_t ret = peci_RdPkgConfig(cpu->clientAddr, 0, 4, sizeof(val),
                                       (uint8_t*)&val, &cc);

        if (ret != PECI_CC_SUCCESS || PECI_CC_UA(cc))
        {
            CRASHDUMP_PRINT(
                ERR, stderr,
                "Cannot find ucode patch! ret: (0x%x), cc: (0x%x) addr: "
                "(%d)\n",
                ret, cc, cpu->clientAddr);
            continue;
        }
        else
        {
            CRASHDUMP_PRINT(
                INFO, stderr,
                "Stored ucode patch version! val: (0x%x), addr: (%d)\n", val,
                cpu->clientAddr);
        }
        cpu->ucodePatch = val;
    }
}

void cleanupComputeDies(CPUInfo* cpus)
{
    for (int i = 0; i < MAX_CPUS; i++)
    {
        if (cpus[i].computeDies != NULL)
        {
            CRASHDUMP_PRINT(INFO, stderr,
                            "Cleaning crashdump cpu%d computeDies\n", i);
            FREE(cpus[i].computeDies);
        }
    }
}

uint8_t UpdateGnrPcuDeviceNum(DieMaskInfo* dieMaskInfo, uint8_t pos)
{
    /* PCU Device 9-5 selection logic */
    /*
     if bus == 30 and func == 0 and (5 <= dev <= 9):
        if io0:
            dev = 5
        if compute0:
            dev = 6
        if compute1:
            dev = 7
        if compute2:
            dev = 8
        if io1:
            dev = 9
    */

    uint8_t dev = 5;

    if (CHECK_BIT(dieMaskInfo->io.range, pos))
    {
        for (uint8_t die = 0; die < dieMaskInfo->io.maxDies; die++)
        {
            if (CHECK_BIT(dieMaskInfo->io.effectiveMask, die))
            {
                if (die != pos)
                {
                    dev = 9;
                }
                break;
            }
        }
    }
    if (CHECK_BIT(dieMaskInfo->compute.range, pos))
    {
        int dieCount = 0;
        for (uint8_t die = 0; die < dieMaskInfo->compute.maxDies; die++)
        {
            if (CHECK_BIT(dieMaskInfo->compute.effectiveMask, die))
            {
                dieCount++;
                if ((dieCount == 1) &&
                    (pos == die + dieMaskInfo->compute.offset))
                {
                    dev = 6;
                    break;
                }
                else if ((dieCount == 2) &&
                         (pos == die + dieMaskInfo->compute.offset))
                {
                    dev = 7;
                    break;
                }
                else if ((dieCount == 3) &&
                         (pos == die + dieMaskInfo->compute.offset))
                {
                    dev = 8;
                    break;
                }
            }
        }
    }
    return dev;
}
// Max length of the input die name and appending digits. An example is
// "Compute10" which is a total of 10 chars
#define MAX_INPUT_LEN 10
// Max length of the input die name. An longest example is "Compute" which is a
// total 8 chars
#define MAX_DOMAIN_LEN 8
/******************************************************************************
 *
 *  parseExplicitDomain
 *
 *  Generalized function to parse both "io" and "compute" strs form Input file
 *  char* domainName - which domain type to be parsed. Ex. "io"
 *  char* inputString - full string from input file. Ex "io0"
 *  uint32_t* valuePtr - ptr to value to return parsed value
 ******************************************************************************/
acdStatus parseExplicitDomain(char* domainName, char* inputString,
                              uint32_t* valuePtr)
{
    char* valueSubString;
    uint32_t domainLen = strnlen_s(domainName, MAX_DOMAIN_LEN);
    uint32_t inputLen = strnlen_s(inputString, MAX_INPUT_LEN);

    if (domainName == NULL || domainLen == 0)
    {
        return ACD_INVALID_OBJECT;
    }
    else if (inputString == NULL || inputLen == 0)
    {
        return ACD_FAILURE;
    }

    // We're going to use lowercase
    strtolowercase_s(domainName, domainLen);
    strtolowercase_s(inputString, inputLen);

    /*
        domainName parameter must exist in inputString parameter otherwise this
        call is fundamentally wrong Ex: "io" must be in "io7"
    */
    if (strstr_s(inputString, inputLen, domainName, domainLen,
                 &valueSubString) != EOK)
    {
        return ACD_FAILURE;
    }

    // Skip domainName and convert to UInt
    *valuePtr = strtoul(&(inputString[domainLen]), NULL, 10);
    return ACD_SUCCESS;
}

/*  Mapping die numeration to bit is not straightforward on GNR
    Ex: io0 is not bit 0 in the io range (0x1FF); but rather can be ANY bit(0-8)
   in the io range. The enumeration value will be determined counting from the
   set bits starting from bit0. Ex: with dieMask of b0 1000 1000, io0 is bit3
   and io1 is bit 7. IO/COMPUTE die is determined by rangeMask.
*/

uint8_t mapDieNumToBit(uint32_t dieMask, uint32_t posToMap, uint32_t rangeMask)
{
    uint32_t maxDieRange = __builtin_popcount(rangeMask);
    uint32_t normalizedDieMask = 0;
    uint32_t dieCount = 0;
    uint32_t normalizedShiftCount = 0;

    if (__builtin_popcount(dieMask) == 0)
    {
        return DIE_MAP_ERROR_VALUE;
    }
    if (maxDieRange == 0)
    {
        return DIE_MAP_ERROR_VALUE;
    }
    if (posToMap >= maxDieRange)
    {
        return DIE_MAP_ERROR_VALUE;
    }

    // Apply the range
    normalizedDieMask = dieMask & rangeMask;

    // Normalize; remove trailing zeros Ex: 018800 -> 0x0188
    while (!(normalizedDieMask & 1))
    {
        // Use this later
        normalizedShiftCount++;
        normalizedDieMask = normalizedDieMask >> 1;
    }

    // Loop through range
    for (uint8_t bitPos = 0; bitPos < maxDieRange; bitPos++)
    {
        // Count our way to posToMap
        if (CHECK_BIT(normalizedDieMask, bitPos))
        {
            // Finished when we find the bit position
            if (dieCount == posToMap)
            {
                // Calc the bitPosition of the full dieMask
                return bitPos + normalizedShiftCount;
            }
            dieCount++;
        }
    }

    // Position doesn't exist!
    return DIE_MAP_ERROR_VALUE;
}

bool isDomainCompute(DieMaskInfo* dieMaskInfo, uint32_t domainID)
{
    return (dieMaskInfo->compute.range & (1 << domainID));
}

uint8_t getClientAddrs(CPUInfo* cpus)
{
    char statusString[256] = {'\0'};
    int index = 0;

    uint8_t validAddrs = 0;
    int cpu, addr;
    for (cpu = 0, addr = MIN_CLIENT_ADDR; addr <= MAX_CLIENT_ADDR;
         cpu++, addr++)
    {
        EPECIStatus status;
        uint8_t cc = 0;
        uint32_t val = 0;
        int charsWritten = 0;

        status =
            peci_RdPkgConfig(addr, 0, 0, sizeof(uint32_t), (uint8_t*)&val, &cc);
        if (status == PECI_CC_SUCCESS)
        {
            validAddrs++;
            cpus[cpu].clientAddr = addr;
        }

        if (addr == MAX_CLIENT_ADDR)
        {
            charsWritten = cd_snprintf_s(statusString + index,
                                         sizeof(statusString) - index,
                                         "0x%x:0x%x", cc, status);
        }
        else
        {
            charsWritten = cd_snprintf_s(statusString + index,
                                         sizeof(statusString) - index,
                                         "0x%x:0x%x, ", cc, status);
        }

        index += charsWritten;
    }

    if (validAddrs < 1)
    {
        CRASHDUMP_PRINT(ERR, stderr, "\nPing to CPUs (CC:RC) [%s]\n",
                        statusString);
    }
    return validAddrs;
}

pwState savePeciWakeOfDomain(int clientAddr, uint32_t domainId)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;
    uint32_t peciRdValue = 0;

    retval =
        peci_RdPkgConfig_dom(clientAddr, domainId, vcuPeciWake, DONT_CARE,
                             sizeof(uint32_t), (uint8_t*)&peciRdValue, &cc);
    if (retval != PECI_CC_SUCCESS)
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "Cannot read Wake_on_PECI-> addr: (0x%x), domainId(%d), ret: (0x%x), \
                    cc: (0x%x)\n",
            clientAddr, domainId, retval, cc);
        return UNKNOWN;
    }
    return (peciRdValue & 0x3);
}

int savePeciWake(CPUInfo* cpus)
{
    int i;
    for (i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }

        if (!cpu->dieMaskInfo.dieMaskSupported)
        {
            cpu->initialPeciWake = savePeciWakeOfDomain(cpu->clientAddr, 0);
            continue;
        }

        int logicComputeNum = 0;
        uint16_t die;
        for (die = 0; die < cpu->dieMaskInfo.compute.maxDies; die++)
        {
            if (!CHECK_BIT(cpu->dieMaskInfo.compute.effectiveMask, die))
            {
                continue;
            }
            uint16_t computeDomainID = die + cpu->dieMaskInfo.compute.offset;
            cpu->dieMaskInfo.compute.initialPeciWake[logicComputeNum] =
                savePeciWakeOfDomain(cpu->clientAddr, computeDomainID);
            logicComputeNum++;
        }

        int logicIoNum = 0;
        for (die = 0; die < cpu->dieMaskInfo.io.maxDies; die++)
        {
            if (!CHECK_BIT(cpu->dieMaskInfo.io.effectiveMask, die))
            {
                continue;
            }
            cpu->dieMaskInfo.io.initialPeciWake[logicIoNum] =
                savePeciWakeOfDomain(cpu->clientAddr, die);
            logicIoNum++;
        }
    }
    return 1;
}

void setPeciWakeOfDomain(int clientAddr, uint32_t domainId, int writeValue)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;

    retval = peci_WrPkgConfig_dom(clientAddr, domainId, vcuPeciWake, writeValue,
                                  &writeValue, sizeof(writeValue), &cc);
    if (retval != PECI_CC_SUCCESS)
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "Cannot set Wake_on_PECI-> addr: (0x%x), domainId: (%d), ret: "
            "(0x%x), cc: (0x%x)\n",
            clientAddr, domainId, retval, cc);
    }
}

void setPeciWakeInitial(CPUInfo* cpus)
{
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        if (!cpu->dieMaskInfo.dieMaskSupported)
        {
            setPeciWakeOfDomain(cpu->clientAddr, 0, cpu->initialPeciWake);
            continue;
        }

        int logicComputeNum = 0;
        uint16_t die;
        for (die = 0; die < cpu->dieMaskInfo.compute.maxDies; die++)
        {
            if (!CHECK_BIT(cpu->dieMaskInfo.compute.effectiveMask, die))
            {
                continue;
            }
            uint16_t computeDomainID = die + cpu->dieMaskInfo.compute.offset;
            setPeciWakeOfDomain(
                cpu->clientAddr, computeDomainID,
                cpu->dieMaskInfo.compute.initialPeciWake[logicComputeNum]);
            logicComputeNum++;
        }

        int logicIoNum = 0;
        for (die = 0; die < cpu->dieMaskInfo.io.maxDies; die++)
        {
            if (!CHECK_BIT(cpu->dieMaskInfo.io.effectiveMask, die))
            {
                continue;
            }
            setPeciWakeOfDomain(
                cpu->clientAddr, die,
                cpu->dieMaskInfo.io.initialPeciWake[logicIoNum]);
            logicIoNum++;
        }
    }
}

int setPeciWake(CPUInfo* cpus, pwState desiredState)
{
    int writeValue = OFF;
    int i;
    for (i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        writeValue = (int)desiredState;
        if (!cpu->dieMaskInfo.dieMaskSupported)
        {
            setPeciWakeOfDomain(cpu->clientAddr, 0, writeValue);
            continue;
        }

        int logicComputeNum = 0;
        uint16_t die;
        for (die = 0; die < cpu->dieMaskInfo.compute.maxDies; die++)
        {
            if (!CHECK_BIT(cpu->dieMaskInfo.compute.effectiveMask, die))
            {
                continue;
            }
            uint16_t computeDomainID = die + cpu->dieMaskInfo.compute.offset;
            setPeciWakeOfDomain(cpu->clientAddr, computeDomainID, writeValue);
            logicComputeNum++;
        }

        int logicIoNum = 0;
        for (die = 0; die < cpu->dieMaskInfo.io.maxDies; die++)
        {
            if (!CHECK_BIT(cpu->dieMaskInfo.io.effectiveMask, die))
            {
                continue;
            }
            setPeciWakeOfDomain(cpu->clientAddr, die, writeValue);
            logicIoNum++;
        }
    }
    return 1;
}

void checkPeciWakeOfDomain(int clientAddr, uint32_t domainID)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;
    uint32_t peciRdValue = 0;
    retval =
        peci_RdPkgConfig_dom(clientAddr, domainID, vcuPeciWake, DONT_CARE,
                             sizeof(uint32_t), (uint8_t*)&peciRdValue, &cc);
    if (retval != PECI_CC_SUCCESS)
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "Cannot read Wake_on_PECI-> addr: (0x%x), die(%d), ret: (0x%x), \
                    cc: (0x%x)\n",
            clientAddr, domainID, retval, cc);
    }
    if (peciRdValue == OFF)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Wake_on_PECI in OFF state: (0x%x) die(%d)\n",
                        clientAddr, domainID);
    }
}

int checkPeciWake(CPUInfo* cpus)
{
    int i;
    for (i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }

        if (!cpu->dieMaskInfo.dieMaskSupported)
        {
            checkPeciWakeOfDomain(cpu->clientAddr, 0);
            continue;
        }

        int logicComputeNum = 0;
        uint16_t die;
        for (die = 0; die < cpu->dieMaskInfo.compute.maxDies; die++)
        {
            if (!CHECK_BIT(cpu->dieMaskInfo.compute.effectiveMask, die))
            {
                continue;
            }
            uint16_t computeDomainID = die + cpu->dieMaskInfo.compute.offset;
            checkPeciWakeOfDomain(cpu->clientAddr, computeDomainID);
            logicComputeNum++;
        }

        int logicIoNum = 0;
        for (die = 0; die < cpu->dieMaskInfo.io.maxDies; die++)
        {
            if (!CHECK_BIT(cpu->dieMaskInfo.io.effectiveMask, die))
            {
                continue;
            }
            checkPeciWakeOfDomain(cpu->clientAddr, die);
            logicIoNum++;
        }
    }
    return 1;
}

acdStatus loadInputFiles(CPUInfo* cpus, InputFileInfo* inputFileInfo,
                         int isTelemetry)
{
    int uniqueCount = 0;
    cJSON* defaultStateSection = NULL;
    acdStatus status = ACD_SUCCESS;

    int i;
    for (i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if ((cpu->clientAddr == 0) || (cpu->model >= cd_numberOfModels))
        {
            continue;
        }
        // read and allocate memory for crashdump input file
        // if it hasn't been read before
        if (inputFileInfo->buffers[cpu->model] == NULL)
        {
            inputFileInfo->buffers[cpu->model] = selectAndReadInputFile(
                cpu->model, &inputFileInfo->filenames[cpu->model], isTelemetry,
                inputFileInfo->defaultPath, inputFileInfo->overridePath);
            if (inputFileInfo->buffers[cpu->model] != NULL)
            {
                uniqueCount++;
            }
        }

        inputFileInfo->unique = (uniqueCount <= 1);
        cpu->inputFile.filenamePtr = inputFileInfo->filenames[cpu->model];
        cpu->inputFile.bufferPtr = inputFileInfo->buffers[cpu->model];

        // Get and Check global enable/disable value from "DefaultState"
        defaultStateSection = cJSON_GetObjectItemCaseSensitive(
            inputFileInfo->buffers[cpu->model], "DefaultState");
        int defaultStateEnable = 1;
        if (defaultStateSection != NULL)
        {
            strcmp_s(defaultStateSection->valuestring, CRASHDUMP_VALUE_LEN,
                     "Enable", &defaultStateEnable);
            if (defaultStateEnable == 0)
            {
                status = ACD_SUCCESS;
            }
            else
            {
                defaultStateEnable = 1;
                strcmp_s(defaultStateSection->valuestring, CRASHDUMP_VALUE_LEN,
                         "Disable", &defaultStateEnable);
                if (defaultStateEnable == 0)
                {
                    status = ACD_INPUT_FILE_ERROR;
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Exiting, \"DefaultState\" set to Disable in: %s\n",
                        inputFileInfo->filenames[cpu->model]);
                }
                else
                {
                    status = ACD_SUCCESS;
                    CRASHDUMP_PRINT(ERR, stderr,
                                    "\"DefaultState\" (%s) is neither "
                                    "Enable/Disable in: %s\n",
                                    defaultStateSection->valuestring,
                                    inputFileInfo->filenames[cpu->model]);
                }
            }
        }
        else
        {
            status = ACD_INPUT_FILE_ERROR;
        }
    }

    return status;
}

void getCPUID(CPUInfo* cpuInfo)
{
    uint8_t cc = 0;
    CPUModel cpuModel = {0};
    uint8_t stepping = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;

    retval = peci_GetCPUID(cpuInfo->clientAddr, &cpuModel, &stepping, &cc);
    cpuInfo->cpuidRead.cpuModel = cpuModel;
    cpuInfo->cpuidRead.stepping = stepping;
    cpuInfo->cpuidRead.cpuidCc = cc;
    cpuInfo->cpuidRead.cpuidRet = retval;
    if (retval != PECI_CC_SUCCESS || PECI_CC_UA(cc))
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "Cannot get CPUID! ret: (0x%x), cc: (0x%x) on PECI address %d\n",
            retval, cc, cpuInfo->clientAddr);
    }
}

void parseCPUInfo(CPUInfo* cpu, cpuidState cpuState)
{
    switch ((int)cpu->cpuidRead.cpuModel)
    {
        case EMR_MODEL:
            CRASHDUMP_PRINT(INFO, stderr,
                            "EMR detected (CPUID 0x%x) on PECI address %d\n",
                            cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                            cpu->clientAddr);
            cpu->model = cd_emr;
            cpu->cpuidRead.cpuidValid = 1;
            cpu->cpuidRead.source = cpuState;
            break;
        case SPR_MODEL:
            if (isSprHbm(cpu))
            {
                CRASHDUMP_PRINT(
                    INFO, stderr,
                    "SPR-HBM detected (CPUID 0x%x) on PECI address %d\n",
                    cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                    cpu->clientAddr);
                cpu->model = cd_sprhbm;
            }
            else
            {
                CRASHDUMP_PRINT(
                    INFO, stderr,
                    "SPR detected (CPUID 0x%x) on PECI address %d\n",
                    cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                    cpu->clientAddr);
                cpu->model = cd_spr;
            }
            cpu->cpuidRead.cpuidValid = 1;
            cpu->cpuidRead.source = cpuState;
            break;
        case GNR_MODEL:
            CRASHDUMP_PRINT(INFO, stderr,
                            "GNR detected (CPUID 0x%x) on PECI address %d\n",
                            cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                            cpu->clientAddr);
            cpu->model = cd_gnr;
            cpu->cpuidRead.cpuidValid = 1;
            cpu->cpuidRead.source = cpuState;
            break;
        case GNRD_MODEL:
            CRASHDUMP_PRINT(INFO, stderr,
                            "GNR-D detected (CPUID 0x%x) on PECI address %d\n",
                            cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                            cpu->clientAddr);
            cpu->model = cd_gnrd;
            cpu->cpuidRead.cpuidValid = 1;
            cpu->cpuidRead.source = cpuState;
            break;
        case SRF_MODEL:
            CRASHDUMP_PRINT(INFO, stderr,
                            "SRF detected (CPUID 0x%x) on PECI address %d\n",
                            cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                            cpu->clientAddr);
            cpu->model = cd_srf;
            cpu->cpuidRead.cpuidValid = 1;
            cpu->cpuidRead.source = cpuState;
            break;
        default:
            CRASHDUMP_PRINT(ERR, stderr,
                            "Unsupported CPUID 0x%x on PECI address %d\n",
                            cpu->cpuidRead.cpuModel | cpu->cpuidRead.stepping,
                            cpu->clientAddr);
            break;
    }
}

void overwriteCPUInfo(CPUInfo* cpus)
{
    int found = 0;
    Model defaultModel = (Model)0x0;
    CPUModel defaultCpuModel = (CPUModel)0x0;
    uint8_t defaultStepping = 0x0;
    int i;
    for (i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr != 0x0 && cpu->cpuidRead.cpuidValid)
        {
            defaultModel = cpu->model;
            defaultCpuModel = cpu->cpuidRead.cpuModel;
            defaultStepping = cpu->cpuidRead.stepping;
            found = 1;
            break;
        }
    }

    if (!found)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "No valid CPUID found, using gnr as default model\n");
        defaultModel = cd_gnr;
        defaultCpuModel = (CPUModel)GNR_MODEL;
        defaultStepping = 0;
        found = 1;
    }

    for (i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr != 0x0 && !cpu->cpuidRead.cpuidValid)
        {
            if (found)
            {
                CRASHDUMP_PRINT(
                    INFO, stderr,
                    "Overwriting model for CPU: %d on PECI address %d\n", i,
                    cpu->clientAddr);
                cpu->model = defaultModel;
                cpu->cpuidRead.cpuModel = defaultCpuModel;
                cpu->cpuidRead.stepping = defaultStepping;
                cpu->cpuidRead.source = OVERWRITTEN;
                cpu->cpuidRead.cpuidCc = PECI_DEV_CC_SUCCESS;
                cpu->cpuidRead.cpuidRet = PECI_CC_SUCCESS;
                cpu->cpuidRead.cpuidValid = 1;
            }
        }
    }
}

int isCpusInfoEmpty(CPUInfo* cpus)
{
    int i;
    for (i = 0; i < MAX_CPUS; i++)
    {
        if (cpus[i].clientAddr != 0x0)
        {
            return 0;
        }
    }
    return 1;
}

void initMaxCPUs(CPUInfo* cpus)
{
    for (int i = 0; i < MAX_CPUS; i++)
    {
        cpus[i].coreMaskRead.coreMaskValid = 0;
        cpus[i].chaCountRead.chaCountValid = 0;
        cpus[i].cpuidRead.cpuidValid = 0;
        cpus[i].coreMaskRead.source = INVALID;
        cpus[i].chaCountRead.source = INVALID;
        cpus[i].cpuidRead.source = INVALID;
        cpus[i].model = (Model)0x0;
        cpus[i].cpuidRead.cpuModel = (CPUModel)0x0;
        cpus[i].platformState.resetDetected = false;
        cpus[i].platformState.resetCpu = DEFAULT_VALUE;
        errno_t err =
            strcpy_s(cpus[i].platformState.resetSectionName,
                     sizeof(cpus[i].platformState.resetSectionName), MD_NA);
        if (err != EOK)
        {
            CRASHDUMP_PRINT(ERR, stderr, "Error copying string: %s\n", MD_NA);
        }
        // Do not reset DieMask maskValid and dieMask
        cpus[i].dieMaskInfo.dieMaskSupported = 0;

        memset_s(&cpus[i].dieMaskInfo.compute,
                 sizeof(cpus[i].dieMaskInfo.compute), 0,
                 sizeof(cpus[i].dieMaskInfo.compute));
        memset_s(&cpus[i].dieMaskInfo.io, sizeof(cpus[i].dieMaskInfo.io), 0,
                 sizeof(cpus[i].dieMaskInfo.io));

        // Core phy2log init
        for (int die = 0; die < GNR_MAX_ACTIVE_DIES; die++)
        {
            memset_s(cpus[i].phy2logReadCore.table[die],
                     sizeof(cpus[i].phy2logReadCore.table[die]), 0,
                     sizeof(cpus[i].phy2logReadCore.table[die]));
        }
        cpus[i].phy2logReadCore.isValid = false;

        // Cha phy2log init
        for (int die = 0; die < GNR_MAX_ACTIVE_DIES; die++)
        {
            memset_s(cpus[i].phy2logReadCHA.table[die],
                     sizeof(cpus[i].phy2logReadCHA.table[die]), 0,
                     sizeof(cpus[i].phy2logReadCHA.table[die]));
        }
        cpus[i].phy2logReadCHA.isValid = false;
    }
}

acdStatus initCPUInfo(CPUInfo* cpus)
{
    if (isCpusInfoEmpty(cpus))
    {
        uint8_t validAddrs = getClientAddrs(cpus);
        // check PING got at least one valid addr
        if (!(validAddrs > 0))
        {
            return ACD_PING_ERROR;
        }
        initMaxCPUs(cpus);
        return ACD_SUCCESS;
    }
    return ACD_FAILURE;
}

acdStatus getCPUData(CPUInfo* cpus, cpuidState cpuState)
{
    if (isCpusInfoEmpty(cpus))
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "cpuInfo is empty, no PECI addresses are available\n");
        return ACD_FAILURE;
    }
    int i;
    for (i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];
        if (cpu->clientAddr == 0)
        {
            continue;
        }
        if (!cpu->cpuidRead.cpuidValid ||
            (cpu->cpuidRead.source == OVERWRITTEN))
        {
            getCPUID(cpu);
            parseCPUInfo(cpu, cpuState);
        }
    }
    if (cpuState == EVENT)
    {
        overwriteCPUInfo(cpus);
    }

    if (cpuState == STARTUP)
    {
        getPlatformIDs(cpus);
        getUcodePatchVers(cpus);
    }
    getDieMasks(cpus, cpuState);
    getCoreMasks(cpus, cpuState);
    getCHACounts(cpus, cpuState);
    return ACD_SUCCESS;
}

void newTimestamp(char* logTime, size_t logTimeSize)
{
    time_t curtime;
    struct tm* loctime;

    // Add the timestamp
    curtime = time(NULL);
    loctime = localtime(&curtime);
    if (NULL != loctime)
    {
        strftime(logTime, logTimeSize, "%FT%TZ", loctime);
    }
}

void logInputFileVersion(CPUInfo* cpuInfo, PlatformInfo* platformInfo)
{
    cJSON* jsonVer = cJSON_GetObjectItemCaseSensitive(
        cpuInfo->inputFile.bufferPtr, "Version");
    if ((jsonVer != NULL) && cJSON_IsString(jsonVer))
    {
        platformInfo->inputfile_ver = jsonVer->valuestring;
    }
    else
    {
        platformInfo->inputfile_ver = MD_NA;
    }
}

void cleanupInputFiles(CPUInfo* cpus, InputFileInfo* inputFileInfo)
{
    for (int i = 0; i < cd_numberOfModels; i++)
    {
        if (inputFileInfo->filenames[i] != NULL)
        {
            free(inputFileInfo->filenames[i]);
            inputFileInfo->filenames[i] = NULL;
        }
        if (inputFileInfo->buffers[i] != NULL)
        {
            cJSON_Delete(inputFileInfo->buffers[i]);
            inputFileInfo->buffers[i] = NULL;
        }
    }

    // Clean up dangling pointers
    for (int i = 0; i < MAX_CPUS; i++)
    {
        if (cpus[i].inputFile.filenamePtr != NULL)
        {
            cpus[i].inputFile.filenamePtr = NULL;
        }
        if (cpus[i].inputFile.bufferPtr != NULL)
        {
            cpus[i].inputFile.bufferPtr = NULL;
        }
    }
}

char* printCJSON(cJSON* root)
{
    char* out = NULL;
#ifdef CRASHDUMP_PRINT_UNFORMATTED
    out = cJSON_PrintUnformatted(root);
#else
    out = cJSON_Print(root);
#endif
    if (out == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr, "cJSON_Print Failed\n");
    }
    return out;
}

acdStatus createCrashdump(CPUInfo* cpus, char** crashdumpContents,
                          char* triggerType, char* timestamp, bool isTelemetry,
                          int* sectionFailCount, InputFileInfo* inputFileInfo,
                          cJSON** root)
{
    cJSON* crashlogData = NULL;
    cJSON* metaData = NULL;
    acdStatus status;
    RunTimeInfo runTimeInfo;
    runTimeInfo.maxGlobalTime = 0xFFFFFFFF;
    char platformName[MAX_PLATFORM_NAME_LEN] = {0};
    getUuid(platformName);
    PlatformInfo platformInfo = {.bmc_fw_ver = NULL,
                                 .bios_id = NULL,
                                 .time_stamp = NULL,
                                 .trigger_type = NULL,
                                 .platform_name = NULL,
                                 .crashdump_ver = NULL,
                                 .inputfile_ver = NULL,
                                 .input_file = NULL,
                                 .loopOverOneCpu = false,
                                 .loopOverOneDie = false,
                                 .loopOverOneIO = false,
                                 .noDomainLoop = false};

    // Clear any resets that happened before the crashdump collection started
    initConsoleLog();
    clearResetDetected(&cpus[0].platformState);

    // start the JSON tree for CPU dump
    *root = cJSON_CreateObject();

    // Build the CPU Crashdump JSON file
    // Everything is logged under a "crash_data" section
    cJSON_AddItemToObject(*root, "crash_data",
                          crashlogData = cJSON_CreateObject());

    // Console log
    cJSON_AddItemToObject(*root, CONSOLE_KEY, console.rootObject);

    CRASHDUMP_PRINT(INFO, stderr, "Crashdump started...\n");
    acdStatus initStatus = initCPUInfo(cpus);
    // add exit for ping failure case
    if (initStatus == ACD_PING_ERROR)
    {
        *sectionFailCount = 1;
        CRASHDUMP_PRINT(
            ERR, stderr,
            "Completed! - Error PING-ing cpus failed, dump aborted\n");

        return ACD_PING_ERROR;
    }

    getCPUData(cpus, EVENT);
    // This is mainly here for non-obmc calls as startup waits for valid phy2log
    // There's still a small chance to get valid data if triggered before IERR
    tpmi_getPHY2LOG(cpus, EVENT, CORE_TYPE);
    tpmi_getPHY2LOG(cpus, EVENT, CHA_TYPE);
    savePeciWake(cpus);
    setPeciWake(cpus, ON_PC2);

    acdStatus inputFileLoadError =
        loadInputFiles(cpus, inputFileInfo, isTelemetry);

    if (inputFileLoadError == ACD_INPUT_FILE_ERROR)
    {
        *sectionFailCount = 1;
        CRASHDUMP_PRINT(ERR, stderr,
                        "Completed! - Error loading file can't, dump failed\n");
        *crashdumpContents = printCJSON(*root);
        checkPeciWake(cpus);
        setPeciWake(cpus, OFF);
        return ACD_INPUT_FILE_ERROR;
    }

    if (inputFileLoadError != ACD_SUCCESS)
    {
        cJSON_AddStringToObject(metaData, "_input_file_error",
                                INPUT_FILE_ERROR_STR);
    }

    // Fill in the Crashdump data in the correct order (uncore to core) for
    // each CPU
    cJSON* maxCrashdumpTime = cJSON_GetObjectItemCaseSensitive(
        cpus[0].inputFile.bufferPtr, "MaxCpuTimeInSec");
    if (maxCrashdumpTime != NULL)
    {
        uint32_t numOfCPUs = (uint32_t)getNumberOfCpus(cpus);
        runTimeInfo.maxGlobalTime =
            numOfCPUs * (uint32_t)maxCrashdumpTime->valueint;
    }

    clock_gettime(CLOCK_MONOTONIC, &runTimeInfo.sectionRunTime);
    clock_gettime(CLOCK_MONOTONIC, &crashdumpStart);

    runTimeInfo.globalRunTime = crashdumpStart;

    char biosVersion[SI_BMC_VER_LEN] = {0};
    char bmcVersion[SI_BMC_VER_LEN] = {0};
    fillMetaDataCommon(&platformInfo, &cpus[0], platformName, triggerType,
                       timestamp, biosVersion, bmcVersion);

    uint8_t numberOfSections = getNumberOfSections(&cpus[0]);
    *sectionFailCount = 0;
    for (uint8_t sectionCount = 0; sectionCount < numberOfSections;
         sectionCount++)
    {
        // Note: section index remains 0 because one item in "Sections" array
        // will get deleted at the end of the loop.
        int section = 0;
        platformInfo.loopOverOneCpu = false;

        /* Check available memory on system if theres a limit */
        bool skipSection = checkAvailableMemory(cpus, root, section);

        // Only run if memory isn't low
        if (!skipSection)
        {
            for (uint8_t cpuNum = 0; cpuNum < MAX_CPUS; cpuNum++)
            {
                updateResetInfo(
                    &cpus[0].platformState, (int)cpuNum,
                    getSectionName(cpus[0].inputFile.bufferPtr, section));
                if (cpus[cpuNum].clientAddr == 0)
                {
                    continue;
                }
                platformInfo.loopOverOneDie = true;
                platformInfo.loopOverOneIO = true;
                if (cpus[cpuNum].cpuidRead.cpuidValid)
                {
                    if (cpus[cpuNum].dieMaskInfo.dieMaskSupported)
                    {
                        cJSON* loopOnIoObj = getJsonFlagFromSection(
                            cpus[0].inputFile.bufferPtr, section, "LoopOnIO");
                        cJSON* loopOnComputeObj =
                            getJsonFlagFromSection(cpus[0].inputFile.bufferPtr,
                                                   section, "LoopOnCompute");
                        cJSON* loopOnDomainObj =
                            getJsonFlagFromSection(cpus[0].inputFile.bufferPtr,
                                                   section, "LoopOnDomain");

                        bool loopOnIO = cJSON_IsTrue(loopOnIoObj);
                        bool loopOnDomain = cJSON_IsTrue(loopOnDomainObj);
                        bool loopOnCompute = cJSON_IsTrue(loopOnComputeObj);
                        bool noDomainLoop =
                            !loopOnIO && !loopOnCompute && !loopOnDomain;
                        // Pass this down to fillSection
                        platformInfo.noDomainLoop = noDomainLoop;
                        if (loopOnIO || loopOnDomain)
                        {
                            int dieNum = 0;
                            for (uint8_t die = 0;
                                 die < cpus[cpuNum].dieMaskInfo.io.maxDies;
                                 die++)
                            {
                                if (!CHECK_BIT(
                                        cpus[cpuNum]
                                            .dieMaskInfo.io.effectiveMask,
                                        die) ||
                                    !platformInfo.loopOverOneIO)
                                {
                                    continue;
                                }
                                if (!getFlagFromSection(
                                        cpus[0].inputFile.bufferPtr, section,
                                        "LoopOnIO"))
                                {
                                    platformInfo.loopOverOneIO = false;
                                }
                                status = fillNewSection(
                                    *root, &platformInfo, &cpus[cpuNum], cpuNum,
                                    die, dieNum, &runTimeInfo, section,
                                    triggerType);
                                if (status == ACD_FAILURE)
                                {
                                    (*sectionFailCount)++;
                                }
                                dieNum++;
                            }
                        }
                        // Note: Separate if condition to check LoopOnDomain and
                        // run both IO/compute paths.
                        if (loopOnCompute || loopOnDomain)
                        {
                            int dieNum = 0;
                            for (uint8_t die = 0;
                                 die < cpus[cpuNum].dieMaskInfo.compute.maxDies;
                                 die++)
                            {
                                if (!CHECK_BIT(
                                        cpus[cpuNum]
                                            .dieMaskInfo.compute.effectiveMask,
                                        die) ||
                                    !platformInfo.loopOverOneDie)
                                {
                                    continue;
                                }
                                if (!getFlagFromSection(
                                        cpus[0].inputFile.bufferPtr, section,
                                        "LoopOnCompute"))
                                {
                                    platformInfo.loopOverOneDie = false;
                                }
                                // Compute die starts at bit8 so increment die
                                status = fillNewSection(
                                    *root, &platformInfo, &cpus[cpuNum], cpuNum,
                                    die +
                                        cpus[cpuNum].dieMaskInfo.compute.offset,
                                    dieNum, &runTimeInfo, section, triggerType);
                                dieNum++;
                                if (status == ACD_FAILURE)
                                {
                                    (*sectionFailCount)++;
                                }
                            }
                        }
                        else if (noDomainLoop)
                        {
                            status = fillNewSection(
                                *root, &platformInfo, &cpus[cpuNum], cpuNum, 0,
                                0, &runTimeInfo, section, triggerType);
                            if (status == ACD_FAILURE)
                            {
                                (*sectionFailCount)++;
                            }
                        }
                    }
                    else
                    {
                        status = fillNewSection(
                            *root, &platformInfo, &cpus[cpuNum], cpuNum, 0, 0,
                            &runTimeInfo, section, triggerType);
                        if (status == ACD_FAILURE)
                        {
                            (*sectionFailCount)++;
                        }
                    }
                }
            }
        }
        cJSON* sections = cJSON_GetObjectItemCaseSensitive(
            cpus[0].inputFile.bufferPtr, "Sections");
        if (sections != NULL && cJSON_IsArray(sections))
        {
            cJSON_DeleteItemFromArray(sections, 0);
        }
        else
        {
            CRASHDUMP_PRINT(
                ERR, stderr,
                "Delete input file 'Sections' array item failed(%d)\n",
                sectionCount);
        }
    }
    metaData = cJSON_GetObjectItemCaseSensitive(crashlogData, "METADATA");
    logSectionRunTime(metaData, &crashdumpStart, GLOBAL_TIME_KEY);
    logResetDetected(metaData, &cpus[0].platformState);

    // Print to capture completion in consolelog
    CRASHDUMP_PRINT(INFO, stderr, "Collection Complete!\n");
    *crashdumpContents = printCJSON(*root);
    if (*crashdumpContents == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr, "cJSON_Print Failed\n");
    }

    if (cpus[0].platformState.resetDetected)
    {
        clearResetDetected(&cpus[0].platformState);
    }

    // Clear crashedCoreMask every time crashdump is run
    for (size_t i = 0; i < MAX_CPUS; i++)
    {
        if (cpus[i].computeDies != NULL)
        {
            uint8_t computeDieCount =
                __builtin_popcount(cpus[i].dieMaskInfo.compute.effectiveMask);
            for (uint8_t die = 0; die < computeDieCount; die++)
            {
                cpus[i].computeDies[die].crashedCoreMask = 0;
            }
        }
        cpus[i].crashedCoreMask = 0;
    }
    checkPeciWake(cpus);
    setPeciWakeInitial(cpus);
    return ACD_SUCCESS;
}

acdStatus initCPUs(CPUInfo* cpus, CrashdumpContext* context)
{
    /* Notes:
     * This function serves two primary purposes:
     * 1. It initializes the 'cpus' structure, which holds information about the
     * CPUs.
     * 2. It uses PECI commands to discover necessary information about the
     * CPUs. This information is crucial for the function
     * 'createCrashdumpContents()' to execute properly.
     */

    initMaxCPUs(cpus);
    return getCPUData(cpus, STARTUP);
}

acdStatus createCrashdumpContents(CPUInfo* cpus,
                                  CrashdumpContext* crashdumpContext)
{
    return createCrashdump(
        cpus, &crashdumpContext->outputContents, crashdumpContext->triggerType,
        crashdumpContext->timestamp, crashdumpContext->isTelemetry,
        &crashdumpContext->sectionFailCount, &crashdumpContext->inputFileInfo,
        &crashdumpContext->root);
}

acdStatus setPECIAddressAndDeviceName(CPUInfo* cpu, int addr, char* devName)
{
    if (cpu == NULL || devName == NULL)
    {
        return ACD_DEVICE_NAME_ERROR;
    }
    cpu->clientAddr = addr;
    errno_t err = strncpy_s(cpu->devName, MAX_DEV_NAME_LEN, devName,
                            MAX_DEV_NAME_LEN - 1);
    if (err != EOK)
    {
        return ACD_DEVICE_NAME_ERROR;
    }
    return ACD_SUCCESS;
}

acdStatus setInputFileLocation(CPUInfo* cpu, char* filename)
{
    if (cpu == NULL || filename == NULL)
    {
        return ACD_INPUT_FILE_ERROR;
    }

    // Free the previous filename if it exists
    if (cpu->inputFile.filenamePtr != NULL)
    {
        free(cpu->inputFile.filenamePtr);
    }

    rsize_t len = strnlen_s(filename, MAX_FILENAME_LEN);
    if (len == 0 || len == MAX_FILENAME_LEN)
    {
        return ACD_INPUT_FILE_ERROR;
    }

    // Allocate memory for the new filename
    cpu->inputFile.filenamePtr = (char*)malloc(len + 1);
    if (cpu->inputFile.filenamePtr == NULL)
    {
        return ACD_INPUT_FILE_ERROR;
    }

    errno_t err = strcpy_s(cpu->inputFile.filenamePtr, len + 1, filename);
    if (err != EOK)
    {
        return ACD_INPUT_FILE_ERROR;
    }

    return ACD_SUCCESS;
}

acdStatus deinitCPUsAndContext(CPUInfo* cpus, CrashdumpContext* context)
{
    acdStatus status = ACD_SUCCESS;
    if (context->root != NULL)
    {
        CRASHDUMP_PRINT(INFO, stderr, "Cleaning ConsoleLog/cJSON root\n");
        deinitConsoleLog();
        cJSON_Delete(context->root);
    }
    if (context->outputContents != NULL)
    {
        free(context->outputContents);
    }
    cleanupComputeDies(cpus);
    return status;
}

bool skipSectionOnLowMemory(uint32_t sectionMaxMemMB, uint32_t numOfCpus,
                            float expansionRatio)
{
    uint32_t collectionMemoryNeeded = 0;
    uint32_t maxMemoryNeeded = 0;

    // No need to calculate
    if (sectionMaxMemMB == 0)
    {
        return false;
    }

    // Calculate what we need before collection
    AppUsageInfo appUsage = getCurrentAppMemoryUsage();
    // Convert to MB
    appUsage.memoryUsage /= 1024;

    if (!appUsage.isSuccess)
    {
        CRASHDUMP_PRINT(INFO, stderr, "Failed to get usage memory;");

        // if > 2S then skip so we get a collection; other wise keep collecting
        if (numOfCpus > 2)
        {
            CRASHDUMP_PRINT(INFO, stderr, "Skipping\n");
            return true;
        }
        else
        {
            CRASHDUMP_PRINT(INFO, stderr, "Continuing\n");
            return false;
        }
    }
    collectionMemoryNeeded =
        appUsage.memoryUsage + (sectionMaxMemMB * numOfCpus);

    // Project our final size
    maxMemoryNeeded = collectionMemoryNeeded * expansionRatio;

    // Check limits of memory
    SystemUsageInfo systemMemory = getCurrentAvailableMemory();
    // Convert to MB
    systemMemory.memoryAvailable /= 1024;

    if (!systemMemory.isSuccess)
    {
        CRASHDUMP_PRINT(INFO, stderr,
                        "Failed to get available memory; Continuing\n");
        // if > 2S then skip so we get a collection; other wise keep collecting
        if (numOfCpus > 2)
        {
            CRASHDUMP_PRINT(INFO, stderr, "Skipping\n");
            return true;
        }
        else
        {
            CRASHDUMP_PRINT(INFO, stderr, "Continuing\n");
            return false;
        }
    }
    else if (maxMemoryNeeded > systemMemory.memoryAvailable)
    {
        CRASHDUMP_PRINT(
            INFO, stderr,
            "Memory is too low %dMB > %ldMB with expansion ratio: %.2f.\n",
            maxMemoryNeeded, systemMemory.memoryAvailable, expansionRatio);
        return true;
    }
    else
    {
        return false;
    }
}

bool checkAvailableMemory(CPUInfo* cpus, cJSON** root, int32_t section)
{
    // ExpansionRatio; is optional
    double expansionRatio = MEM_CHECK_FINAL_EXPANSION_RATIO_DEFAULT_VALUE;
    cJSON* expansionRatioObj = cJSON_GetObjectItemCaseSensitive(
        cpus[0].inputFile.bufferPtr, MEM_CHECK_FINAL_EXPANSION_RATIO_KEY);
    if (expansionRatioObj != NULL)
    {
        expansionRatio = expansionRatioObj->valuedouble;
    }

    // Check for MemRequired
    bool skipSection = false;
    cJSON* memCheckObj = getJsonFlagFromSection(cpus[0].inputFile.bufferPtr,
                                                section, MEM_CHECK_INPUT_KEY);
    if (memCheckObj != NULL)
    {
        // Get section name
        char* sectionName = NULL;
        sectionName = getSectionName(cpus[0].inputFile.bufferPtr, section);
        CRASHDUMP_PRINT(ERR, stderr, "Checking memory for %s\n", sectionName);

        // Run memory projection formula
        skipSection = skipSectionOnLowMemory(
            memCheckObj->valueint, getNumberOfCpus(cpus), expansionRatio);

        if (skipSection)
        {
            CRASHDUMP_PRINT(ERR, stderr, "Skipping %s\n", sectionName);

            // Add _skip_section_<section_name> object to output
            char jsonItemString[LOGGER_JSON_STRING_LEN];

            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN,
                          SKIPPED_SECTION_KEY_FMT, sectionName);
            // Add string to Section OutputPath
            cJSON* currentSectionObj =
                getSectionObj(cpus[0].inputFile.bufferPtr, section);
            logStringAtPath(*root, currentSectionObj, jsonItemString,
                            SKIPPED_SECTION_VALUE);
        }
    }
    return skipSection;
    ;
}
