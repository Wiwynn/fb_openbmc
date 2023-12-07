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

#include "engine/BigCore.h"
#include "engine/Crashlog.h"
#include "engine/TorDump.h"
#include "utils.h"

const CrashdumpSection sectionNames[NUMBER_OF_SECTIONS] = {
    {"MCA", MCA_UNCORE, RECORD_TYPE_MCALOG, NULL},
    {"uncore", UNCORE, RECORD_TYPE_UNCORESTATUSLOG, NULL},
    {"TOR", TOR, RECORD_TYPE_TORDUMP, NULL},
    {"PM_info", PM_INFO, RECORD_TYPE_PMINFO, NULL},
    {"address_map", ADDRESS_MAP, RECORD_TYPE_ADDRESSMAP, NULL},
#ifdef OEMDATA_SECTION
    {"OEM", OEM, RECORD_TYPE_OEM, NULL},
#endif
    {"big_core", BIG_CORE, RECORD_TYPE_CORECRASHLOG, NULL},
    {"MCA", MCA_CORE, RECORD_TYPE_MCALOG, NULL},
    {"METADATA", METADATA, RECORD_TYPE_METADATA},
    {"crashlog", CRASHLOG, RECORD_TYPE_METADATA, NULL}};

int revision_uncore = 0;
bool commonMetaDataEnabled = true;
const DieMaskBits DieMaskRange = {0xFFFFFFFF};

void logCrashdumpVersion(cJSON* parent, CPUInfo* cpuInfo, int recordType)
{
    VersionInfo cpuProduct[cd_numberOfModels] = {
        {cd_icx, PRODUCT_TYPE_ICXSP},  {cd_icx2, PRODUCT_TYPE_ICXSP},
        {cd_spr, PRODUCT_TYPE_SPR},    {cd_icxd, PRODUCT_TYPE_ICXDSP},
        {cd_emr, PRODUCT_TYPE_SPR},    {cd_gnr, PRODUCT_TYPE_GNR_SP},
        {cd_srf, PRODUCT_TYPE_GNR_SP},
    };

    VersionInfo cpuRevision[cd_numberOfModels] = {
        {cd_icx, REVISION_1},  {cd_icx2, REVISION_1}, {cd_spr, REVISION_1},
        {cd_icxd, REVISION_1}, {cd_emr, REVISION_1},  {cd_gnr, REVISION_1},
        {cd_srf, REVISION_1},
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
        clientAddr, domainID, 0, 30, device, 0, 0x489, sizeof(coreMask1),
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
        getComputeDieCoreMasks(cpu->clientAddr, &cpu->dieMaskInfo, domainID,
                               &cpu->computeDies[idx], cpuState);
        idx++;
    }
    return true;
}

static bool getCoreMasksICXSPR(CPUInfo* cpu, cpuidState cpuState)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;

    uint32_t coreMask0 = 0x0;
    uint32_t coreMask1 = 0x0;
    cpu->coreMask = 0;
    switch (cpu->model)
    {
        case cd_icx:
        case cd_icx2:
        case cd_icxd:
            // RESOLVED_CORES Local PCI B14:D30:F3 Reg 0xD0 and 0xD4
            retval = peci_RdPCIConfigLocal(cpu->clientAddr, 14, 30, 3, 0xD0,
                                           sizeof(coreMask0),
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
            retval = peci_RdPCIConfigLocal(cpu->clientAddr, 14, 30, 3, 0xD4,
                                           sizeof(coreMask1),
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
        getComputeDieCHAMasks(cpu->clientAddr, &cpu->dieMaskInfo, domainID,
                              &cpu->computeDies[idx], cpuState);
        idx++;
    }
    return true;
}

static bool getCHACountsICXSPR(CPUInfo* cpu, cpuidState cpuState)
{
    uint8_t cc = 0;
    EPECIStatus retval = PECI_CC_SUCCESS;

    uint32_t chaMask0 = 0x0;
    uint32_t chaMask1 = 0x0;
    cpu->chaCount = 0;
    switch (cpu->model)
    {
        case cd_icx:
        case cd_icx2:
        case cd_icxd:
            // LLC_SLICE_EN Local PCI B14:D30:F3 Reg 0x9C and 0xA0
            retval = peci_RdPCIConfigLocal(cpu->clientAddr, 14, 30, 3, 0x9C,
                                           sizeof(chaMask0),
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
            retval = peci_RdPCIConfigLocal(cpu->clientAddr, 14, 30, 3, 0xA0,
                                           sizeof(chaMask1),
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
            case cd_srf:
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
                ret = getCoreMasksICXSPR(cpu, cpuState);
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
            case cd_srf:
                ret = getCHACountsGNR(cpu, cpuState);
                break;
            default:
                if (cpu->chaCountRead.chaCountValid)
                {
                    continue;
                }
                ret = getCHACountsICXSPR(cpu, cpuState);
        }
    }
    return ret;
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

uint8_t mapDieNumToBitGNR(uint32_t dieMask, uint32_t posToMap,
                          uint32_t rangeMask)
{
    uint32_t mapMask = 0;
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

bool isDomainComputeGNR(uint32_t domainID)
{
    return (GNR_COMPUTE_DIE_RANGE & (1 << domainID));
}
