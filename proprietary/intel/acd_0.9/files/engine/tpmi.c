/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2024 Intel Corporation.
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
#include "tpmi.h"

#include "utils.h"
/*TPMI Registers*/
#define TPMI_GNR_SRF_BMC_CTL_BASE 0xC000
#define TPMI_BMC_CTL_INTERFACE TPMI_GNR_SRF_BMC_CTL_BASE + 8
#define TPMI_BMC_CTL_DATA TPMI_GNR_SRF_BMC_CTL_BASE + 16

/*Bus register information for platforms*/
#define GNR_SRF_MMIO_BUS 8
#define GNRD_MMIO_BUS 3
#define MAX_BUSNO 29
#define MAX_CPUBUSNO_REG 8

/*
 * This function validates the table within a PHY2LOGRead Core struct.
 * It checks that there are no duplicate values in the core table.
 * If the table is valid, it sets the isValid field of the struct to true.
 * If the table is not valid, it sets the isValid field of the struct to false.
 * returns false if any table is invalid
 * return true if all tables are valid
 */
bool ValidatePhy2LogTableCore(PHY2LOGRead* phy2logRead, uint8_t dieCount)
{
    int computeIndex, coreIndex, coreIndexY;

    // Loop through each element in the table
    for (computeIndex = 0; computeIndex < dieCount; computeIndex++)
    {
        for (coreIndex = 0; coreIndex < GNR_PHY2LOG_MAX_INDICES; coreIndex++)
        {
            // Check for duplicate values in the table
            for (coreIndexY = coreIndex + 1;
                 coreIndexY < GNR_PHY2LOG_MAX_INDICES; coreIndexY++)
            {
                if (phy2logRead->table[computeIndex][coreIndex] ==
                    phy2logRead->table[computeIndex][coreIndexY])
                {
                    // If a duplicate is found, the table is not valid
                    CRASHDUMP_PRINT(ERR, stderr,
                                    "Phy2log core is invalid at compute: %d \n",
                                    computeIndex);
                    phy2logRead->isValid = false;
                    return phy2logRead->isValid;
                }
            }
        }
    }

    // If no duplicates or more than one zero were found, the table is valid
    phy2logRead->isValid = true;
    return phy2logRead->isValid;
}

/*
 * This function validates the table within a PHY2LOGRead CHA struct.
 * It checks that there are no duplicate values in the Cha table.
 * It also ignores invalid chas that are represented by 0xFF
 * If the table is valid, it sets the isValid field of the struct to true.
 * If the table is not valid, it sets the isValid field of the struct to false.
 * returns false if any table is invalid
 * return true if all tables are valid
 */
bool ValidatePhy2LogTableCHA(PHY2LOGRead* phy2logRead, uint8_t dieCount)
{
    // Create a pointer to loop through the table as a 1D array
    uint8_t* phy2logBuffer = (uint8_t*)phy2logRead->table;
    // Loop through each element in the table as a 1D array
    for (int currentIndex = 0;
         currentIndex < dieCount * GNR_PHY2LOG_MAX_INDICES * MB_IDS_PER_INDEX;
         currentIndex++)
    {
        for (int nextIndex = currentIndex + 1;
             nextIndex < dieCount * GNR_PHY2LOG_MAX_INDICES * MB_IDS_PER_INDEX;
             nextIndex++)
        {

            if (phy2logBuffer[currentIndex] == INVALID_CHA)
            {
                // 255 is invalid cha and will show up several times;
                // ignore this indice
                break;
            }

            if (phy2logBuffer[currentIndex] == phy2logBuffer[nextIndex])
            {
                // If a duplicate is found, the table is not valid
                CRASHDUMP_PRINT(
                    ERR, stderr,
                    "Phy2log cha is invalid at compute: %d byte %d and %d\n",
                    (currentIndex / GNR_PHY2LOG_MAX_INDICES), currentIndex,
                    nextIndex);
                phy2logRead->isValid = false;
                return phy2logRead->isValid;
            }
        }
    }

    // If no duplicates the table is valid
    phy2logRead->isValid = true;
    return phy2logRead->isValid;
}

/*
 * This function validates the table within a PHY2LOGRead Core struct.
 */
bool ValidatePhy2LogTable(PHY2LOGRead* phy2logRead, uint8_t dieCount,
                          PHY2LOG_Type type)
{
    if (type == CORE_TYPE)
    {
        return ValidatePhy2LogTableCore(phy2logRead, dieCount);
    }
    else if (type == CHA_TYPE)
    {
        return ValidatePhy2LogTableCHA(phy2logRead, dieCount);
    }

    return false;
}

/*
 * Returns true if busNumber was properly recieved otherwise false
 */
bool getBusEnumeration(uint8_t clientAddr, uint8_t busNumber, BDFS* bdfs,
                       uint8_t* postEnumBus)
{
    uint32_t busValid;
    EPECIStatus status;
    uint8_t cc = 0;

    if (!(busNumber < MAX_BUSNO))
    {
        return false;
    }

    // Registers to PostEnumBus
    int16_t busNumberOffset[MAX_CPUBUSNO_REG] = {0x190, 0x194, 0x198, 0x19C,
                                                 0x1C0, 0x1C4, 0x1C8, 0x1CC};
    // Calculate register; 4 per register
    uint16_t busOffset = busNumberOffset[busNumber / 4];

    // CPUBUSNO_VALID
    status = peci_RdEndPointConfigPciLocal_dom(
        clientAddr, 0, bdfs->seg, bdfs->bus, bdfs->device, bdfs->function,
        0x1a0, 4, (uint8_t*)&busValid, &cc);
    if (status != PECI_CC_SUCCESS)
    {
        CRASHDUMP_PRINT(ERR, stderr, "CPUBusNumValid retrieval failed\n");
        return false;
    }
    if (!CHECK_BIT(busValid, busNumber))
    {
        return false;
    }

    // CPUBUSNO
    uint32_t EnumBus;
    status = peci_RdEndPointConfigPciLocal_dom(
        clientAddr, 0, bdfs->seg, bdfs->bus, bdfs->device, bdfs->function,
        busOffset, 4, (uint8_t*)&EnumBus, &cc);
    if (status != PECI_CC_SUCCESS)
    {
        CRASHDUMP_PRINT(ERR, stderr, "CPUBusNo%d retrieval failed\n",
                        busNumber);
    }

    // 4 buses per register;
    *postEnumBus = (EnumBus >> ((busNumber % 4) * 8)) & 0xFF;
    return true;
}

acdStatus tpmi_getPHY2LOG(CPUInfo* cpus, cpuidState cpuState, PHY2LOG_Type type)
{
    uint32_t mbInterfaceReg = 0;
    uint32_t mbBusy = 0;
    uint32_t mbOpCode = 0;
    uint32_t mbData = 0;
    bool isDataValid = false;
    uint8_t cc = 0;
    TPMIInfo tpmi = {};
    BDFS bdfs = {};
    EPECIStatus status;
    PHY2LOGRead* phy2logRead;

    if (type >= INVALID_TYPE)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Invalid Type. Default to Core\n");
        type = CORE_TYPE;
    }

    // Set TPMI access attributes by model
    // Loop through cpu and computes
    for (int i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpus[i];

        //// Common MB members
        // Busy bit 31 needs to be set
        mbBusy = 0x80000000;

        if (type == CORE_TYPE)
        {
            // opcode byte
            mbOpCode = MB_OPCODE_CORE_PHY2LOG;
            phy2logRead = &cpu->phy2logReadCore;
        }
        else if (type == CHA_TYPE)
        {
            // opcode byte
            mbOpCode = MB_OPCODE_CHA_PHY2LOG;
            phy2logRead = &cpu->phy2logReadCHA;
        }
        // Only need to read phy2log once
        if (cpu->clientAddr == 0 || phy2logRead->isValid)
        {
            continue;
        }
        else if (!cpus->dieMaskInfo.maskValid)
        {
            CRASHDUMP_PRINT(ERR, stderr,
                            "Skipping phy2log due to invalid diemask\n");
            continue;
        }

        /*  TPMI registers are hardcoded and are static per platform.
            The below values have been retrieved using tpmi scripts:
           tpmi-peci.sh , tpmi-dbus.sh Scripts did the upfront work of discovery
           of TPMI VSEC and power features. For BHS we decided to hardcode tpmi
           registers as they are static per platform and wanted to simplify
           retrieval rather than retreive from dbus
            */
        switch (cpu->model)
        {
            case cd_gnr:
            case cd_srf:

                tpmi.addressType = 6;
                tpmi.segment = 0;
                tpmi.bus = 0;
                tpmi.device = 3;
                tpmi.function = 7;
                tpmi.barIndex = 1;

                bdfs.bus = 8;
                bdfs.device = 3;
                bdfs.function = 0;
                bdfs.seg = 0;

                // Get bus8 for TPMI MMIO calls
                if (!getBusEnumeration(cpu->clientAddr, GNR_SRF_MMIO_BUS, &bdfs,
                                       &tpmi.bus))
                {
                    return ACD_FAILURE;
                }
                break;
            case cd_gnrd:
                tpmi.addressType = 6;
                tpmi.segment = 0;
                tpmi.bus = 0;
                tpmi.device = 3;
                tpmi.function = 7;
                tpmi.barIndex = 1;

                bdfs.bus = 3;
                bdfs.device = 3;
                bdfs.function = 0;
                bdfs.seg = 0;

                // Get bus8 for TPMI MMIO calls
                if (!getBusEnumeration(cpu->clientAddr, GNRD_MMIO_BUS, &bdfs,
                                       &tpmi.bus))
                {
                    CRASHDUMP_PRINT(ERR, stderr, "Bus issue\n");
                    return ACD_FAILURE;
                }
                break;
            default:
                // Skip this step
                return ACD_SUCCESS;
                break;
        }

        // Loop through compute count
        int8_t dieCount =
            __builtin_popcount(cpu->dieMaskInfo.compute.effectiveMask);

        for (int computeDie = 0; computeDie < dieCount; computeDie++)
        {
            uint32_t mbComputeDie = 0;
            uint32_t mbIndex = 0;

            // We get 4 cores/Chas per command
            for (int index = 0; index < GNR_PHY2LOG_MAX_INDICES; index++)
            {
                mbComputeDie = computeDie << 16;
                mbIndex = index << 8;
                mbInterfaceReg = mbBusy | mbComputeDie | mbIndex | mbOpCode;

                // Write BMC_CTL_INTERFACE; Domain 0 is ok
                status = peci_WrEndPointConfigMmio_dom(
                    cpu->clientAddr, 0, tpmi.segment, tpmi.bus, tpmi.device,
                    tpmi.function, tpmi.barIndex, tpmi.addressType,
                    TPMI_BMC_CTL_INTERFACE, sizeof(uint32_t), mbInterfaceReg,
                    &cc);

                if (status != PECI_CC_SUCCESS || PECI_CC_UA(cc))
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Phy2Log BMC_CTL_INTERFACE failed on compute %d \n", i);
                    mbData = 0;
                }

                // Read BMC_CTL_MAILBOX
                status = peci_RdEndPointConfigMmio_dom(
                    cpu->clientAddr, 0, tpmi.segment, tpmi.bus, tpmi.device,
                    tpmi.function, tpmi.barIndex, tpmi.addressType,
                    TPMI_BMC_CTL_DATA, sizeof(uint32_t), (uint8_t*)&mbData,
                    &cc);
                if (status != PECI_CC_SUCCESS || PECI_CC_UA(cc))
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Phy2Log BMC_CTL_INTERFACE failed on compute %d\n", i);
                    mbData = 0;
                }

                // Read the 4 cores per command
                phy2logRead->table[computeDie][index] = mbData;
            }
        }

        // Check data
        isDataValid = ValidatePhy2LogTable(phy2logRead, dieCount, type);
        if (!isDataValid)
        {
            return ACD_FAILURE;
        }
    }
    // if last data was valid we made it through
    if (isDataValid)
    {
        return ACD_SUCCESS;
    }
    // Otherwise there may have been some skipping
    return ACD_FAILURE;
}
