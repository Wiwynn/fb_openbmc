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

#include <stdio.h>
#include <string.h>
#include <openbmc/ipmi.h>
#include <openbmc/ipmb.h>

#include "ipmb_peci_interface.h"

#define PECI_HOST_ID          0x00
#define NODE_IPMB_SLAVE_ADDR  0x20
#define CMD_SEND_RAW_PECI     0x29
#define MAX_PECI_RETRY   50
#define RDIAMSR_READ_LEN 8
#define RDIAMSR_WRITE_LEN 5

EPECIStatus peci_GetDIB_seq(uint8_t target, uint64_t* dib);

int node_bus_id;  // bus# for compute node

EPECIStatus peci_Lock(int* peci_fd, int timeout_ms)
{
	return PECI_CC_SUCCESS;
}

void peci_Unlock(int peci_fd)
{
	return;
}

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

uint8_t CalculateCRC8(uint8_t *data, size_t len)
{
    uint8_t crc = 0;

    crc = pec_calculate(crc, data, len);

    return crc;

}

/*-------------------------------------------------------------------------
 * This function calculates the PECI FCS
 *------------------------------------------------------------------------*/
static unsigned char calculate_fcs(peci_cmd_t *peci_cmd)
{
	unsigned char tbuf[PECI_DATA_BUF_SIZE + 3];

	tbuf[0] = peci_cmd->target;
	tbuf[1] = peci_cmd->write_len;
	tbuf[2] = peci_cmd->read_len;
	memcpy(&tbuf[3], peci_cmd->write_buffer, peci_cmd->write_len - 1);

	return CalculateCRC8(tbuf, peci_cmd->write_len + 2);
}

/*-------------------------------------------------------------------------
 * This function issues peci commands to PECI bridge device
 *------------------------------------------------------------------------*/
static EPECIStatus IPMB_peci_issue_cmd(ipmb_req_t *req, ipmb_res_t *res)
{
	int rlen;
	int tlen;
	peci_cmd_t *peci_cmd;
	EPECIStatus  ret = PECI_CC_IPMB_ERR;
        uint8_t iana[3] = {0x9c, 0x9c, 0x0};
        uint8_t data_tmp[PECI_DATA_BUF_SIZE] = {0};
	int retry = 0;

	if (NULL == req || NULL == res)
	{
		return PECI_CC_INVALID_REQ;
	}

        memset(data_tmp, 0, sizeof(data_tmp));
	memcpy(data_tmp, req->data, sizeof(data_tmp));
	memset(req->data, 0, sizeof(data_tmp));
	memcpy(req->data, iana, sizeof(iana));
        memcpy(&req->data[3], data_tmp, sizeof(data_tmp));

	// add IPMB header to the compute node IPMB interface
	req->res_slave_addr = NODE_IPMB_SLAVE_ADDR << 1;
	req->netfn_lun = NETFN_OEM_1S_REQ << LUN_OFFSET;
	req->cmd = CMD_SEND_RAW_PECI;           // TODO: need define an OEM command for PECI raw over IPMB

	req->hdr_cksum = req->res_slave_addr + req->netfn_lun;
	req->hdr_cksum = ZERO_CKSUM_CONST - req->hdr_cksum;

	req->req_slave_addr = BMC_SLAVE_ADDR << 1;
	req->seq_lun = 0x00;

	peci_cmd = &(data_tmp[0]);

        tlen = peci_cmd->write_len + 6 + MIN_IPMB_REQ_LEN;
	rlen = ipmb_send_buf(node_bus_id, tlen);
	if (rlen >= MIN_IPMB_RES_LEN)
	{
		ret = PECI_CC_IPMB_SUCCESS;
	}
	else
	{
		if (rlen == MIN_IPMB_RES_LEN && peci_cmd->read_len == 0)
		{
			ret = PECI_CC_IPMB_SUCCESS;
		}
		else
		{
			printf("IPMB ERROR, response length = %d\n", rlen);
			return PECI_CC_IPMB_ERR;
		}
	}


        //Remove return iana code
	memset(data_tmp, 0, sizeof(data_tmp));
	memcpy(data_tmp, &(res->data[3]), rlen-3);
	memset(res->data, 0, rlen);
	memcpy(res->data, data_tmp, rlen-3);

	return ret;
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
	uint8_t data[4] = {0x30, 0x0, 0x0, 0xe1};

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	memcpy(ipmb_req->data, data, sizeof(data));
	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	//No CC returned for Ping.

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
	peci_cmd_t	*peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;
	uint8_t     cc;

	if (dib == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = 9;
	peci_cmd->write_len = 1;
	peci_cmd->write_buffer[0] = PECI_GET_DIB;

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		cc = ipmb_res->data[0];

		if (cc == PECI_DEV_CC_SUCCESS)
		{
			memcpy((uint8_t*)dib, (uint8_t*)&(ipmb_res->data[1]), peci_cmd->read_len - 1);
		}
		else
		{
			ret = cc;
		}
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
	peci_cmd_t	*peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;
	uint8_t     cc;

	if (temperature == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = sizeof(int16_t) + 1;
	peci_cmd->write_len = 1;
	// peci_cmd->write_buffer[0] = PECI_GET_TEMP | (0x01 << peci_cmd->target);
	peci_cmd->write_buffer[0] = PECI_GET_TEMP | 0x01;

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		cc = ipmb_res->data[0];

		if (cc == PECI_DEV_CC_SUCCESS)
		{
			memcpy((uint8_t*)temperature, (uint8_t*)&(ipmb_res->data[1]), sizeof(int16_t));
		}
		else
		{
			ret = cc;
		}
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
	int peci_fd = -1;

	if (pPkgConfig == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ret = peci_RdPkgConfig_seq(target, u8Index, u16Value, u8ReadLen, pPkgConfig, peci_fd, cc);

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
	EPECIStatus ret;
	peci_cmd_t *peci_cmd;
	ipmb_req_t *ipmb_req;
	ipmb_res_t *ipmb_res;

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
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                       // Add 1 for completion code.
	peci_cmd->write_len = 5;

	peci_cmd->write_buffer[0] = PECI_RDPKG | 0x01;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = u8Index;                      // RdPkgConfig index
	peci_cmd->write_buffer[3] = (uint8_t)u16Value;            // Config parameter value - low byte
	peci_cmd->write_buffer[4] = (uint8_t)(u16Value >> 8);     // Config parameter value - high byte

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pPkgConfig, (uint8_t*)&(ipmb_res->data[1]), u8ReadLen);

		if (*cc != PECI_DEV_CC_SUCCESS && *cc != PECI_DEV_CC_FATAL_MCA_DETECTED)
		{
			ret = PECI_CC_HW_ERR;
		}
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
	int peci_fd = -1;

	if (cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ret = peci_WrPkgConfig_seq(target, u8Index, u16Param, u32Value, u8WriteLen, peci_fd, cc);

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
	peci_cmd_t	*peci_cmd;
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
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = 1;                                 // 1 byte Completion Code
	peci_cmd->write_len = u8WriteLen + 6;                   // 5 [1:cmd code] [1:host_id]
										                   // [1:index] [2:parameter] [1:AW FCS]
	//peci_cmd->write_buffer[0] = PECI_WRPKG | (0x01 << peci_cmd->target);
	peci_cmd->write_buffer[0] = PECI_WRPKG | 0x01;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = u8Index;                    // WrPkgConfig index
	peci_cmd->write_buffer[3] = (uint8_t)u16Param;          // parameter value - low byte
	peci_cmd->write_buffer[4] = (uint8_t)(u16Param >> 8);   // parameter value - high byte
	peci_cmd->write_buffer[5] = (uint8_t)u32Value;
	peci_cmd->write_buffer[6] = (uint8_t)(u32Value >> 8);
	peci_cmd->write_buffer[7] = (uint8_t)(u32Value >> 16);
	peci_cmd->write_buffer[8] = (uint8_t)(u32Value >> 24);

	// FCS is calculated and invert the MSB to get AW FCS.
	peci_cmd->write_buffer[peci_cmd->write_len - 1] = calculate_fcs(peci_cmd) ^ 0x80;

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];

		if (*cc != PECI_DEV_CC_SUCCESS && *cc != PECI_DEV_CC_FATAL_MCA_DETECTED)
		{
			ret = PECI_CC_HW_ERR;
		}
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
	EPECIStatus ret;
	peci_cmd_t	*peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;
	int retry = 0;

	if (u64MsrVal == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}
	for (retry = 0; retry < MAX_PECI_RETRY; retry++) {

		ipmb_req = ipmb_txb();
		ipmb_res = ipmb_rxb();
		peci_cmd = &(ipmb_req->data[0]);
		memset(peci_cmd, 0, sizeof(peci_cmd_t));

		peci_cmd->target = target;
		peci_cmd->read_len = RDIAMSR_READ_LEN + 1;                                // Add 1 for completion code.
		peci_cmd->write_len = RDIAMSR_WRITE_LEN;

		peci_cmd->write_buffer[0] = PECI_RDIAMSR | 0x01;
		peci_cmd->write_buffer[1] = PECI_HOST_ID;
		peci_cmd->write_buffer[2] = threadID;                      // request byte for thread ID
		peci_cmd->write_buffer[3] = (uint8_t)(MSRAddress & 0x00ff);           // MSR Address - low byte
		peci_cmd->write_buffer[4] = (uint8_t)((MSRAddress & 0xff00) >> 8);    // MSR Address - high byte
		if (retry != 0) {
			peci_cmd->write_buffer[1] = PECI_HOST_ID | 1;                 // set retry bit (bit 0) to declare this is retry command
		}
		ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);
		if (ipmb_res->data[0] != PECI_DEV_CC_NEED_RETRY) {
			break;
		}
	}
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy((uint8_t*)u64MsrVal, (uint8_t*)&(ipmb_res->data[1]), 8);

        if (*cc != PECI_DEV_CC_SUCCESS && *cc != PECI_DEV_CC_FATAL_MCA_DETECTED
		&& *cc != PECI_DEV_CC_CATASTROPHIC_MCA_ERROR)
        {
            ret = PECI_CC_HW_ERR;
        }
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
	int peci_fd = -1;

	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ret = peci_RdPCIConfig_seq(target, u8Bus, u8Device, u8Fcn, u16Reg, pPCIData, peci_fd, cc);

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
	peci_cmd_t	*peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ipmb_req = ipmb_txb();
	ipmb_res = ipmb_rxb();
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = 4 + 1;                                                     // Add 1 for completion code.
	peci_cmd->write_len = 6;

	//peci_cmd->write_buffer[0] = PECI_RDPCICONFIG | (0x01 << peci_cmd->target);
	peci_cmd->write_buffer[0] = PECI_RDPCICONFIG | 0x01;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = (uint8_t)u16Reg;		                            // Register[11:0]
	peci_cmd->write_buffer[3] = u8Device << 7;                                      // Device[19:15]
	peci_cmd->write_buffer[3] |= u8Fcn << 4;                                        // Function[14:12]
	peci_cmd->write_buffer[3] |= (uint8_t)(u16Reg >> 8);                            // Register[11:0]
	peci_cmd->write_buffer[4] = u8Device >> 1;                                      // Device[19:15]
	peci_cmd->write_buffer[4] |= u8Bus << 4;                                        // Bus[27:20]
	peci_cmd->write_buffer[5] = u8Bus >> 4;	                                        // Bus[27:20] & Reserved[31:28]

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pPCIData, (uint8_t*)&(ipmb_res->data[1]), 4);

        if (*cc != PECI_DEV_CC_SUCCESS && *cc != PECI_DEV_CC_FATAL_MCA_DETECTED)
        {
            ret = PECI_CC_HW_ERR;
        }
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
	int peci_fd = -1;

	if (pPCIReg == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ret = peci_RdPCIConfigLocal_seq(target, u8Bus, u8Device, u8Fcn, u16Reg,
		u8ReadLen, pPCIReg, peci_fd, cc);

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
	EPECIStatus ret;
	peci_cmd_t	*peci_cmd;
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
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                                            // Add 1 for completion code.
	peci_cmd->write_len = 5;

	//peci_cmd->write_buffer[0] = PECI_RDPCICONFIGLOCAL | (0x01 << peci_cmd->target);
	peci_cmd->write_buffer[0] = PECI_RDPCICONFIGLOCAL | 0x01;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = (uint8_t)u16Reg;		                            // Register[11:0]
	peci_cmd->write_buffer[3] = u8Device << 7;                                      // Device[19:15]
	peci_cmd->write_buffer[3] |= u8Fcn << 4;                                        // Function[14:12]
	peci_cmd->write_buffer[3] |= (uint8_t)(u16Reg >> 8);                            // Register[11:0]
	peci_cmd->write_buffer[4] = u8Device >> 1;                                      // Device[19:15]
	peci_cmd->write_buffer[4] |= u8Bus << 4;                                        // Bus[23:20]

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pPCIReg, (uint8_t *)&(ipmb_res->data[1]), u8ReadLen);

        if (*cc != PECI_DEV_CC_SUCCESS && *cc != PECI_DEV_CC_FATAL_MCA_DETECTED)
        {
            ret = PECI_CC_HW_ERR;
        }
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
	peci_cmd_t	*peci_cmd;
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
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = 1;                   // Completion Code
	peci_cmd->write_len = DataLen + 5;        // 5 [1:cmd code] [1:host_id]
						 // [1:index] [2:parameter] [1:AW FCS]
	peci_cmd->write_buffer[0] = PECI_WRPCICONFIGLOCAL | 0x01;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = (uint8_t)u16Reg;		                            // Register[11:0]
	peci_cmd->write_buffer[3] = u8Device << 7;                                      // Device[19:15]
	peci_cmd->write_buffer[3] |= u8Fcn << 4;                                        // Function[14:12]
	peci_cmd->write_buffer[3] |= (uint8_t)(u16Reg >> 8);                            // Register[11:0]
	peci_cmd->write_buffer[4] = u8Device >> 1;                                      // Device[19:15]
	peci_cmd->write_buffer[4] |= u8Bus << 4;                                        // Bus[23:20]

	memcpy((uint8_t *)&(peci_cmd->write_buffer[5]), (uint8_t *)&DataVal, DataLen);

	// FCS is calculated and invert the MSB to get AW FCS.
	peci_cmd->write_buffer[peci_cmd->write_len - 1] = calculate_fcs(peci_cmd) ^ 0x80;

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];

        if (*cc != PECI_DEV_CC_SUCCESS && *cc != PECI_DEV_CC_FATAL_MCA_DETECTED)
        {
            ret = PECI_CC_HW_ERR;
        }
	}

	return ret;
}

/*-------------------------------------------------------------------------
 * This internal function is the common interface for RdEndPointConfig to PCI
 *------------------------------------------------------------------------*/
static EPECIStatus peci_RdEndPointConfigPciCommon(
	uint8_t target, uint8_t u8MsgType, uint8_t u8Seg, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn, uint16_t u16Reg, uint8_t u8ReadLen,
	uint8_t* pPCIData, int peci_fd, uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t	*peci_cmd;
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
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                                             // Add 1 for completion code.
	peci_cmd->write_len = 12;                                                       // 12 bytes command

	peci_cmd->write_buffer[0] = PECI_RD_END_PT_CFG | 0x01;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = u8MsgType;                                          // Message Type (0x03, 0x04)
	peci_cmd->write_buffer[3] = 0;                                                  // End Point ID 0x00
	peci_cmd->write_buffer[4] = 0;                                                  // Reserved
	peci_cmd->write_buffer[5] = 0;                                                  // Reserved
	peci_cmd->write_buffer[6] = 0x04;                                               // Address Type 0x04
	peci_cmd->write_buffer[7] = u8Seg;                                              // Segment
	peci_cmd->write_buffer[8] = (uint8_t)u16Reg;		                               // Register[11:0]
	peci_cmd->write_buffer[9] = u8Device << 7;                                      // Device[19:15]
	peci_cmd->write_buffer[9] |= u8Fcn << 4;                                        // Function[14:12]
	peci_cmd->write_buffer[9] |= (uint8_t)(u16Reg >> 8);                            // Register[11:0]
	peci_cmd->write_buffer[10] = u8Device >> 1;                                     // Device[19:15]
	peci_cmd->write_buffer[10] |= u8Bus << 4;                                       // Bus[27:20]
	peci_cmd->write_buffer[11] = u8Bus >> 4;	                                       // Bus[27:20] & Reserved[31:28]

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pPCIData, (uint8_t *)&(ipmb_res->data[1]), u8ReadLen);

        if (*cc != PECI_DEV_CC_SUCCESS && *cc != PECI_DEV_CC_FATAL_MCA_DETECTED)
        {
            ret = PECI_CC_HW_ERR;
        }
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
	EPECIStatus ret;
	int peci_fd = -1;

	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ret = peci_RdEndPointConfigPci_seq(target, u8Seg, u8Bus, u8Device, u8Fcn,
			u16Reg, u8ReadLen, pPCIData, peci_fd, cc);

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
	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the read length must be a byte, word, or dword
	if (u8ReadLen != 1 && u8ReadLen != 2 && u8ReadLen != 4)
	{
		return PECI_CC_INVALID_REQ;
	}

	return peci_RdEndPointConfigPciCommon(target, PECI_ENDPTCFG_TYPE_PCI, u8Seg,
		u8Bus, u8Device, u8Fcn, u16Reg,
		u8ReadLen, pPCIData, peci_fd, cc);
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
	EPECIStatus ret;
	int peci_fd = -1;

	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ret = peci_RdEndPointConfigPciLocal_seq(target, u8Seg, u8Bus, u8Device,
		u8Fcn, u16Reg, u8ReadLen, pPCIData, peci_fd, cc);

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
	if (pPCIData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the read length must be a byte, word, or dword
	if (u8ReadLen != 1 && u8ReadLen != 2 && u8ReadLen != 4)
	{
		return PECI_CC_INVALID_REQ;
	}

	return peci_RdEndPointConfigPciCommon(target, PECI_ENDPTCFG_TYPE_LOCAL_PCI,
		u8Seg, u8Bus, u8Device, u8Fcn, u16Reg,
		u8ReadLen, pPCIData, peci_fd, cc);
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
	int peci_fd = -1;

	if (pMmioData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	ret = peci_RdEndPointConfigMmio_seq(target, u8Seg, u8Bus, u8Device, u8Fcn,
		u8Bar, u8AddrType, u64Offset, u8ReadLen,
		pMmioData, peci_fd, cc);

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
	EPECIStatus ret;
	peci_cmd_t	*peci_cmd;
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
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                                             // Add 1 for completion code.
	peci_cmd->write_len = 0x12;                                                     // 0x12 bytes command

	// peci_cmd->write_buffer[0] = PECI_RD_END_PT_CFG | (0x01 << peci_cmd->target);
	peci_cmd->write_buffer[0] = PECI_RD_END_PT_CFG | 0x01;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = 0x05;                                               // Message Type 0x05 for MMIO
	peci_cmd->write_buffer[3] = 0;                                                  // End Point ID 0x00
	peci_cmd->write_buffer[4] = 0;                                                  // Reserved
	peci_cmd->write_buffer[5] = u8Bar;                                              // BAR#
	peci_cmd->write_buffer[6] = u8AddrType;                                         // Address Type
	peci_cmd->write_buffer[7] = u8Seg;                                              // Segment
	peci_cmd->write_buffer[8] = (u8Fcn & 0x07) | ((u8Device & 0x1F) << 3);          // Function[2:0] Device[7:3]
	peci_cmd->write_buffer[9] = u8Bus;                                              // Bus

	// 4 or 8 bytes address
	memcpy((uint8_t*)&(peci_cmd->write_buffer[10]), (uint8_t*)&u64Offset, sizeof(uint64_t));

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pMmioData, (uint8_t*)&(ipmb_res->data[1]), u8ReadLen);

        if (*cc != PECI_DEV_CC_SUCCESS && *cc != PECI_DEV_CC_FATAL_MCA_DETECTED)
        {
            ret = PECI_CC_HW_ERR;
        }
	}

	return ret;
}

/*-------------------------------------------------------------------------
 * This function allows sequential peci_WrEndPointConfig to PCI EndPoint with
 *the provided peci file descriptor.
 *------------------------------------------------------------------------*/
EPECIStatus peci_WrEndPointConfig_seq(uint8_t target, uint8_t u8MsgType,
	uint8_t u8Seg, uint8_t u8Bus,
	uint8_t u8Device, uint8_t u8Fcn,
	uint16_t u16Reg, uint8_t DataLen,
	uint32_t DataVal, int peci_fd,
	uint8_t* cc)
{
	EPECIStatus ret;
	peci_cmd_t	*peci_cmd;
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
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = 1;                                                         // 1 byte completion code.
	peci_cmd->write_len = 12 + DataLen;                                             // 12 bytes + DataLen

	// peci_cmd->write_buffer[0] = PECI_WR_END_PT_CFG | (0x01 << peci_cmd->target);
	peci_cmd->write_buffer[0] = PECI_WR_END_PT_CFG | 0x01;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = u8MsgType;                                          // Message Type (0x03, 0x04)
	peci_cmd->write_buffer[3] = 0;                                                  // End Point ID 0x00
	peci_cmd->write_buffer[4] = 0;                                                  // Reserved
	peci_cmd->write_buffer[5] = 0;                                                  // Reserved
	peci_cmd->write_buffer[6] = 0x04;                                               // Address Type 0x04
	peci_cmd->write_buffer[7] = u8Seg;                                              // Segment
	peci_cmd->write_buffer[8] = (uint8_t)u16Reg;		                               // Register[11:0]
	peci_cmd->write_buffer[9] = u8Device << 7;                                      // Device[19:15]
	peci_cmd->write_buffer[9] |= u8Fcn << 4;                                        // Function[14:12]
	peci_cmd->write_buffer[9] |= (uint8_t)(u16Reg >> 8);                            // Register[11:0]
	peci_cmd->write_buffer[10] = u8Device >> 1;                                     // Device[19:15]
	peci_cmd->write_buffer[10] |= u8Bus << 4;                                       // Bus[27:20]
	peci_cmd->write_buffer[11] = u8Bus >> 4;	                                       // Bus[27:20] & Reserved[31:28]

	// 1, 2, or 4 bytes of data
	memcpy((uint8_t*)&(peci_cmd->write_buffer[12]), (uint8_t*)&DataVal, DataLen);

	// FCS is calculated and invert the MSB to get AW FCS.
	peci_cmd->write_buffer[peci_cmd->write_len - 1] = calculate_fcs(peci_cmd) ^ 0x80;

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];

        if (*cc != PECI_DEV_CC_SUCCESS && *cc != PECI_DEV_CC_FATAL_MCA_DETECTED)
        {
            ret = PECI_CC_HW_ERR;
        }
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
	int peci_fd = -1;
	EPECIStatus ret;

	ret = peci_WrEndPointConfig_seq(target, PECI_ENDPTCFG_TYPE_LOCAL_PCI, u8Seg,
		u8Bus, u8Device, u8Fcn, u16Reg, DataLen,
		DataVal, peci_fd, cc);

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
	EPECIStatus ret;
	int peci_fd = -1;

	ret = peci_WrEndPointConfig_seq(target, PECI_ENDPTCFG_TYPE_PCI, u8Seg,
		u8Bus, u8Device, u8Fcn, u16Reg, DataLen,
		DataVal, peci_fd, cc);

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
	EPECIStatus ret;
	peci_cmd_t	*peci_cmd;
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
	peci_cmd = &(ipmb_req->data[0]);
	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	peci_cmd->target = target;
	peci_cmd->read_len = u8ReadLen + 1;                                             // plus 1 byte completion code.
	peci_cmd->write_len = 9;                                                        // 9 bytes command

	// peci_cmd->write_buffer[0] = PECI_CRASHDUMP | (0x01 << peci_cmd->target);
	peci_cmd->write_buffer[0] = PECI_CRASHDUMP | 0x01;
	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	peci_cmd->write_buffer[2] = 0;                                                  // Version 0x00
	peci_cmd->write_buffer[3] = 0x01;                                               // Opcode 0x01 - Discovery
	peci_cmd->write_buffer[4] = subopcode;                                          // SubOpcode
	peci_cmd->write_buffer[5] = param0;                                             // Parameter 0
	peci_cmd->write_buffer[6] = (uint8_t)(param1 & 0x00ff);                         // Parameter 1 - low byte
	peci_cmd->write_buffer[7] = (uint8_t)((param1 & 0xff00) >> 8);                  // Parameter 1 - high byte
	peci_cmd->write_buffer[8] = param2;                                             // Parameter 2

	ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);

	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pData, (uint8_t*)&(ipmb_res->data[1]), u8ReadLen);

        if (*cc != PECI_DEV_CC_SUCCESS && *cc != PECI_DEV_CC_FATAL_MCA_DETECTED)
        {
            ret = PECI_CC_HW_ERR;
        }
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
	EPECIStatus ret;
	peci_cmd_t  *peci_cmd;
	ipmb_req_t  *ipmb_req;
	ipmb_res_t  *ipmb_res;

	if (pData == NULL || cc == NULL)
	{
		return PECI_CC_INVALID_REQ;
	}

	// Per the PECI spec, the read length must be a qword or dqword
	if (u8ReadLen != 8 && u8ReadLen != 16)
	{
		return PECI_CC_INVALID_REQ;
	}
	
	for (int retry = 0; retry < MAX_PECI_RETRY; retry++) {
		ipmb_req = ipmb_txb();
	  	ipmb_res = ipmb_rxb();
	  	peci_cmd = &(ipmb_req->data[0]);
	  	memset(peci_cmd, 0, sizeof(peci_cmd_t));

	  	peci_cmd->target = target;
	  	peci_cmd->read_len = u8ReadLen + 1;                                             // plus 1 byte completion code.
	  	peci_cmd->write_len = 10;                                                       // for get frame command the input length is 10 

	  	peci_cmd->write_buffer[0] = PECI_CRASHDUMP | 0x01;
	  	peci_cmd->write_buffer[1] = PECI_HOST_ID;
	  	peci_cmd->write_buffer[2] = 0;                                                  // Version 0x00
	  	peci_cmd->write_buffer[3] = 0x03;                                               // Opcode 0x03 - GetFrame
	  	peci_cmd->write_buffer[4] = (uint8_t)(param0 & 0x00ff);                         // Parameter 0 - low byte
	  	peci_cmd->write_buffer[5] = (uint8_t)((param0 & 0xff00) >> 8);                  // Parameter 0 - high byte
	  	peci_cmd->write_buffer[6] = (uint8_t)(param1 & 0x00ff);                         // Parameter 1 - low byte
	  	peci_cmd->write_buffer[7] = (uint8_t)((param1 & 0xff00) >> 8);                  // Parameter 1 - high byte
	  	peci_cmd->write_buffer[8] = (uint8_t)(param2 & 0x00ff);                         // Parameter 2 - low byte
	  	peci_cmd->write_buffer[9] = (uint8_t)((param2 & 0xff00) >> 8);                  // Parameter 2 - high byte
	    
		if (retry != 0) {
			peci_cmd->write_buffer[1] = PECI_HOST_ID | 1;                           // Set retry bit (bit 0) to declare this is retry command
		}
		ret = IPMB_peci_issue_cmd(ipmb_req, ipmb_res);
		if (ipmb_res->data[0] != PECI_DEV_CC_NEED_RETRY) {
			break;
		}
	}
       	  
	
	if (ret == PECI_CC_IPMB_SUCCESS)
	{
		// PECI completion code in the 1st byte
		*cc = ipmb_res->data[0];
		memcpy(pData, (uint8_t*)&(ipmb_res->data[1]), u8ReadLen);

        if (*cc != PECI_DEV_CC_SUCCESS && *cc != PECI_DEV_CC_FATAL_MCA_DETECTED
		&& *cc != PECI_DEV_CC_CATASTROPHIC_MCA_ERROR)
        {
            ret = PECI_CC_HW_ERR;
        }
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
	EPECIStatus ret = PECI_CC_SUCCESS;
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
