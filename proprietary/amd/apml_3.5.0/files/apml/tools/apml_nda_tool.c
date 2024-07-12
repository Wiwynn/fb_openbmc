/*
 * Copyright (c) 2023, Advanced Micro Devices, Inc. All rights reserved.
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

extern uint8_t p_type;
static int flag;

void show_module_nda_commands(char *exe_name, char *command, struct processor_info *p_info)
{
	if (!strcmp(command, "mailbox") || !strcmp(command, "1")) {
		if (p_type == FAM_1A_MOD_00 || p_type == FAM_1A_MOD_10) {
			printf("  --setbmcrasaction\t\t\t  [PAYLOAD(HEX)]"
			       "[OFFSET]\n\t\t\t\t\t  [REPAIRENTRYNUMBER]"
			       "[RASACTIONID][EOM]\t Set BMC RAS action\n"
			       "  --getbmcrasactionstatus\t\t"
			       "  [REPAIRENTRYNUMBER][RASACTIONID]\t"
			       " Get BMC RAS action status\n");
		}
	}
}

static oob_status_t apml_get_ras_action_status(uint8_t soc_num,
					       struct get_ras_action_data_in data_in)
{
	struct ras_action_status status = {0};
	oob_status_t ret;

	ret = get_bmc_ras_action_status(soc_num, data_in, &status);
	if (ret) {
		printf("Failed to get ras action status, Err[%d]:%s\n",
		       ret, esmi_get_err_msg(ret));
		return ret;
	}

	printf("---------------------------------------------\n");
	printf("| Repair result \t | %-16u |\n", status.repair_result);
	printf("| Repair entry num \t | %-16u |\n",
	       status.repair_entry_num);
	printf("| RAS action ID \t | %-16u |\n", status.ras_action_id);
	printf("---------------------------------------------\n");
}

static oob_status_t apml_set_ras_action(uint8_t soc_num,
					struct set_ras_action_data_in data_in)
{
	uint32_t resp = 0;
	oob_status_t ret;

	ret = set_bmc_ras_action_status(soc_num, data_in, &resp);
	if (ret) {
		printf("Failed to set ras action, Err[%d]:%s\n",
		       ret, esmi_get_err_msg(ret));
		return ret;
	}

	printf("---------------------------------------------\n");
	printf("| RAS action response\t | %-16u |\n", resp);
	printf("---------------------------------------------\n");
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
		{"setbmcrasaction",	      required_argument,  &flag,  405},
		{"getbmcrasactionstatus",     required_argument,  &flag,  406},
		{0,                     0,                      0,      0},
	};

	struct set_ras_action_data_in d_in = {0};
	struct get_ras_action_data_in ras_din = {0};
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

		if (opt == 0 && (*long_options[long_index].flag == 405)) {
			if ((optind + 3) >= argc) {
				printf("Option '--%s' require FIVE"
				       " arguments\n",
				       long_options[long_index].name);
				return OOB_SUCCESS;
			}

			if (validate_number(argv[optind - 1], 0)) {
				printf("Option '--%s' require argument as"
				       "valid hex value\n\n",
				       long_options[long_index].name);
				return OOB_SUCCESS;
			}
			if (validate_number(argv[optind], 10)
			    || validate_number(argv[optind + 1], 10)
			    || validate_number(argv[optind + 2], 10)
			    || validate_number(argv[optind + 3], 10)) {
				printf("Option '--%s' require argument as"
				       " valid numeric value\n",
				       long_options[long_index].name);
				return OOB_SUCCESS;
			}
		}

		if (opt == 0 && (*long_options[long_index].flag == 406)) {
			if ((optind) >= argc) {
				printf("Option '--%s' require TWO"
				       " arguments\n",
				       long_options[long_index].name);
				return OOB_SUCCESS;
			}

			if (validate_number(argv[optind - 1], 10)
			    || validate_number(argv[optind], 10)) {
				printf("Option '--%s' require argument as"
				       " valid numeric value\n",
				       long_options[long_index].name);
				return OOB_SUCCESS;
			}
		}



		switch (opt) {
		case 0:
			if (*(long_options[long_index].flag) == 405) {
				/* Set bmc ras action */
				/* pay load */
				d_in.payload.pay_load = strtol(argv[optind - 1], &end, 0);
				/* offset */
				d_in.payload.offset = atoi(argv[optind++]);
				/* repair entry number */
				d_in.payload.repair_entry_num = atoi(argv[optind++]);
				/* ras action ID */
				d_in.ras_act_id = atoi(argv[optind++]);
				/* EOM flag */
				d_in.eom_flag = atoi(argv[optind++]);
				apml_set_ras_action(soc_num, d_in);
			} else if (*(long_options[long_index].flag) == 406) {
				/* Get bmc ras action status */
				/* Repair entry number */
				ras_din.pay_load.repair_entry_num = atoi(argv[optind - 1]);
				/* ras action id */
				ras_din.ras_action_id = atoi(argv[optind++]);
				apml_get_ras_action_status(soc_num, ras_din);
			}

			ret = OOB_SUCCESS;
			break;
		default:
			break;
		}
	}

	return ret;
}
