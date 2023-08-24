/*
 * University of Illinois/NCSA Open Source License
 *
 * Copyright (c) 2022, Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Developed by:
 *
 *                 AMD Research and AMD Software Development
 *
 *                 Advanced Micro Devices, Inc.
 *
 *                 www.amd.com
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

#ifndef INCLUDE_APML_MAILBOX_NDA_H_
#define INCLUDE_APML_MAILBOX_NDA_H_

#include "apml_err.h"

/* CONSTANTS OR MAGIC NUMBERS */

/* Maximum error log length */
#define MAX_ERR_LOG_LEN                 256	//!< Max error log length //
/* Maximum DF block-ID's */
#define MAX_DF_BLOCK_IDS                256	//!< Max DF block IDs //
/* Maximum instances of DF - Block-ID */
#define MAX_DF_BLOCK_INSTS              256	//!< Max DF block instances //

/** \file esmi_mailbox_nda.h
 *  Header file for the NDA Mailbox messages supported by APML library.
 *  All required function, structure, enum, etc. definitions should be defined
 *  in this file.
 *
 *  @details  This header file contains the following:
 *  NDA APIs prototype of the Mailbox messages exported by the APML library.
 *  Description of the API, arguments and return values.
 *  The Error codes returned by the API.
 */

/**
 * @brief Mailbox message IDs defined in the APML library (NDA)
 */
typedef enum {
	READ_PACKAGE_CCLK_FREQ_LIMIT = 0x16,
	READ_PACKAGE_C0_RESIDENCY,
	READ_PPIN_FUSE = 0x1F,
	READ_RAS_LAST_TRANS_ADDR_CHK = 0x5B,
	READ_RAS_LAST_TRANS_ADDR_DUMP,
} esb_mailbox_nda_commmands;

/**
 * @brief RAS df err validity check output status.
 * Structure  contains the following members.
 * df_block_instances number of block instance with error log to report (0 - 256)
 * err_log_len length of error log in bytes per instance (0 - 256).
 */
struct ras_df_err_chk {
	uint16_t df_block_instances : 9;  //!<  Number of DF block instances
	uint16_t err_log_len : 9;        //!< len of er log in bytes per inst.
};

/**
 * @brief RAS df error dump input.
 * Union contains the following members.
 * input[0] 4 byte alligned offset in error log ( 0 - 255)
 * input[1] DF block ID (0 - 255)
 * input[2] Zero based index of DF block instance (0 - 255)
 * input[3] Reserved
 * data_in  32-bit data input
 *
 */
union ras_df_err_dump {
	uint8_t input[4];     //!< [0] offset, [1] DF block ID
			      //!< [2] block ID inst, [3] RESERVED
	uint32_t data_in;     //!< 32 bit data in for the DF err dump
};

/**
 *  @brief Read number of instances of DF blocks of type DF_BLOCK_ID with errors.
 *
 *  @details This function reads the number instance of DF blocks
 *  of type DF_BLOCK_ID that have an error log to report.
 *
 *  @param[in] soc_num Socket index.
 *
 *  @param[in] df_block_id DF block ID.
 *
 *  @param[out] err_chk ras_df_err_chk Struct containing number of DF block
 *  instances andclength of error log in bytes per instanace.
 *
 *  @retval ::OOB_SUCCESS is returned upon successful call.
 *  @retval Non-zero is returned upon failure.
 *
 */
oob_status_t read_ras_df_err_validity_check(uint8_t soc_num,
					    uint8_t df_block_id,
					    struct ras_df_err_chk *err_chk);

/**
 *  @brief Read RAS DF error dump.
 *
 *  @details This function reads 32 bits of data from the offset provided for
 *  DF block instance retported by  read_ras_df_err_validity_check.
 *
 *  @param[in] soc_num Socket index.
 *
 *  @param[in] ras_err ras_df_err_dump Union containing 4 byte offset,
 *  DF block ID and block ID instance.
 *
 *  @param[out] data output data from offset of DF block instance.
 *
 *  @retval ::OOB_SUCCESS is returned upon successful call.
 *  @retval Non-zero is returned upon failure.
 *
 */
oob_status_t read_ras_df_err_dump(uint8_t soc_num,
				  union ras_df_err_dump ras_err,
				  uint32_t *data);

/**
 *  @brief Read CCLK frequency limit for the given socket
 *
 *  @details This function will read CPU core clock frequency limit
 *  for the given socket.
 *
 *  @param[in] soc_num Socket index.
 *
 *  @param[out] cclk_freq CPU core clock frequency limit [MHz]
 *
 *  @retval ::OOB_SUCCESS is returned upon successful call.
 *  @retval Non-zero is returned upon failure.
 *
 */
oob_status_t read_cclk_freq_limit(uint8_t soc_num, uint32_t *cclk_freq);

/**
 *  @brief Read socket C0 residency
 *
 *  @details This function will provides average C0 residency across all cores
 *  in the socket. 100% specifies that all enabled cores in the socket are runningin C0.
 *
 *  @param[in] soc_num Socket index.
 *
 *  @param[out] c0_res is to read Socket C0 residency[%].
 *
 *  @retval ::OOB_SUCCESS is returned upon successful call.
 *  @retval Non-zero is returned upon failure.
 *
 */
oob_status_t read_socket_c0_residency(uint8_t soc_num, uint32_t *c0_res);

/**
 *  @brief Get the 64 bit PPIN fuse
 *
 *  @details This function will read the 64 bit PPIN fuse available via OPN_PPIN
 *  fuse.
 *
 *  @param[in] soc_num Socket index.
 *
 *  @param[out] data PPIN fuse data
 *
 */
oob_status_t read_ppin_fuse(uint8_t soc_num, uint64_t *data);

/*
 * @} */  // end of MailboxNDAMsg
/****************************************************************************/

#endif  // INCLUDE_ESMI_OOB_MAILBOX_NDA_H_
