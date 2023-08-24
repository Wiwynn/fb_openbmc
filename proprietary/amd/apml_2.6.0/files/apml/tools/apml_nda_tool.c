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

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <esmi_oob/apml.h>
#include <esmi_oob/esmi_mailbox.h>
#include <esmi_oob/esmi_mailbox_nda.h>
#include "apml_nda_tool.h"

static int flag;

void show_module_nda_commands(char *exe_name, char *command)
{
        if (!strcmp(command, "mailbox") || !strcmp(command, "1"))
		printf("  --showcclkfreqlimit\t\t\t\t\t\t\t\t Get "
		       "cclk freqlimit for a given socket in MHz\n"
		       "  --showc0residency\t\t\t\t\t\t\t\t Show "
		       "c0_residency for a given socket\n"
		       "  --showppinfuse\t\t\t\t\t\t\t\t Show 64bit PPIN"
		       " fuse data\n"
		       "  --showrasdferrvaliditycheck\t\t  [DF_BLOCK_ID]\t\t\t\t "
		       "Show RAS DF error validity check for a given blockID\n"
		       "  --showrasdferrdump\t\t\t  [OFFSET][BLK_ID][BLK_INST]\t\t "
		       "Show RAS DF error dump\n");
}

static void apml_get_ras_df_validity_chk(uint8_t soc_num, uint8_t blk_id) {
	struct ras_df_err_chk err_chk;
	oob_status_t ret;

	ret = read_ras_df_err_validity_check(soc_num, blk_id, &err_chk);
	if (ret) {
		printf("Failed to read RAS DF validity check, Err[%d]:%s\n",
		       ret, esmi_get_err_msg(ret));
		return;
	}

	printf("----------------------------------------------------\n");
	printf("| Err log length\t\t| %-17u|\n", err_chk.err_log_len);
	printf("| DF Block instances\t\t| %-17u|\n", err_chk.df_block_instances);
	printf("----------------------------------------------------\n");
}

static void apml_get_ras_df_err_dump(uint8_t soc_num, union ras_df_err_dump df_err)
{
	uint32_t data = 0;
	oob_status_t ret;

	ret = read_ras_df_err_dump(soc_num, df_err, &data);
	if (ret) {
		printf("Failed to read RAS error dump for offset[%d] "
		       "Err[%d]:%s\n", df_err.input[0], ret,
		       esmi_get_err_msg(ret));
		return;
	}

	printf("--------------------------------------------------"
	       "-------------------\n");
	printf("| Data from offset[%03d]\t\t| 0x%-32x|\n", df_err.input[0], data);
	printf("--------------------------------------------------"
	       "-------------------\n");
}

static oob_status_t apml_get_ppin_fuse(uint8_t soc_num)
{
	uint64_t data = 0;
	oob_status_t ret;

	ret = read_ppin_fuse(soc_num, &data);
	if (ret) {
		printf("Failed to get the PPIN fuse data, Err[%d]:%s\n",
		       ret, esmi_get_err_msg(ret));
		return ret;
	}

	printf("------------------------------------------------------------"
	       "---------------------\n");
	printf("| PPIN Fuse | 0x%-64llx |\n", data);
	printf("------------------------------------------------------------"
	       "---------------------\n");
}

static oob_status_t apml_get_cclk_freqlimit(uint8_t soc_num)
{
	uint32_t buffer = 0;
	oob_status_t ret;

	ret = read_cclk_freq_limit(soc_num, &buffer);
	if (ret != OOB_SUCCESS) {
		printf("Failed to get cclk_freqlimit, Err[%d]:%s\n",
		       ret, esmi_get_err_msg(ret));
		return ret;
	}
	printf("-----------------------------------------------------\n");
	printf("| cclk_freqlimit (MHz)\t\t | %-16u |\n", buffer);
	printf("-----------------------------------------------------\n");

	return OOB_SUCCESS;
}

static oob_status_t apml_get_sockc0_residency(uint8_t soc_num)
{
	uint32_t buffer = 0;
	oob_status_t ret;

	ret = read_socket_c0_residency(soc_num, &buffer);
	if (ret != OOB_SUCCESS) {
		printf("Failed to get c0_residency, Err[%d]:%s\n",
		       ret, esmi_get_err_msg(ret));
		return ret;
	}
	printf("----------------------------------------------\n");
	printf("| c0_residency (%%)\t |  %-16u |\n", buffer);
	printf("----------------------------------------------\n");

	return OOB_SUCCESS;
}
/*
 * returns 0 if the given string is a number for the given base, else 1.
 * Base will be 16 for hexdecimal value and 10 for decimal value.
 */
static oob_status_t validate_number(char *str, uint8_t base)
{
	uint64_t buffer_number = 0;
	char *endptr;

	buffer_number = strtol(str, &endptr, base);
	if (*endptr != '\0')
		return OOB_INVALID_INPUT;

	return OOB_SUCCESS;
}

/*
 * Parse command line parameters and set data for program.
 * @param argc number of command line parameters
 * @param argv list of command line parameters
 */
oob_status_t parseesb_nda_args(int argc, char **argv, uint8_t soc_num)
{
	//Specifying the expected options
	static struct option long_options[] = {
		{"showrasdferrvaliditycheck", required_argument,  &flag,  400},
		{"showrasdferrdump",          required_argument,  &flag,  401},
		{"showcclkfreqlimit",         no_argument,	  &flag,  402},
		{"showc0residency",	      no_argument,	  &flag,  403},
		{"showppinfuse",	      no_argument,	  &flag,  404},
		{0,                     0,                      0,      0},
	};

	union ras_df_err_dump df_err = {0};
	uint32_t val1;
	uint32_t val2;
	int opt = 0; /* option character */
	int long_index = 0;
	char *end;
        char *helperstring = "";
        oob_status_t ret = OOB_NOT_FOUND;

	optind = 2;
	opterr = 0;

	while ((opt = getopt_long(argc, argv, helperstring,
		long_options, &long_index)) != -1) {
		if (opt && optopt) {
			printf("Option '%s' require an argument\n", argv[ optind - 1]);
			return OOB_SUCCESS;
		}
		if (opt == 0 && ((*long_options[long_index].flag) == 400)) {
			if (validate_number(argv[optind - 1], 10)) {
				printf("Option '--%s' require argument as valid"
				       " numeric value\n",
				       long_options[long_index].name);
				return OOB_SUCCESS;
			}
	}

		if (opt == 0 && (*long_options[long_index].flag == 401)) {
			if ((optind + 1) >= argc) {
				printf("Option '--%s' require THREE"
				       " arguments\n",
				       long_options[long_index].name);
				return OOB_SUCCESS;
			}

			if (validate_number(argv[optind - 1], 10)
			    || validate_number(argv[optind], 10) ||
			    validate_number(argv[optind+1], 10)) {
				printf("Option '--%s' require argument as"
				       " valid numeric value\n",
				       long_options[long_index].name);
				return OOB_SUCCESS;
			}
		}

		switch (opt) {
		case 0:
			if (*(long_options[long_index].flag) == 400) {
				val1 = atoi(argv[optind - 1]);
				apml_get_ras_df_validity_chk(soc_num, val1);
			} else if (*(long_options[long_index].flag) == 401) {
				/* Offset */
				df_err.input[0] = atoi(argv[optind - 1]);
				/* DF block ID */
				df_err.input[1] = atoi(argv[optind++]);
				/* DF block ID instance */
				df_err.input[2] = atoi(argv[optind++]);
				apml_get_ras_df_err_dump(soc_num, df_err);
			} else if (*(long_options[long_index].flag) == 402) {
				/* show cclk frequency limit */
				apml_get_cclk_freqlimit(soc_num);
			} else if (*(long_options[long_index].flag) == 403) {
				/* show C0 residency */
				apml_get_sockc0_residency(soc_num);
			} else if (*(long_options[long_index].flag) == 404) {
				/* show PPIN Fuse data */
				apml_get_ppin_fuse(soc_num);
			}
			ret = OOB_SUCCESS;
			break;
		default:
			break;
		}
	}

	return ret;
}
