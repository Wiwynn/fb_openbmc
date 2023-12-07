//*********************************************************************************
//
// File Name :   IPMB_peci_interface.c
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

#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <linux/peci-ioctl.h>
#include <openbmc/ipmi.h>
#include <openbmc/ipmb.h>

#include "ipmb_peci_interface.h"

#define PECI_HOST_ID             0x00
#define NODE_IPMB_SLAVE_ADDR     0x20
#define CMD_SEND_RAW_PECI        0x29
#define PECI_DATA_BUF_SIZE       32
#define IANA_SIZE                3
#define MAX_RETRIES              10
#define MAX_RETRY_INTERVAL_MSEC  128

EPECIStatus peci_GetDIB_seq(uint8_t target, uint64_t* dib);

int node_bus_id;  // bus# for compute node

#define POLYCHECK (0x1070U << 3) //0x07
static uint8_t crc8_calculate(uint16_t d)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (d & 0x8000) {
			d = d ^ POLYCHECK;
		}
		d = d << 1;
	}

	return (uint8_t)(d >> 8);
}

static uint8_t pec_calculate(uint8_t crc, uint8_t *p, size_t count)
{
	int i;

	for (i = 0; i < count; i++) {
		crc = crc8_calculate((crc ^ p[i]) << 8);
	}

	return crc;
}

static uint8_t CalculateCRC8(uint8_t *data, size_t len)
{
    uint8_t crc = 0;

    crc = pec_calculate(crc, data, len);

    return crc;

}

/*-------------------------------------------------------------------------
 * This function calculates the PECI FCS
 *------------------------------------------------------------------------*/
static uint8_t calculate_fcs(peci_cmd_t *peci_cmd)
{
	uint8_t tbuf[PECI_DATA_BUF_SIZE + 3];

	tbuf[0] = peci_cmd->target;
	tbuf[1] = peci_cmd->write_len;
	tbuf[2] = peci_cmd->read_len;
	memcpy(&tbuf[3], peci_cmd->write_buffer, peci_cmd->write_len - 1);

	return CalculateCRC8(tbuf, peci_cmd->write_len + 2);
}

/*-------------------------------------------------------------------------
 * This function opens the peci interface and returns a file descriptor
 *------------------------------------------------------------------------*/
static EPECIStatus peci_Open(int* peci_fd)
{
	if (NULL == peci_fd)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Lock the PECI driver with a default timeout
	return peci_Lock(peci_fd, PECI_TIMEOUT_MS);
}

/*-------------------------------------------------------------------------
 * This function closes the peci interface
 *------------------------------------------------------------------------*/
static void peci_Close(int peci_fd)
{
	peci_Unlock(peci_fd);
}

/*-------------------------------------------------------------------------
 * This function issues peci commands to PECI bridge device
 *------------------------------------------------------------------------*/
static EPECIStatus IPMB_peci_issue_cmd(ipmb_req_t *req, ipmb_res_t *res,
	bool do_retry, bool has_aw_fcs)
{
	EPECIStatus ret = PECI_CC_IPMB_ERR;
	peci_cmd_t  *peci_cmd;
	uint8_t iana[IANA_SIZE] = {0x15, 0xa0, 0x00};
	uint8_t data_tmp[PECI_DATA_BUF_SIZE] = {0};
	int i, tlen, rlen, interval;

	if (NULL == req || NULL == res) {
		return PECI_CC_INVALID_REQ;
	}

	memcpy(req->data, iana, sizeof(iana));

	// add IPMB header to the compute node IPMB interface
	req->res_slave_addr = NODE_IPMB_SLAVE_ADDR << 1;
	req->netfn_lun = NETFN_OEM_1S_REQ << LUN_OFFSET;
	req->cmd = CMD_SEND_RAW_PECI;

	req->hdr_cksum = req->res_slave_addr + req->netfn_lun;
	req->hdr_cksum = ZERO_CKSUM_CONST - req->hdr_cksum;

	req->req_slave_addr = BMC_SLAVE_ADDR << 1;
	req->seq_lun = 0x00;

	peci_cmd = (peci_cmd_t *)&req->data[IANA_SIZE];

	tlen = peci_cmd->write_len + 6 + MIN_IPMB_REQ_LEN;
	for (i = 0; i <= MAX_RETRIES; i++) {
		rlen = ipmb_send_buf(node_bus_id, tlen);
		if (rlen < (MIN_IPMB_RES_LEN + 3 + peci_cmd->read_len)) {
			ret = PECI_CC_IPMB_ERR;
		} else {
			ret = PECI_CC_IPMB_SUCCESS;
		}

		if (!do_retry || (i == MAX_RETRIES) || ret) {
			break;
		}

		// Retry is needed when completion code is 0x8x
		if ((res->data[3] & PECI_DEV_CC_RETRY_CHECK_MASK) !=
		    PECI_DEV_CC_NEED_RETRY) {
			break;
		}

		// Set the retry bit to indicate a retry attempt
		peci_cmd->write_buffer[1] |= PECI_DEV_RETRY_BIT;

		// Recalculate the AW FCS if it has one
		if (has_aw_fcs) {
			peci_cmd->write_buffer[peci_cmd->write_len - 1] = calculate_fcs(peci_cmd) ^ 0x80;
		}

		if ((interval = (1 << i)) > MAX_RETRY_INTERVAL_MSEC) {
			interval = MAX_RETRY_INTERVAL_MSEC;
		}
		usleep(interval * 1000);
	}

	if (ret) {
		syslog(LOG_WARNING, "IPMB ERROR: cmd=%02X, rlen = %d, read_len=%u",
		       peci_cmd->write_buffer[0], rlen, peci_cmd->read_len);
		return PECI_CC_DRIVER_ERR;
	}

	rlen -= (MIN_IPMB_RES_LEN + 3);
	// Remove return iana code
	memset(data_tmp, 0, sizeof(data_tmp));
	memcpy(data_tmp, &res->data[3], rlen);
	memset(res->data, 0, rlen);
	memcpy(res->data, data_tmp, rlen);

	return ret;
}

EPECIStatus peci_Lock(int* peci_fd, int timeout_ms)
{
	return PECI_CC_SUCCESS;
}

void peci_Unlock(int peci_fd)
{

}

/*-------------------------------------------------------------------------
 * This function checks the CPU PECI interface
 *------------------------------------------------------------------------*/
EPECIStatus peci_Ping(uint8_t target)
{
	EPECIStatus ret;

	ret = peci_Ping_seq(target, 0);

	return ret;
}

/*-------------------------------------------------------------------------
 * This function allows sequential Ping with the provided
 * peci file descriptor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_Ping_seq(uint8_t target, int peci_fd)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = 0;
	peci_cmd->write_len = 0;

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, false, false);

	// No CC returned for Ping.

	return ret;
}

/*-------------------------------------------------------------------------
 * This function gets PECI device information
 *------------------------------------------------------------------------*/
EPECIStatus peci_GetDIB(uint8_t target, uint64_t* dib)
{
	EPECIStatus ret;

	if (dib == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ret = peci_GetDIB_seq(target, dib);

	return ret;
}

/*-------------------------------------------------------------------------
 * This function allows sequential GetDIB with the provided
 * peci file descriptor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_GetDIB_seq(uint8_t target, uint64_t* dib)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (dib == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = PECI_GET_DIB_RD_LEN;              // 8 bytes, without completion code
	peci_cmd->write_len = PECI_GET_DIB_WR_LEN;             // 1 byte
	peci_cmd->write_buffer[0] = PECI_GET_DIB_CMD;

	memset(ipmb_res->data, 0, peci_cmd->read_len);
	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, false, false);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		memcpy((uint8_t *)dib, ipmb_res->data, peci_cmd->read_len);
	}

	return ret;
}

/*-------------------------------------------------------------------------
 * This function get PECI Thermal temperature
 * Expressed in signed fixed point value of 1/64 degrees celsius
 *------------------------------------------------------------------------*/
EPECIStatus peci_GetTemp(uint8_t target, int16_t* temperature)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (temperature == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = PECI_GET_TEMP_RD_LEN;             // 2 bytes, without completion code
	peci_cmd->write_len = PECI_GET_TEMP_WR_LEN;            // 1 byte
	peci_cmd->write_buffer[0] = PECI_GET_TEMP_CMD;

	memset(ipmb_res->data, 0, peci_cmd->read_len);
	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, false, false);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		memcpy((uint8_t *)temperature, ipmb_res->data, peci_cmd->read_len);
	}

	return ret;
}

/*-------------------------------------------------------------------------
 * This function provides read access to the package configuration
 * space within the processor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdPkgConfig(uint8_t target, uint8_t u8Index, uint16_t u16Value,
	uint8_t u8ReadLen, uint8_t* pPkgConfig,
	uint8_t* cc)
{
	EPECIStatus ret;

	if (pPkgConfig == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	//  Default to domain ID 0
	return peci_RdPkgConfig_dom(target, 0, u8Index, u16Value, u8ReadLen,
				    pPkgConfig, cc);
}

/*-------------------------------------------------------------------------
 * This function provides read access to the package configuration
 * space within the processor in the specified domain.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdPkgConfig_dom(uint8_t target, uint8_t domainId,
	uint8_t u8Index, uint16_t u16Value,
	uint8_t u8ReadLen, uint8_t* pPkgConfig,
	uint8_t* cc)
{
	int peci_fd = -1;
	EPECIStatus ret = PECI_CC_SUCCESS;

	if (pPkgConfig == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// The target address must be in the valid range
	if (target < MIN_CLIENT_ADDR || target > MAX_CLIENT_ADDR)
	{
		return PECI_CC_INVALID_REQ;
	}

	if (peci_Open(&peci_fd) != PECI_CC_SUCCESS)
	{
		return PECI_CC_DRIVER_ERR;
	}
	ret = peci_RdPkgConfig_seq_dom(target, domainId, u8Index, u16Value,
				       u8ReadLen, pPkgConfig, peci_fd, cc);

	peci_Close(peci_fd);
	return ret;
}

/*-------------------------------------------------------------------------
 * This function allows sequential RdPkgConfig with the provided
 * peci file descriptor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdPkgConfig_seq(uint8_t target, uint8_t u8Index,
	uint16_t u16Value, uint8_t u8ReadLen,
	uint8_t* pPkgConfig, int peci_fd, uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_RdPkgConfig_seq_dom(target, 0, u8Index, u16Value, u8ReadLen,
					pPkgConfig, peci_fd, cc);
}

/*-------------------------------------------------------------------------
 * This function allows sequential RdPkgConfig with the provided
 * peci file descriptor in the specified domain.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdPkgConfig_seq_dom(uint8_t target, uint8_t domainId,
	uint8_t u8Index, uint16_t u16Value,
	uint8_t u8ReadLen, uint8_t* pPkgConfig,
	int peci_fd, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pPkgConfig == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the write length must be a byte, word, or dword
	if (u8ReadLen != 1 && u8ReadLen != 2 && u8ReadLen != 4)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                        // Add 1 for completion code
	peci_cmd->write_len = PECI_RDPKGCFG_WRITE_LEN;             // 5 bytes

	peci_cmd->write_buffer[0] = PECI_RDPKGCFG_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x0;
	peci_cmd->write_buffer[2] = u8Index;                       // RdPkgConfig index
	peci_cmd->write_buffer[3] = (uint8_t)u16Value;             // Config parameter value - low byte
	peci_cmd->write_buffer[4] = (uint8_t)(u16Value >> 8);      // Config parameter value - high byte

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pPkgConfig, &ipmb_res->data[1], u8ReadLen);
	}

	return ret;
}

/*-------------------------------------------------------------------------
 * This function provides write access to the package configuration
 * space within the processor
 *------------------------------------------------------------------------*/
EPECIStatus peci_WrPkgConfig(uint8_t target, uint8_t u8Index, uint16_t u16Param,
	uint32_t u32Value, uint8_t u8WriteLen, uint8_t* cc)
{
	EPECIStatus ret;

	if (cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ret = peci_WrPkgConfig_seq(target, u8Index, u16Param, u32Value, u8WriteLen, 0, cc);

	return ret;
}

/*-------------------------------------------------------------------------
 * This function allows sequential WrPkgConfig with the provided
 * peci file descriptor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_WrPkgConfig_seq(uint8_t target, uint8_t u8Index,
	uint16_t u16Param, uint32_t u32Value,
	uint8_t u8WriteLen, int peci_fd, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the write length must be a byte, word, or dword
	if ((u8WriteLen != 1) && (u8WriteLen != 2) && (u8WriteLen != 4))
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = 1;                                    // 1 byte completion Code
	peci_cmd->write_len = u8WriteLen + 6;                      // 6 bytes: [1:cmd code] [1:host_id]
	                                                           // [1:index] [2:parameter] [1:AW FCS]
	peci_cmd->write_buffer[0] = PECI_WRPKGCFG_CMD;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = u8Index;                       // WrPkgConfig index
	peci_cmd->write_buffer[3] = (uint8_t)u16Param;             // parameter value - low byte
	peci_cmd->write_buffer[4] = (uint8_t)(u16Param >> 8);      // parameter value - high byte
	peci_cmd->write_buffer[5] = (uint8_t)u32Value;
	peci_cmd->write_buffer[6] = (uint8_t)(u32Value >> 8);
	peci_cmd->write_buffer[7] = (uint8_t)(u32Value >> 16);
	peci_cmd->write_buffer[8] = (uint8_t)(u32Value >> 24);

	// FCS is calculated and invert the MSB to get AW FCS.
	peci_cmd->write_buffer[peci_cmd->write_len - 1] = calculate_fcs(peci_cmd) ^ 0x80;

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, true);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
	}

	return ret;
}

/*-------------------------------------------------------------------------
 * This function provides read access to Model Specific Registers
 * defined in the processor doc.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdIAMSR(uint8_t target, uint8_t threadID, uint16_t MSRAddress,
	uint64_t* u64MsrVal, uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_RdIAMSR_dom(target, 0, threadID, MSRAddress, u64MsrVal, cc);
}

/*-------------------------------------------------------------------------
 * This function provides read access to Model Specific Registers
 * defined in the processor doc in the specified domain.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdIAMSR_dom(uint8_t target, uint8_t domainId, uint8_t threadID,
	uint16_t MSRAddress, uint64_t* u64MsrVal,
	uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (u64MsrVal == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = PECI_RDIAMSR_READ_LEN;                // 9 bytes, include completion code
	peci_cmd->write_len = PECI_RDIAMSR_WRITE_LEN;              // 5 bytes

	peci_cmd->write_buffer[0] = PECI_RDIAMSR_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x0;
	peci_cmd->write_buffer[2] = threadID;                      // request byte for thread ID
	peci_cmd->write_buffer[3] = (uint8_t)MSRAddress;           // MSR Address - low byte
	peci_cmd->write_buffer[4] = (uint8_t)(MSRAddress >> 8);    // MSR Address - high byte

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy((uint8_t *)u64MsrVal, &ipmb_res->data[1], 8);
	}

	return ret;
}

/*-------------------------------------------------------------------------
 * This function provides read access to the PCI configuration space at
 * the requested PCI configuration address.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdPCIConfig(uint8_t target, uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg, uint8_t* pPCIData,
	uint8_t* cc)
{
	EPECIStatus ret;

	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ret = peci_RdPCIConfig_seq(target, u8Bus, u8Device, u8Fcn, u16Reg, pPCIData, 0, cc);

	return ret;
}

/*-------------------------------------------------------------------------
 * This function allows sequential RdPCIConfig with the provided
 * peci file descriptor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdPCIConfig_seq(uint8_t target, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t* pPCIData,
	int peci_fd, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = PECI_RDPCICFG_READ_LEN;               // 5 bytes, include completion code
	peci_cmd->write_len = PECI_RDPCICFG_WRITE_LEN;             // 6 bytes

	peci_cmd->write_buffer[0] = PECI_RDPCICFG_CMD;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = (uint8_t)u16Reg;               // Register[11:0]
	peci_cmd->write_buffer[3] = u8Device << 7;                 // Device[19:15]
	peci_cmd->write_buffer[3] |= u8Fcn << 4;                   // Function[14:12]
	peci_cmd->write_buffer[3] |= (uint8_t)(u16Reg >> 8);       // Register[11:0]
	peci_cmd->write_buffer[4] = u8Device >> 1;                 // Device[19:15]
	peci_cmd->write_buffer[4] |= u8Bus << 4;                   // Bus[27:20]
	peci_cmd->write_buffer[5] = u8Bus >> 4;                    // Bus[27:20] & Reserved[31:28]

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pPCIData, &ipmb_res->data[1], 4);
	}

	return ret;
}

/*-------------------------------------------------------------------------
 *  This function provides read access to the local PCI configuration space
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdPCIConfigLocal(uint8_t target, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t u8ReadLen,
	uint8_t* pPCIReg, uint8_t* cc)
{
	EPECIStatus ret;

	if (pPCIReg == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	//  Default to domain ID 0
	return peci_RdPCIConfigLocal_dom(target, 0, u8Bus, u8Device, u8Fcn, u16Reg,
					 u8ReadLen, pPCIReg, cc);
}

/*-------------------------------------------------------------------------
 * This function provides read access to the local PCI configuration space in
 * the specified domain
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdPCIConfigLocal_dom(uint8_t target, uint8_t domainId,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t u8ReadLen, uint8_t* pPCIReg,
	uint8_t* cc)
{
	int peci_fd = -1;
	EPECIStatus ret = PECI_CC_SUCCESS;

	if (pPCIReg == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// The target address must be in the valid range
	if (target < MIN_CLIENT_ADDR || target > MAX_CLIENT_ADDR)
	{
		return PECI_CC_INVALID_REQ;
	}

	if (peci_Open(&peci_fd) != PECI_CC_SUCCESS)
	{
		return PECI_CC_DRIVER_ERR;
	}
	ret = peci_RdPCIConfigLocal_seq_dom(target, domainId, u8Bus, u8Device,
					    u8Fcn, u16Reg, u8ReadLen, pPCIReg,
					    peci_fd, cc);

	peci_Close(peci_fd);
	return ret;
}

/*-------------------------------------------------------------------------
 * This function allows sequential RdPCIConfigLocal with the provided
 * peci file descriptor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdPCIConfigLocal_seq(uint8_t target, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t u8ReadLen,
	uint8_t* pPCIReg, int peci_fd,
	uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_RdPCIConfigLocal_seq_dom(target, 0, u8Bus, u8Device, u8Fcn,
					     u16Reg, u8ReadLen, pPCIReg, peci_fd,
					     cc);
}

/*-------------------------------------------------------------------------
 * This function allows sequential RdPCIConfigLocal with the provided
 * peci file descriptor in the specified domain.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdPCIConfigLocal_seq_dom(uint8_t target, uint8_t domainId,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t u8ReadLen, uint8_t* pPCIReg,
	int peci_fd, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pPCIReg == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the read length must be a byte, word, or dword
	if (u8ReadLen != 1 && u8ReadLen != 2 && u8ReadLen != 4)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                        // Add 1 for completion code
	peci_cmd->write_len = PECI_RDPCICFGLOCAL_WRITE_LEN;        // 5 bytes

	peci_cmd->write_buffer[0] = PECI_RDPCICFGLOCAL_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x0;
	peci_cmd->write_buffer[2] = (uint8_t)u16Reg;               // Register[11:0]
	peci_cmd->write_buffer[3] = u8Device << 7;                 // Device[19:15]
	peci_cmd->write_buffer[3] |= u8Fcn << 4;                   // Function[14:12]
	peci_cmd->write_buffer[3] |= (uint8_t)(u16Reg >> 8);       // Register[11:0]
	peci_cmd->write_buffer[4] = u8Device >> 1;                 // Device[19:15]
	peci_cmd->write_buffer[4] |= u8Bus << 4;                   // Bus[23:20]

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pPCIReg, &ipmb_res->data[1], u8ReadLen);
	}

	return ret;
}

/*-------------------------------------------------------------------------
 *  This function provides write access to the local PCI configuration space
 *------------------------------------------------------------------------*/
EPECIStatus peci_WrPCIConfigLocal(uint8_t target, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t DataLen,
	uint32_t DataVal, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the write length must be a byte, word, or dword
	if (DataLen != 1 && DataLen != 2 && DataLen != 4)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = 1;                                    // 1 byte completion Code
	peci_cmd->write_len = DataLen + 6;                         // 6 [1:cmd code] [1:host_id]
	                                                           // [3:reg,BDF] [1:AW FCS]
	peci_cmd->write_buffer[0] = PECI_WRPCICFGLOCAL_CMD;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = (uint8_t)u16Reg;               // Register[11:0]
	peci_cmd->write_buffer[3] = u8Device << 7;                 // Device[19:15]
	peci_cmd->write_buffer[3] |= u8Fcn << 4;                   // Function[14:12]
	peci_cmd->write_buffer[3] |= (uint8_t)(u16Reg >> 8);       // Register[11:0]
	peci_cmd->write_buffer[4] = u8Device >> 1;                 // Device[19:15]
	peci_cmd->write_buffer[4] |= u8Bus << 4;                   // Bus[23:20]

	memcpy(&peci_cmd->write_buffer[5], (uint8_t *)&DataVal, DataLen);

	// FCS is calculated and invert the MSB to get AW FCS.
	peci_cmd->write_buffer[peci_cmd->write_len - 1] = calculate_fcs(peci_cmd) ^ 0x80;

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, true);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
	}

	return ret;
}

/*-------------------------------------------------------------------------
 * This internal function is the common interface for RdEndPointConfig to PCI in
 * the specified domain
 *------------------------------------------------------------------------*/
static EPECIStatus peci_RdEndPointConfigPciCommon_dom(
	uint8_t target, uint8_t domainId, uint8_t u8MsgType, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device, uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t u8ReadLen, uint8_t* pPCIData, int peci_fd, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the write length must be a byte, word, or dword
	if (u8ReadLen != 1 && u8ReadLen != 2 && u8ReadLen != 4)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                            // Add 1 for completion code
	peci_cmd->write_len = PECI_RDENDPTCFG_PCI_WRITE_LEN;           // 12 bytes

	peci_cmd->write_buffer[0] = PECI_RDENDPTCFG_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x0;
	peci_cmd->write_buffer[2] = u8MsgType;                         // Message Type (0x03, 0x04)
	peci_cmd->write_buffer[3] = 0;                                 // End Point ID 0x00
	peci_cmd->write_buffer[4] = 0;                                 // Reserved
	peci_cmd->write_buffer[5] = 0;                                 // Reserved
	peci_cmd->write_buffer[6] = PECI_ENDPTCFG_ADDR_TYPE_PCI;       // Address Type 0x04
	peci_cmd->write_buffer[7] = u8Seg;                             // Segment
	peci_cmd->write_buffer[8] = (uint8_t)u16Reg;                   // Register[11:0]
	peci_cmd->write_buffer[9] = u8Device << 7;                     // Device[19:15]
	peci_cmd->write_buffer[9] |= u8Fcn << 4;                       // Function[14:12]
	peci_cmd->write_buffer[9] |= (uint8_t)(u16Reg >> 8);           // Register[11:0]
	peci_cmd->write_buffer[10] = u8Device >> 1;                    // Device[19:15]
	peci_cmd->write_buffer[10] |= u8Bus << 4;                      // Bus[27:20]
	peci_cmd->write_buffer[11] = u8Bus >> 4;                       // Bus[27:20] & Reserved[31:28]

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pPCIData, &ipmb_res->data[1], u8ReadLen);
	}

	return ret;
}

/*-------------------------------------------------------------------------
 * This function provides read access to the PCI configuration space at
 * the requested PCI configuration address.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigPci(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t u8ReadLen, uint8_t* pPCIData,
	uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_RdEndPointConfigPci_dom(target, 0, u8Seg, u8Bus, u8Device,
					    u8Fcn, u16Reg, u8ReadLen, pPCIData, cc);
}

/*-------------------------------------------------------------------------
 * This function provides read access to the PCI configuration space at
 * the requested PCI configuration address in the specified domain.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigPci_dom(uint8_t target, uint8_t domainId,
	uint8_t u8Seg, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t u8ReadLen,
	uint8_t* pPCIData, uint8_t* cc)
{
	int peci_fd = -1;
	EPECIStatus ret = PECI_CC_SUCCESS;

	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// The target address must be in the valid range
	if (target < MIN_CLIENT_ADDR || target > MAX_CLIENT_ADDR)
	{
		return PECI_CC_INVALID_REQ;
	}

	if (peci_Open(&peci_fd) != PECI_CC_SUCCESS)
	{
		return PECI_CC_DRIVER_ERR;
	}
	ret = peci_RdEndPointConfigPci_seq_dom(target, domainId, u8Seg, u8Bus,
					       u8Device, u8Fcn, u16Reg, u8ReadLen,
					       pPCIData, peci_fd, cc);
	peci_Close(peci_fd);
	return ret;
}

/*-------------------------------------------------------------------------
 * This function allows sequential RdEndPointConfig to PCI with the provided
 * peci file descriptor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigPci_seq(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t u8ReadLen, uint8_t* pPCIData,
	int peci_fd, uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_RdEndPointConfigPci_seq_dom(target, 0, u8Seg, u8Bus, u8Device,
						u8Fcn, u16Reg, u8ReadLen, pPCIData,
						peci_fd, cc);
}

/*-------------------------------------------------------------------------
 * This function allows sequential RdEndPointConfig to PCI with the provided
 * peci file descriptor in the specified domain.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigPci_seq_dom(uint8_t target, uint8_t domainId,
	uint8_t u8Seg, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t u8ReadLen,
	uint8_t* pPCIData, int peci_fd,
	uint8_t* cc)
{
	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// The target address must be in the valid range
	if (target < MIN_CLIENT_ADDR || target > MAX_CLIENT_ADDR)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the read length must be a byte, word, or dword
	if (u8ReadLen != 1 && u8ReadLen != 2 && u8ReadLen != 4)
	{
		return PECI_CC_INVALID_REQ;
	}

	return peci_RdEndPointConfigPciCommon_dom(
		target, domainId, PECI_ENDPTCFG_TYPE_PCI, u8Seg, u8Bus, u8Device, u8Fcn,
		u16Reg, u8ReadLen, pPCIData, peci_fd, cc);
}

/*-------------------------------------------------------------------------
 * This function provides read access to the Local PCI configuration space at
 * the requested PCI configuration address.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigPciLocal(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t u8ReadLen, uint8_t* pPCIData,
	uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_RdEndPointConfigPciLocal_dom(target, 0, u8Seg, u8Bus, u8Device,
						 u8Fcn, u16Reg, u8ReadLen, pPCIData,
						 cc);
}

/*-------------------------------------------------------------------------
 * This function provides read access to the Local PCI configuration space at
 * the requested PCI configuration address in the specified domain.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigPciLocal_dom(uint8_t target, uint8_t domainId,
	uint8_t u8Seg, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg,
	uint8_t u8ReadLen,
	uint8_t* pPCIData, uint8_t* cc)
{
    int peci_fd = -1;
    EPECIStatus ret = PECI_CC_SUCCESS;

    if (pPCIData == NULL || cc == NULL)
    {
        return PECI_CC_INVALID_REQ;
    }

    // The target address must be in the valid range
    if (target < MIN_CLIENT_ADDR || target > MAX_CLIENT_ADDR)
    {
        return PECI_CC_INVALID_REQ;
    }

    if (peci_Open(&peci_fd) != PECI_CC_SUCCESS)
    {
        return PECI_CC_DRIVER_ERR;
    }
    ret = peci_RdEndPointConfigPciLocal_seq_dom(
        target, domainId, u8Seg, u8Bus, u8Device, u8Fcn, u16Reg, u8ReadLen,
        pPCIData, peci_fd, cc);
    peci_Close(peci_fd);
    return ret;
}

/*-------------------------------------------------------------------------
 * This function allows sequential RdEndPointConfig to PCI Local with the
 *provided peci file descriptor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigPciLocal_seq(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t u8ReadLen,
	uint8_t* pPCIData, int peci_fd,
	uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_RdEndPointConfigPciLocal_seq_dom(
		target, 0, u8Seg, u8Bus, u8Device, u8Fcn, u16Reg, u8ReadLen, pPCIData,
		peci_fd, cc);
}

/*-------------------------------------------------------------------------
 * This function allows sequential RdEndPointConfig to PCI Local with the
 * provided peci file descriptor in the specified domain.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigPciLocal_seq_dom(
	uint8_t target, uint8_t domainId, uint8_t u8Seg, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn, uint16_t u16Reg, uint8_t u8ReadLen,
	uint8_t* pPCIData, int peci_fd, uint8_t* cc)
{
	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// The target address must be in the valid range
	if (target < MIN_CLIENT_ADDR || target > MAX_CLIENT_ADDR)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the read length must be a byte, word, or dword
	if (u8ReadLen != 1 && u8ReadLen != 2 && u8ReadLen != 4)
	{
		return PECI_CC_INVALID_REQ;
	}

	return peci_RdEndPointConfigPciCommon_dom(
		target, domainId, PECI_ENDPTCFG_TYPE_LOCAL_PCI, u8Seg, u8Bus, u8Device,
		u8Fcn, u16Reg, u8ReadLen, pPCIData, peci_fd, cc);
}

/*-------------------------------------------------------------------------
 * This function provides read access to PCI MMIO space at
 * the requested PCI configuration address.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigMmio(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint8_t u8Bar,
	uint8_t u8AddrType, uint64_t u64Offset,
	uint8_t u8ReadLen, uint8_t* pMmioData,
	uint8_t* cc)
{
	EPECIStatus ret;

	if (pMmioData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	//  Default to domain ID 0
	return peci_RdEndPointConfigMmio_dom(target, 0, u8Seg, u8Bus, u8Device,
					     u8Fcn, u8Bar, u8AddrType, u64Offset,
					     u8ReadLen, pMmioData, cc);
}

/*-------------------------------------------------------------------------
 * This function provides read access to PCI MMIO space at
 * the requested PCI configuration address in the specified domain.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigMmio_dom(uint8_t target, uint8_t domainId,
	uint8_t u8Seg, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint8_t u8Bar, uint8_t u8AddrType,
	uint64_t u64Offset, uint8_t u8ReadLen,
	uint8_t* pMmioData, uint8_t* cc)
{
	int peci_fd = -1;
	EPECIStatus ret = PECI_CC_SUCCESS;

	if (pMmioData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// The target address must be in the valid range
	if (target < MIN_CLIENT_ADDR || target > MAX_CLIENT_ADDR)
	{
		return PECI_CC_INVALID_REQ;
	}

	if (peci_Open(&peci_fd) != PECI_CC_SUCCESS)
	{
		return PECI_CC_DRIVER_ERR;
	}
	ret = peci_RdEndPointConfigMmio_seq_dom(
	    target, domainId, u8Seg, u8Bus, u8Device, u8Fcn, u8Bar, u8AddrType,
	    u64Offset, u8ReadLen, pMmioData, peci_fd, cc);
	peci_Close(peci_fd);
	return ret;
}

/*-------------------------------------------------------------------------
 * This function allows sequential RdEndPointConfig to PCI MMIO with the
 * provided peci file descriptor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigMmio_seq(
	uint8_t target, uint8_t u8Seg, uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint8_t u8Bar, uint8_t u8AddrType, uint64_t u64Offset,
	uint8_t u8ReadLen, uint8_t* pMmioData, int peci_fd, uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_RdEndPointConfigMmio_seq_dom(
	    target, 0, u8Seg, u8Bus, u8Device, u8Fcn, u8Bar, u8AddrType, u64Offset,
	    u8ReadLen, pMmioData, peci_fd, cc);
}

/*-------------------------------------------------------------------------
 * This function allows sequential RdEndPointConfig to PCI MMIO with the
 * provided peci file descriptor in the specified domain.
 *------------------------------------------------------------------------*/
EPECIStatus peci_RdEndPointConfigMmio_seq_dom(
	uint8_t target, uint8_t domainId, uint8_t u8Seg, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn, uint8_t u8Bar, uint8_t u8AddrType,
	uint64_t u64Offset, uint8_t u8ReadLen, uint8_t* pMmioData, int peci_fd,
	uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pMmioData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the read length must be a byte, word, dword, or qword
	if (u8ReadLen != 1 && u8ReadLen != 2 && u8ReadLen != 4 && u8ReadLen != 8)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                                        // Add 1 for completion code
	if (u8AddrType == PECI_ENDPTCFG_ADDR_TYPE_MMIO_D) {                        // Address Type 0x05
		peci_cmd->write_len = PECI_RDENDPTCFG_MMIO_D_WRITE_LEN;                // 14 bytes
	} else {                                                                   // Address Type 0x06
		peci_cmd->write_len = PECI_RDENDPTCFG_MMIO_Q_WRITE_LEN;                // 18 bytes
	}

	peci_cmd->write_buffer[0] = PECI_RDENDPTCFG_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x0;
	peci_cmd->write_buffer[2] = PECI_ENDPTCFG_TYPE_MMIO;                       // Message Type 0x05 for MMIO
	peci_cmd->write_buffer[3] = 0;                                             // End Point ID 0x00
	peci_cmd->write_buffer[4] = 0;                                             // Reserved
	peci_cmd->write_buffer[5] = u8Bar;                                         // BAR#
	peci_cmd->write_buffer[6] = u8AddrType;                                    // Address Type
	peci_cmd->write_buffer[7] = u8Seg;                                         // Segment
	peci_cmd->write_buffer[8] = (u8Fcn & 0x07) | ((u8Device & 0x1F) << 3);     // Function[2:0] Device[7:3]
	peci_cmd->write_buffer[9] = u8Bus;                                         // Bus

	// 4 or 8 bytes address
	memcpy(&peci_cmd->write_buffer[10], (uint8_t *)&u64Offset, sizeof(uint64_t));

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pMmioData, &ipmb_res->data[1], u8ReadLen);
	}

	return ret;
}

/*-------------------------------------------------------------------------
 * This function allows sequential peci_WrEndPointConfig to PCI EndPoint with
 * the provided peci file descriptor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_WrEndPointConfig_seq(uint8_t target, uint8_t u8MsgType,
	uint8_t u8Seg, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t DataLen,
	uint32_t DataVal, int peci_fd,
	uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_WrEndPointConfig_seq_dom(target, 0, u8MsgType, u8Seg, u8Bus,
	    u8Device, u8Fcn, u16Reg, DataLen,
	    DataVal, peci_fd, cc);
}

/*-------------------------------------------------------------------------
 * This function allows sequential peci_WrEndPointConfig to PCI EndPoint with
 * the provided peci file descriptor in the specified domain.
 *------------------------------------------------------------------------*/
EPECIStatus peci_WrEndPointConfig_seq_dom(uint8_t target, uint8_t domainId,
	uint8_t u8MsgType, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t DataLen, uint32_t DataVal,
	int peci_fd, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the write length must be a byte, word, or dword
	if (DataLen != 1 && DataLen != 2 && DataLen != 4)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = 1;                                                // 1 byte completion code
	peci_cmd->write_len = PECI_WRENDPTCFG_PCI_WRITE_LEN_BASE + DataLen;    // 13 bytes + DataLen

	peci_cmd->write_buffer[0] = PECI_WRENDPTCFG_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x0;
	peci_cmd->write_buffer[2] = u8MsgType;                                 // Message Type (0x03, 0x04)
	peci_cmd->write_buffer[3] = 0;                                         // End Point ID 0x00
	peci_cmd->write_buffer[4] = 0;                                         // Reserved
	peci_cmd->write_buffer[5] = 0;                                         // Reserved
	peci_cmd->write_buffer[6] = PECI_ENDPTCFG_ADDR_TYPE_PCI;               // Address Type 0x04
	peci_cmd->write_buffer[7] = u8Seg;                                     // Segment
	peci_cmd->write_buffer[8] = (uint8_t)u16Reg;                           // Register[11:0]
	peci_cmd->write_buffer[9] = u8Device << 7;                             // Device[19:15]
	peci_cmd->write_buffer[9] |= u8Fcn << 4;                               // Function[14:12]
	peci_cmd->write_buffer[9] |= (uint8_t)(u16Reg >> 8);                   // Register[11:0]
	peci_cmd->write_buffer[10] = u8Device >> 1;                            // Device[19:15]
	peci_cmd->write_buffer[10] |= u8Bus << 4;                              // Bus[27:20]
	peci_cmd->write_buffer[11] = u8Bus >> 4;                               // Bus[27:20] & Reserved[31:28]

	// 1, 2, or 4 bytes of data
	memcpy(&peci_cmd->write_buffer[12], (uint8_t *)&DataVal, DataLen);

	// FCS is calculated and invert the MSB to get AW FCS.
	peci_cmd->write_buffer[peci_cmd->write_len - 1] = calculate_fcs(peci_cmd) ^ 0x80;

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, true);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
	}

	return ret;
}

/*-------------------------------------------------------------------------
 *  This function provides write access to the EP local PCI configuration space
 *------------------------------------------------------------------------*/
EPECIStatus peci_WrEndPointPCIConfigLocal(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t DataLen, uint32_t DataVal,
	uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_WrEndPointPCIConfigLocal_dom(
	    target, 0, u8Seg, u8Bus, u8Device, u8Fcn, u16Reg, DataLen, DataVal, cc);
}

/*-------------------------------------------------------------------------
 * This function provides write access to the EP local PCI configuration space
 * in the specified domain
 *------------------------------------------------------------------------*/
EPECIStatus peci_WrEndPointPCIConfigLocal_dom(uint8_t target, uint8_t domainId,
uint8_t u8Seg, uint8_t u8Bus,
uint8_t u8Device, uint8_t u8Fcn,
uint16_t u16Reg, uint8_t DataLen,
uint32_t DataVal, uint8_t* cc)
{
	int peci_fd = -1;
	EPECIStatus ret = PECI_CC_SUCCESS;

	if (peci_Open(&peci_fd) != PECI_CC_SUCCESS)
	{
		return PECI_CC_DRIVER_ERR;
	}

	ret = peci_WrEndPointConfig_seq_dom(
	    target, domainId, PECI_ENDPTCFG_TYPE_LOCAL_PCI, u8Seg, u8Bus, u8Device,
	    u8Fcn, u16Reg, DataLen, DataVal, peci_fd, cc);
	peci_Close(peci_fd);
	return ret;
}

/*-------------------------------------------------------------------------
 *  This function provides write access to the EP local PCI configuration space
 *------------------------------------------------------------------------*/
EPECIStatus peci_WrEndPointPCIConfig(uint8_t target, uint8_t u8Seg,
	uint8_t u8Bus, uint8_t u8Device,
	uint8_t u8Fcn, uint16_t u16Reg,
	uint8_t DataLen, uint32_t DataVal,
	uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_WrEndPointPCIConfig_dom(target, 0, u8Seg, u8Bus, u8Device,
					    u8Fcn, u16Reg, DataLen, DataVal, cc);
}

/*-------------------------------------------------------------------------
 * This function provides write access to the EP local PCI configuration space
 * in the specified domain
 *------------------------------------------------------------------------*/
EPECIStatus peci_WrEndPointPCIConfig_dom(uint8_t target, uint8_t domainId,
	uint8_t u8Seg, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t DataLen,
	uint32_t DataVal, uint8_t* cc)
{
	int peci_fd = -1;
	EPECIStatus ret = PECI_CC_SUCCESS;

	if (peci_Open(&peci_fd) != PECI_CC_SUCCESS)
	{
		return PECI_CC_DRIVER_ERR;
	}
	ret = peci_WrEndPointConfig_seq_dom(
	    target, domainId, PECI_ENDPTCFG_TYPE_PCI, u8Seg, u8Bus, u8Device, u8Fcn,
	    u16Reg, DataLen, DataVal, peci_fd, cc);
	peci_Close(peci_fd);
	return ret;
}

/*-------------------------------------------------------------------------
 *  This function provides crashdump discovery data over PECI
 *------------------------------------------------------------------------*/
EPECIStatus peci_CrashDump_Discovery(uint8_t target, uint8_t subopcode,
	uint8_t param0, uint16_t param1,
	uint8_t param2, uint8_t u8ReadLen,
	uint8_t* pData, uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_CrashDump_Discovery_dom(target, 0, subopcode, param0, param1,
					    param2, u8ReadLen, pData, cc);
}

/*-------------------------------------------------------------------------
 * This function provides crashdump discovery data over PECI in the specified
 * domain
 *------------------------------------------------------------------------*/
EPECIStatus peci_CrashDump_Discovery_dom(uint8_t target, uint8_t domainId,
	uint8_t subopcode, uint8_t param0,
	uint16_t param1, uint8_t param2,
	uint8_t u8ReadLen, uint8_t* pData,
	uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the read length must be a byte, word, or qword
	if (u8ReadLen != 1 && u8ReadLen != 2 && u8ReadLen != 8)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                            // plus 1 byte completion code
	peci_cmd->write_len = PECI_CRASHDUMP_DISC_WRITE_LEN;           // 9 bytes command

	peci_cmd->write_buffer[0] = PECI_CRASHDUMP_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x0;
	peci_cmd->write_buffer[2] = PECI_CRASHDUMP_DISC_VERSION;       // Version 0x00
	peci_cmd->write_buffer[3] = PECI_CRASHDUMP_DISC_OPCODE;        // Opcode 0x01 - Discovery
	peci_cmd->write_buffer[4] = subopcode;                         // SubOpcode
	peci_cmd->write_buffer[5] = param0;                            // Parameter 0
	peci_cmd->write_buffer[6] = (uint8_t)param1;                   // Parameter 1 - low byte
	peci_cmd->write_buffer[7] = (uint8_t)(param1 >> 8);            // Parameter 1 - high byte
	peci_cmd->write_buffer[8] = param2;                            // Parameter 2

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pData, &ipmb_res->data[1], u8ReadLen);
	}

	return ret;
}

/*-------------------------------------------------------------------------
 *  This function provides crashdump GetFrame data over PECI
 *------------------------------------------------------------------------*/
EPECIStatus peci_CrashDump_GetFrame(uint8_t target, uint16_t param0,
	uint16_t param1, uint16_t param2,
	uint8_t u8ReadLen, uint8_t* pData,
	uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_CrashDump_GetFrame_dom(target, 0, param0, param1, param2,
					   u8ReadLen, pData, cc);
}

/*-------------------------------------------------------------------------
 * This function provides crashdump GetFrame data over PECI in the specified
 * domain
 *------------------------------------------------------------------------*/
EPECIStatus peci_CrashDump_GetFrame_dom(uint8_t target, uint8_t domainId,
	uint16_t param0, uint16_t param1,
	uint16_t param2, uint8_t u8ReadLen,
	uint8_t* pData, uint8_t* cc)
{
	int peci_fd = -1;
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// The target address must be in the valid range
	if (target < MIN_CLIENT_ADDR || target > MAX_CLIENT_ADDR)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the read length must be a qword or dqword
	if (u8ReadLen != 8 && u8ReadLen != 16)
	{
		return PECI_CC_INVALID_REQ;
	}

	if (peci_Open(&peci_fd) != PECI_CC_SUCCESS)
	{
		return PECI_CC_DRIVER_ERR;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                            // plus 1 byte completion code
	peci_cmd->write_len = PECI_CRASHDUMP_GET_FRAME_WRITE_LEN;      // 10 bytes command

	peci_cmd->write_buffer[0] = PECI_CRASHDUMP_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x00;
	peci_cmd->write_buffer[2] = PECI_CRASHDUMP_GET_FRAME_VERSION;  // Version 0x00
	peci_cmd->write_buffer[3] = PECI_CRASHDUMP_GET_FRAME_OPCODE;   // Opcode 0x03 - GetFrame
	peci_cmd->write_buffer[4] = (uint8_t)param0;                   // Parameter 0 - low byte
	peci_cmd->write_buffer[5] = (uint8_t)(param0 >> 8);            // Parameter 0 - high byte
	peci_cmd->write_buffer[6] = (uint8_t)param1;                   // Parameter 1 - low byte
	peci_cmd->write_buffer[7] = (uint8_t)(param1 >> 8);            // Parameter 1 - high byte
	peci_cmd->write_buffer[8] = (uint8_t)param2;                   // Parameter 2 - low byte
	peci_cmd->write_buffer[9] = (uint8_t)(param2 >> 8);            // Parameter 2 - high byte

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pData, &ipmb_res->data[1], u8ReadLen);
	}

	peci_Close(peci_fd);

	return ret;
}

/*-------------------------------------------------------------------------
+ * This function provides telemetry discovery data over PECI
+ *------------------------------------------------------------------------*/
EPECIStatus peci_Telemetry_Discovery(uint8_t target, uint8_t subopcode,
	uint8_t param0, uint16_t param1,
	uint8_t param2, uint8_t u8ReadLen,
	uint8_t* pData, uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_Telemetry_Discovery_dom(target, 0, subopcode, param0, param1,
					    param2, u8ReadLen, pData, cc);
}

/*-------------------------------------------------------------------------
+ * This function provides telemetry discovery data over PECI in the specified
+ * domain
+ *------------------------------------------------------------------------*/
EPECIStatus peci_Telemetry_Discovery_dom(uint8_t target, uint8_t domainId,
	uint8_t subopcode, uint8_t param0,
	uint16_t param1, uint8_t param2,
	uint8_t u8ReadLen, uint8_t* pData,
	uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pData == NULL || cc == NULL) {
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                        // plus 1 byte completion code
	peci_cmd->write_len = PECI_TELEMETRY_DISC_WRITE_LEN;       // 9 bytes command

	peci_cmd->write_buffer[0] = PECI_TELEMETRY_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x0;
	peci_cmd->write_buffer[2] = PECI_TELEMETRY_DISC_VERSION;   // Version 0x00
	peci_cmd->write_buffer[3] = PECI_TELEMETRY_DISC_OPCODE;    // Opcode 0x01 - Discovery
	peci_cmd->write_buffer[4] = subopcode;                     // SubOpcode
	peci_cmd->write_buffer[5] = param0;                        // Parameter 0
	peci_cmd->write_buffer[6] = (uint8_t)param1;               // Parameter 1 - low byte
	peci_cmd->write_buffer[7] = (uint8_t)(param1 >> 8);        // Parameter 1 - high byte
	peci_cmd->write_buffer[8] = param2;                        // Parameter 2

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS) {
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pData, &ipmb_res->data[1], u8ReadLen);
	}

	return ret;
}

/*-------------------------------------------------------------------------
 *  This function provides telemetry sample data over PECI
 *------------------------------------------------------------------------*/
EPECIStatus peci_Telemetry_GetTelemSample(uint8_t target, uint16_t index,
    uint16_t sample, uint8_t u8ReadLen,
    uint8_t* pData, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pData == NULL || cc == NULL) {
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = PECI_TELEMETRY_GET_TELEM_SAMPLE_READ_LEN;         // 9 bytes, include completion code
	peci_cmd->write_len = PECI_TELEMETRY_GET_TELEM_SAMPLE_WRITE_LEN;       // 8 bytes command

	peci_cmd->write_buffer[0] = PECI_TELEMETRY_CMD;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = PECI_TELEMETRY_GET_TELEM_SAMPLE_VERSION;   // Version 0x00
	peci_cmd->write_buffer[3] = PECI_TELEMETRY_GET_TELEM_SAMPLE_OPCODE;    // Opcode 0x02 - GetTelemSample
	peci_cmd->write_buffer[4] = (uint8_t)index;                            // Index - low byte
	peci_cmd->write_buffer[5] = (uint8_t)(index >> 8);                     // Index - high byte
	peci_cmd->write_buffer[6] = (uint8_t)sample;                           // Sample - low byte
	peci_cmd->write_buffer[7] = (uint8_t)(sample >> 8);                    // Sample - high byte

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS) {
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pData, &ipmb_res->data[1], u8ReadLen);
	}

    return ret;
}

/*-------------------------------------------------------------------------
 *  This function provides config watcher read over PECI
 *------------------------------------------------------------------------*/
EPECIStatus peci_Telemetry_ConfigWatcherRd(uint8_t target, uint16_t watcher,
	uint16_t offset, uint8_t u8ReadLen,
	uint8_t* pData, uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_Telemetry_ConfigWatcherRd_dom(target, 0, watcher, offset,
						  u8ReadLen, pData, cc);
}

/*-------------------------------------------------------------------------
+ * This function provides config watcher read over PECI in the specified domain
+ *------------------------------------------------------------------------*/
EPECIStatus peci_Telemetry_ConfigWatcherRd_dom(uint8_t target, uint8_t domainId,
	uint16_t watcher,
	uint16_t offset,
	uint8_t u8ReadLen,
	uint8_t* pData, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pData == NULL || cc == NULL) {
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = PECI_TELEMETRY_CONFIG_WATCHER_RD_READ_LEN;        // 9 bytes, include completion code
	peci_cmd->write_len = PECI_TELEMETRY_CONFIG_WATCHER_RD_WRITE_LEN;      // 9 bytes

	peci_cmd->write_buffer[0] = PECI_TELEMETRY_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x0;
	peci_cmd->write_buffer[2] = PECI_TELEMETRY_CONFIG_WATCHER_VERSION;     // Version 0x00
	peci_cmd->write_buffer[3] = PECI_TELEMETRY_CONFIG_WATCHER_OPCODE;      // Opcode 0x03 - ConfigWatcher
	peci_cmd->write_buffer[4] = PECI_TELEMETRY_CONFIG_WATCHER_RD_PARAM;    // 0 - Read
	peci_cmd->write_buffer[5] = (uint8_t)watcher;                          // Watcher - low byte
	peci_cmd->write_buffer[6] = (uint8_t)(watcher >> 8);                   // Watcher - high byte
	peci_cmd->write_buffer[7] = (uint8_t)offset;                           // Offset - low byte
	peci_cmd->write_buffer[8] = (uint8_t)(offset >> 8);                    // Offset - high byte

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS) {
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pData, &ipmb_res->data[1], u8ReadLen);
	}

	return ret;
}

/*-------------------------------------------------------------------------
 * This function provides config watcher write over PECI
 *------------------------------------------------------------------------*/
EPECIStatus peci_Telemetry_ConfigWatcherWr(uint8_t target, uint16_t watcher,
	uint16_t offset, uint8_t u8DataLen,
	uint8_t* pData, uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_Telemetry_ConfigWatcherWr_dom(target, 0, watcher, offset,
						  u8DataLen, pData, cc);
}

/*-------------------------------------------------------------------------
+ * This function provides config watcher write over PECI in the specified domain
+ *------------------------------------------------------------------------*/
EPECIStatus peci_Telemetry_ConfigWatcherWr_dom(uint8_t target, uint8_t domainId,
	uint16_t watcher,
	uint16_t offset,
	uint8_t u8DataLen,
	uint8_t* pData, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pData == NULL || cc == NULL) {
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = 1;                                                // 1 byte completion code
	peci_cmd->write_len = PECI_TELEMETRY_CONFIG_WATCHER_WR_WRITE_LEN;      // 17 bytes

	peci_cmd->write_buffer[0] = PECI_TELEMETRY_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x0;
	peci_cmd->write_buffer[2] = PECI_TELEMETRY_CONFIG_WATCHER_VERSION;     // Version 0x00
	peci_cmd->write_buffer[3] = PECI_TELEMETRY_CONFIG_WATCHER_OPCODE;      // Opcode 0x03 - ConfigWatcher
	peci_cmd->write_buffer[4] = PECI_TELEMETRY_CONFIG_WATCHER_WR_PARAM;    // 0x01 - Write
	peci_cmd->write_buffer[5] = (uint8_t)watcher;                          // Watcher - low byte
	peci_cmd->write_buffer[6] = (uint8_t)(watcher >> 8);                   // Watcher - high byte
	peci_cmd->write_buffer[7] = (uint8_t)offset;                           // Offset - low byte
	peci_cmd->write_buffer[8] = (uint8_t)(offset >> 8);                    // Offset - high byte
	memcpy(&peci_cmd->write_buffer[9], pData, u8DataLen);

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS) {
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
	} else {
		/*
		* WORKAROUND:
		* For SPR D0 and earlier, the config watcher write command
		* incorrectly assumes that the last byte of data contains
		* an AW FCS. For those SPR revisions, the command will fail
		* with -EIO as the return.  In that case, we need to replace
		* the last byte with an AW FCS and try again for the command
		* to succeed.
		*/
		peci_cmd->write_buffer[peci_cmd->write_len - 1] = calculate_fcs(peci_cmd) ^ 0x80;
		ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, true);
		if (ret == PECI_CC_IPMB_SUCCESS) {
			// PECI completion code in the 1st byte
			*cc = ipmb_res->data[0];
		}
	}

	return ret;
}



/*-------------------------------------------------------------------------
 *  This function provides telemetry crashlog sample data over PECI
 *------------------------------------------------------------------------*/
EPECIStatus peci_Telemetry_GetCrashlogSample(uint8_t target, uint16_t index,
	uint16_t sample, uint8_t u8ReadLen,
	uint8_t* pData, uint8_t* cc)
{
	//  Default to domain ID 0
	return peci_Telemetry_GetCrashlogSample_dom(target, 0, index, sample,
						    u8ReadLen, pData, cc);
}

/*-------------------------------------------------------------------------
+ * This function provides telemetry crashlog sample data over PECI in the
+ * specified domain
+ *------------------------------------------------------------------------*/
EPECIStatus peci_Telemetry_GetCrashlogSample_dom(
	uint8_t target, uint8_t domainId, uint16_t index, uint16_t sample,
	uint8_t u8ReadLen, uint8_t* pData, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pData == NULL || cc == NULL) {
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = (peci_cmd_t *)(ipmb_req->data + IANA_SIZE);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = PECI_TELEMETRY_GET_CRASHLOG_SAMPLE_READ_LEN;          // 9 bytes, include completion code
	peci_cmd->write_len = PECI_TELEMETRY_GET_CRASHLOG_SAMPLE_WRITE_LEN;        // 8 bytes

	peci_cmd->write_buffer[0] = PECI_TELEMETRY_CMD;
	peci_cmd->write_buffer[1] = (domainId << 1) | 0x0;
	peci_cmd->write_buffer[2] = PECI_TELEMETRY_GET_CRASHLOG_SAMPLE_VERSION;    // Version 0x00
	peci_cmd->write_buffer[3] = PECI_TELEMETRY_GET_CRASHLOG_SAMPLE_OPCODE;     // Opcode 0x0C - GetCrashLogSample
	peci_cmd->write_buffer[4] = (uint8_t)index;                                // Index - low byte
	peci_cmd->write_buffer[5] = (uint8_t)(index >> 8);                         // Index - high byte
	peci_cmd->write_buffer[6] = (uint8_t)sample;                               // Sample - low byte
	peci_cmd->write_buffer[7] = (uint8_t)(sample >> 8);                        // Sample - high byte

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res, true, false);
	if (ret == PECI_CC_IPMB_SUCCESS) {
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pData, &ipmb_res->data[1], u8ReadLen);
	}

    return ret;
}

/*-------------------------------------------------------------------------
 * This function returns the CPUID (Model and stepping) for the given PECI
 * client address
 *------------------------------------------------------------------------*/
EPECIStatus peci_GetCPUID(const uint8_t clientAddr, CPUModel* cpuModel,
	uint8_t* stepping, uint8_t* cc)
{
	EPECIStatus ret;
	uint32_t cpuid = 0;

	if (cpuModel == NULL || stepping == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	if (peci_Ping(clientAddr) != PECI_CC_SUCCESS)
	{
		return PECI_CC_CPU_NOT_PRESENT;
	}

	ret = peci_RdPkgConfig(clientAddr, PECI_MBX_INDEX_CPU_ID, PECI_PKG_ID_CPU_ID,
			sizeof(uint32_t), (uint8_t*)&cpuid, cc);

	// Separate out the model and stepping (bits 3:0) from the CPUID
	*cpuModel = cpuid & 0xFFFFFFF0;
	*stepping = (uint8_t)(cpuid & 0x0000000F);

	return ret;
}
