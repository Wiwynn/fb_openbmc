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

#ifndef TPMI_H
#define TPMI_H

#include "crashdump.h"

#define MB_OPCODE_CORE_PHY2LOG 0x00000077
#define MB_OPCODE_CHA_PHY2LOG 0x00000076
#define MB_IDS_PER_INDEX 4
#define INVALID_CHA 0xFF

typedef struct
{
    uint8_t segment;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t barIndex;
    uint8_t addressType;
    uint8_t cpuAddr;
} TPMIInfo;

typedef struct
{
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t seg;
} BDFS;

typedef enum
{
    CORE_TYPE = 0,
    CHA_TYPE,
    INVALID_TYPE,
} PHY2LOG_Type;

bool getBusEnumeration(uint8_t clientAddr, uint8_t busNumber, BDFS* bdf,
                       uint8_t* postEnumBus);
acdStatus tpmi_getPHY2LOG(CPUInfo* cpus, cpuidState cpuState,
                          PHY2LOG_Type Type);
bool ValidatePhy2LogTable(PHY2LOGRead* phy2logRead, uint8_t dieCount,
                          PHY2LOG_Type Type);
#endif // TPMI_H
