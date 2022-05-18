//*********************************************************************************
//
// File Name :   IPMB_peci_interface.h
// Description : IPMB PECI Interface
// Copyright(c) 2020 Intel Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.
//
//
// SPDX-License-Identifier: Apache-2.0
//
//*********************************************************************************
#ifndef __IPMB_PECI_INTERFACE_H__
#define __IPMB_PECI_INTERFACE_H__

//#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

#define PECI_MBX_INDEX_CPU_ID			0  /* Package Identifier Read */

/* When index is PECI_MBX_INDEX_CPU_ID */
#define PECI_PKG_ID_CPU_ID			0x0000  /* CPUID Info */
#define PECI_PKG_ID_PLATFORM_ID			0x0001  /* Platform ID */
#define PECI_PKG_ID_UNCORE_ID			0x0002  /* Uncore Device ID */
#define PECI_PKG_ID_MAX_THREAD_ID		0x0003  /* Max Thread ID */
#define PECI_PKG_ID_MICROCODE_REV		0x0004  /* CPU Microcode Update Revision */
#define PECI_PKG_ID_MACHINE_CHECK_STATUS	0x0005  /* Machine Check Status */

/* Crashdump Agent */
#define PECI_CRASHDUMP_CORE			0x00
#define PECI_CRASHDUMP_TOR			0x01

/* Crashdump Agent Param */
#define PECI_CRASHDUMP_PAYLOAD_SIZE		0x00

/* Crashdump Agent Data Param */
#define PECI_CRASHDUMP_AGENT_ID			0x00
#define PECI_CRASHDUMP_AGENT_PARAM		0x01

#define PECI_CRASHDUMP_ENABLED			0x00
#define PECI_CRASHDUMP_NUM_AGENTS		0x01
#define PECI_CRASHDUMP_AGENT_DATA		0x02

#define PECI_DATA_BUF_SIZE 32

typedef struct
{
	unsigned char target;
	unsigned char write_len;
	unsigned char read_len;
	unsigned char write_buffer[PECI_DATA_BUF_SIZE];
}__attribute__((packed)) peci_cmd_t;

#define PECI_HEADER_SIZE 3

#define PECI_GET_DIB                        0xf7
#define PECI_GET_TEMP                       0x00
#define PECI_RDPKG                          0xa0
#define PECI_WRPKG                          0xa4
#define PECI_RDIAMSR                        0xb0
#define PECI_WRIAMSR                        0xb4
#define PECI_RDPCICONFIG                    0x60
#define PECI_WRPCICONFIG                    0x64
#define PECI_RD_END_PT_CFG                  0xc1
#define PECI_WR_END_PT_CFG                  0xc5
#define PECI_CRASHDUMP                      0x71
#define PECI_MBX_SEND                       0xd0
#define PECI_MBX_GET                        0xd4
#define PECI_RDPCICONFIGLOCAL               0xe0
#define PECI_WRPCICONFIGLOCAL               0xe4
#define PECI_ENDPTCFG_TYPE_LOCAL_PCI        0x03
#define PECI_ENDPTCFG_TYPE_PCI              0x04
#define PECI_ENDPTCFG_TYPE_MMIO             0x05
#define PECI_ENDPTCFG_ADDR_TYPE_PCI         0x04
#define PECI_ENDPTCFG_ADDR_TYPE_MMIO_D      0x05
#define PECI_ENDPTCFG_ADDR_TYPE_MMIO_Q      0x06

// PECI Client Address List
#define MIN_CLIENT_ADDR 0x30
#define MAX_CLIENT_ADDR 0x30
#define MAX_CPUS (MAX_CLIENT_ADDR - MIN_CLIENT_ADDR + 1)

typedef enum
{
	skx = 0x00050650,
	icx = 0x000606A0,
} CPUModel;

// PECI Status Codes
typedef enum
{
	PECI_CC_SUCCESS = 0,
	PECI_CC_IPMB_SUCCESS = 0,
	PECI_CC_INVALID_REQ,
	PECI_CC_HW_ERR,
	PECI_CC_DRIVER_ERR,
	PECI_CC_CPU_NOT_PRESENT,
	PECI_CC_MEM_ERR,
	PECI_CC_IPMB_ERR,
} EPECIStatus;


/* Device Specific Completion Code (CC) Definition */
#define PECI_DEV_CC_SUCCESS                             0x40
#define PECI_DEV_CC_NEED_RETRY                          0x80
#define PECI_DEV_CC_OUT_OF_RESOURCE                     0x81
#define PECI_DEV_CC_UNAVAIL_RESOURCE                    0x82
#define PECI_DEV_CC_INVALID_REQ                         0x90
#define PECI_DEV_CC_MCA_ERROR                           0x91
#define PECI_DEV_CC_CATASTROPHIC_MCA_ERROR              0x93
#define PECI_DEV_CC_FATAL_MCA_DETECTED                  0x94
#define PECI_DEV_CC_PARITY_ERROR_ON_GPSB_OR_PMSB        0x98
#define PECI_DEV_CC_PARITY_ERROR_ON_GPSB_OR_PMSB_IERR	0x9B
#define PECI_DEV_CC_PARITY_ERROR_ON_GPSB_OR_PMSB_MCA	0x9C

// PECI Timeout Options
typedef enum
{
        PECI_WAIT_FOREVER = -1,
        PECI_NO_WAIT = 0,
} EPECITimeout;

#define PECI_TIMEOUT_RESOLUTION_MS 10 // 10 ms
#define PECI_TIMEOUT_MS 100           // 100 ms

// VCU Index and Sequence Paramaters
#define VCU_SET_PARAM									0x0001
#define VCU_READ									0x0002
#define VCU_OPEN_SEQ									0x0003
#define VCU_CLOSE_SEQ									0x0004
#define VCU_ABORT_SEQ									0x0005
#define VCU_VERSION									0x0009

typedef enum
{
	VCU_READ_LOCAL_CSR_SEQ = 0x2,
	VCU_READ_LOCAL_MMIO_SEQ = 0x6,
	VCU_EN_SECURE_DATA_SEQ = 0x14,
	VCU_CORE_MCA_SEQ = 0x10000,
	VCU_UNCORE_MCA_SEQ = 0x10000,
	VCU_IOT_BRKPT_SEQ = 0x10010,
	VCU_MBP_CONFIG_SEQ = 0x10026,
	VCU_PWR_MGT_SEQ = 0x1002a,
	VCU_CRASHDUMP_SEQ = 0x10038,
	VCU_ARRAY_DUMP_SEQ = 0x20000,
	VCU_SCAN_DUMP_SEQ = 0x20008,
	VCU_TOR_DUMP_SEQ = 0x30002,
	VCU_SQ_DUMP_SEQ = 0x30004,
	VCU_UNCORE_CRASHDUMP_SEQ = 0x30006,
} EPECISequence;

#define MBX_INDEX_VCU 128 // VCU Index

typedef enum
{
	MMIO_DWORD_OFFSET = 0x05,
	MMIO_QWORD_OFFSET = 0x06,
} EEndPtMmioAddrType;

// Find the specified PCI bus number value
EPECIStatus FindBusNumber(uint8_t u8Bus, uint8_t u8Cpu, uint8_t* pu8BusValue);

// Gets the temperature from the target
// Expressed in signed fixed point value of 1/64 degrees celsius
EPECIStatus peci_GetTemp(uint8_t target, int16_t* temperature);

// Provides read access to the package configuration space within the processor
EPECIStatus peci_RdPkgConfig(uint8_t target, uint8_t u8Index, uint16_t u16Value,
	uint8_t u8ReadLen, uint8_t* pPkgConfig,
	uint8_t* cc);

// Allows sequential RdPkgConfig with the provided peci file descriptor
EPECIStatus peci_RdPkgConfig_seq(uint8_t target, uint8_t u8Index,
	uint16_t u16Value, uint8_t u8ReadLen,
	uint8_t* pPkgConfig, int peci_fd, uint8_t* cc);

// Provides write access to the package configuration space within the processor
EPECIStatus peci_WrPkgConfig(uint8_t target, uint8_t u8Index, uint16_t u16Param,
	uint32_t u32Value, uint8_t u8WriteLen,
	uint8_t* cc);

// Allows sequential WrPkgConfig with the provided peci file descriptor
EPECIStatus peci_WrPkgConfig_seq(uint8_t target, uint8_t u8Index,
	uint16_t u16Param, uint32_t u32Value,
	uint8_t u8WriteLen, int peci_fd, uint8_t* cc);

// Provides read access to Model Specific Registers
EPECIStatus peci_RdIAMSR(uint8_t target, uint8_t threadID, uint16_t MSRAddress,
	uint64_t* u64MsrVal, uint8_t* cc);

// Provides read access to PCI Configuration space
EPECIStatus peci_RdPCIConfig(uint8_t target, uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg, uint8_t* pPCIData,
	uint8_t* cc);

// Allows sequential RdPCIConfig with the provided peci file descriptor
EPECIStatus peci_RdPCIConfig_seq(uint8_t target, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t* pPCIData,
	int peci_fd, uint8_t* cc);

// Provides read access to the local PCI Configuration space
EPECIStatus peci_RdPCIConfigLocal(uint8_t target, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t u8ReadLen,
	uint8_t* pPCIReg, uint8_t* cc);

// Allows sequential RdPCIConfigLocal with the provided peci file descriptor
EPECIStatus peci_RdPCIConfigLocal_seq(uint8_t target, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t u8ReadLen,
	uint8_t* pPCIReg, int peci_fd,
	uint8_t* cc);

// Provides write access to the local PCI Configuration space
EPECIStatus peci_WrPCIConfigLocal(uint8_t target, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t DataLen,
	uint32_t DataVal, uint8_t* cc);

// Provides read access to PCI configuration space
EPECIStatus peci_RdEndPointConfigPci(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t u8ReadLen, uint8_t* pPCIData,
	uint8_t* cc);

// Allows sequential RdEndPointConfig to PCI Configuration space
EPECIStatus peci_RdEndPointConfigPci_seq(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t u8ReadLen, uint8_t* pPCIData,
	int peci_fd, uint8_t* cc);

// Provides read access to the local PCI configuration space
EPECIStatus peci_RdEndPointConfigPciLocal(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t u8ReadLen, uint8_t* pPCIData,
	uint8_t* cc);

// Allows sequential RdEndPointConfig to the local PCI Configuration space
EPECIStatus peci_RdEndPointConfigPciLocal_seq(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t u8ReadLen,
	uint8_t* pPCIData, int peci_fd,
	uint8_t* cc);

// Provides read access to PCI MMIO space
EPECIStatus peci_RdEndPointConfigMmio(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint8_t u8Bar,
	uint8_t u8AddrType, uint64_t u64Offset,
	uint8_t u8ReadLen, uint8_t* pMmioData,
	uint8_t* cc);

// Allows sequential RdEndPointConfig to PCI MMIO space
EPECIStatus peci_RdEndPointConfigMmio_seq(
	uint8_t target, uint8_t u8Seg, uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint8_t u8Bar, uint8_t u8AddrType, uint64_t u64Offset,
	uint8_t u8ReadLen, uint8_t* pMmioData, int peci_fd, uint8_t* cc);

// Provides write access to the EP local PCI Configuration space
EPECIStatus peci_WrEndPointPCIConfigLocal(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t DataLen, uint32_t DataVal,
	uint8_t* cc);

// Provides write access to the EP PCI Configuration space
EPECIStatus peci_WrEndPointPCIConfig(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t DataLen, uint32_t DataVal,
	uint8_t* cc);

// Allows sequential write access to the EP PCI Configuration space
EPECIStatus peci_WrEndPointConfig_seq(uint8_t target, uint8_t u8MsgType,
	uint8_t u8Seg, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t DataLen,
	uint32_t DataVal, int peci_fd,
	uint8_t* cc);

// Provides access to the Crashdump Discovery API
EPECIStatus peci_CrashDump_Discovery(uint8_t target, uint8_t subopcode,
	uint8_t param0, uint16_t param1,
	uint8_t param2, uint8_t u8ReadLen,
	uint8_t* pData, uint8_t* cc);

// Provides access to the Crashdump GetFrame API
EPECIStatus peci_CrashDump_GetFrame(uint8_t target, uint16_t param0,
	uint16_t param1, uint16_t param2,
	uint8_t u8ReadLen, uint8_t* pData,
	uint8_t* cc);

// Provides raw PECI command access
EPECIStatus peci_raw(uint8_t target, uint8_t u8ReadLen, const uint8_t* pRawCmd,
	const uint32_t cmdSize, uint8_t* pRawResp,
	uint32_t respSize);

EPECIStatus peci_Lock(int* peci_fd, int timeout_ms);
void peci_Unlock(int peci_fd);
EPECIStatus peci_Ping(uint8_t target);
EPECIStatus peci_Ping_seq(uint8_t target, int peci_fd);
EPECIStatus peci_GetCPUID(const uint8_t clientAddr, CPUModel* cpuModel,
	uint8_t* stepping, uint8_t* cc);

#ifdef __cplusplus

}

#endif
#endif // __IPMB_PECI_INTERFACE_H__
