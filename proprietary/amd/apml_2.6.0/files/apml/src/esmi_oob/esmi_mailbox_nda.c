/*
 * University of Illinois/NCSA Open Source License
 *
 * Copyright (c) 2022, Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Developed by:
 *
 *              AMD Research and AMD Software Development
 *
 *              Advanced Micro Devices, Inc.
 *
 *              www.amd.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal with the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimers.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimers in
 *    the documentation and/or other materials provided with the distribution.
 *  - Neither the names of <Name of Development Group, Name of Institution>,
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this Software without specific prior written
 *    permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include <esmi_oob/apml.h>
#include <esmi_oob/esmi_mailbox.h>
#include <esmi_oob/esmi_mailbox_nda.h>

oob_status_t read_ras_df_err_validity_check(uint8_t soc_num,
					    uint8_t df_block_id,
					    struct ras_df_err_chk *err_chk)
{
	uint32_t buffer;
	oob_status_t ret;

	if (!err_chk)
		return OOB_ARG_PTR_NULL;

	if (df_block_id > (MAX_DF_BLOCK_IDS - 1))
		return OOB_INVALID_INPUT;

	ret = esmi_oob_read_mailbox(soc_num, READ_RAS_LAST_TRANS_ADDR_CHK,
				    (uint32_t)df_block_id, &buffer);
	if (!ret) {
		/* Number of df block instances */
		err_chk->df_block_instances = buffer;
		/* bits 16 - 24 of buffer will length of error log */
		/* in bytes per instance  */
		err_chk->err_log_len = buffer >> 16;
	}

	return ret;
}

oob_status_t read_ras_df_err_dump(uint8_t soc_num,
                                  union ras_df_err_dump ras_err,
                                  uint32_t *data)
{
	/* Validate error log offset, DF_BLOCK_ID and DF_BLOCK_INSTANCE */
	if ((ras_err.input[0] & 3) != 0 ||
	     ras_err.input[0] > (MAX_ERR_LOG_LEN - 1) ||
	     ras_err.input[1] > (MAX_DF_BLOCK_IDS - 1) ||
	     ras_err.input[2] > (MAX_DF_BLOCK_INSTS - 1))
		return OOB_INVALID_INPUT;

	return esmi_oob_read_mailbox(soc_num, READ_RAS_LAST_TRANS_ADDR_DUMP,
				     ras_err.data_in, data);
}

oob_status_t read_ppin_fuse(uint8_t soc_num, uint64_t *data)
{
	uint32_t buffer;
	oob_status_t ret;

	/* NULL Pointer check */
	if (!data)
		return OOB_ARG_PTR_NULL;

	/* Read lower 32 bit PPIN data */
	ret = esmi_oob_read_mailbox(soc_num, READ_PPIN_FUSE,
				    LO_WORD_REG, &buffer);
	if (!ret) {
		*data = buffer;
		/* Read higher 32 bit PPIN data */
		ret = esmi_oob_read_mailbox(soc_num, READ_PPIN_FUSE,
					    HI_WORD_REG, &buffer);
		if (!ret)
			*data |= ((uint64_t)buffer << 32);
	}

	return ret;
}

oob_status_t read_cclk_freq_limit(uint8_t soc_num, uint32_t *cclk_freq)
{
	return esmi_oob_read_mailbox(soc_num, READ_PACKAGE_CCLK_FREQ_LIMIT,
				     0, cclk_freq);
}

oob_status_t read_socket_c0_residency(uint8_t soc_num, uint32_t *c0_res)
{
	return esmi_oob_read_mailbox(soc_num, READ_PACKAGE_C0_RESIDENCY,
				     0, c0_res);
}
