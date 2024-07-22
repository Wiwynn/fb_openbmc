###############################################################################
# INTEL CONFIDENTIAL                                                          #
#                                                                             #
# Copyright 2023 Intel Corporation.                                           #
#                                                                             #
# This software and the related documents are Intel copyrighted materials,    #
# and your use of them is governed by the express license under which they    #
# were provided to you ("License"). Unless the License provides otherwise,    #
# you may not use, modify, copy, publish, distribute, disclose or transmit    #
# this software or the related documents without Intel's prior written        #
# permission.                                                                 #
#                                                                             #
# This software and the related documents are provided as is, with no express #
# or implied warranties, other than those that are expressly stated in the    #
# License.                                                                    #
###############################################################################

###############################################################################
# Note:                                                                       #
# "addtional_check" is a special key for excluding additionalProperties       #
#                                                                             #
###############################################################################

import copy

Template_Base = \
{
    "METADATA": {
        "_version": "0x[0-9a-f]+",
        "bmc_fw_ver": ".+",
        "bios_id": ".+",
        "timestamp": "\d{4}-\d{1,2}-\d{1,2}T\d{1,2}:\d{1,2}:\d{1,2}Z",
        "trigger_type": ".+",
        "platform_name": ".+",
        "crashdump_ver": ".+",
        "_input_file_ver": ".+",
        "_input_file": ".+",
        "cjson_version":	".+",
        "_time%s:1": "^([0-9]+.[0-9]+s)$",
        "cpu%d:1": {
            "peci_id": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
            "cpuid": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
            "_cpuid_source": "(EVENT|STARTUP|OVERWRITTEN|INVALID)",
            "die_mask": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
            "early_ucode_patch_ver":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
            "ppin": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
            "ucode_patch_ver": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
            "cha_count_total": ".+",
            "platform_id": ".+",
            "compute%d:die_mask": {
                "_core_mask_source": "(EVENT|STARTUP)",
                "_cha_count_source": "(EVENT|STARTUP)",
                "core_mask": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                "cha_mask": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                "core_count": "0x[0-9a-f]+",
                "cha_count": "0x[0-9a-f]+",
                "crashcore_count": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
				"crashcore_mask":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",                
                "additionalProperties": "false",
            },
            "io0": {
                "ierrloggingreg":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                "mcerrloggingreg":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                "final_mca_err_src_log":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                "final_ierrloggingreg":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                "firstierrtsc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                "firstmcerrtsc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                "mca_err_src_log":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                "final_mca_err_src_log":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)"          
            },
            "additionalProperties": "false",
        },
        "_total_time": "^([0-9]+.[0-9]+s)$",
        "_reset_detected": ".+",
        "additionalProperties": "false",
    },
    "PROCESSORS": {
        "cpu%d:1": {
            "io0": {
                "MCA": {
                    "uncore": {
                        "MC4": {
                            "mc4_ctl": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "mc4_status": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "mc4_addr": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "mc4_misc": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "mc4_ctl2": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "additionalProperties": "false"
                        }
                    }
                },
                "RDIAMSR": {
                    "_version": "0x[0-9a-f]+",
                    "_time_%s:1": "^([0-9]+.[0-9]+s)$",
                    "RDIAMSR%s:1": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)"
                },
                "crashlog":	{
                    "agent_id_0x10840161":	{
                        "#data_IO_DIE_S3M_INDEX":	".+"
                    }
                } 
            },
            "io%d:die_mask": {
                "MCA": {
                    "_version": "0x[0-9a-f]+",
                    "uncore": {
                        "PCU": {
                            "pcu_ctl": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "pcu_status": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "pcu_addr": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "pcu_misc": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "pcu_ctl2": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "additionalProperties": "false"
                        }
                    },
                    "_time_MCA_%s:1": "^([0-9]+.[0-9]+s)$",
                    "_failcount_%s:1":"\d+",
                    "additionalProperties": "false"
                },
                "crashlog":	{
                    "_version":	"0x[0-9a-f]+",
                    "cpu_crashlog_enabled":	"(true|false)",
                    "agent_id_0xf3dcb":	{
                        "#data_IO_DIE_PCODE_CRASH_INDEX":	".+"
                    },
                    "agent_id_0x13a60":	{
                        "#data_PUNIT_CRASH_INDEX":	".+"
                    },
                    "agent_id_0xdfd23947":	{
                        "#data_IO_DIE_OOBMSM_CRASH_INDEX":	".+"
                    },
                    "agent_id_0xe3335de0":	{
                        "#data_FW_OOBMSM_CRASH_INDEX":	".+"
                    },
                    "_time_crashlog":	"^([0-9]+.[0-9]+s)$",
                    "_failcount_crashlog":	"\d+"
                }                
            },
            "compute%d:die_mask": {
                "MCA": {
                    "uncore":{
                        "PCU":	{
                            "pcu_ctl": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "pcu_status": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "pcu_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "pcu_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "pcu_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "additionalProperties": "false"
                        },
                    },
                    "_version":	"0x[0-9a-f]+",
                    "additionalProperties": "false",
                    "_time_MCA_%s:1": "^([0-9]+.[0-9]+s)$",
                    "_failcount_MCA_%s:1":	"\d+",
                    "_time_MCA_CORE": "^([0-9]+.[0-9]+s)$",
                    "additionalProperties": "false"
                },
                "RDIAMSR": {
                    "_version": "0x[0-9a-f]+",
                    "_time_%s:1": "^([0-9]+.[0-9]+s)$",
                },
                "crashlog": {
                    "_version": "0x[0-9a-f]+",
                    "cpu_crashlog_enabled": "(true|false)",
                    "agent_id_0x17a0f0":	{
                        "#data_C_DIE_PCODE_CRASH_INDEX": ".+"
                    },
                    "agent_id_0x13a60":	{
                        "#data_PUNIT_CRASH_INDEX": ".+"
                    },
                    "agent_id_0xdfd23957":	{
                        "#data_C_DIE_OOBMSM_CRASH_INDEX": ".+"
                    },
                    "agent_id_0x406b1a82":	{
                        "#data_C_DIE_TOR_CRASH_INDEX": ".+"
                    },
                    "agent_id_0xe3335de0":	{
                        "#data_FW_OOBMSM_CRASH_INDEX": ".+"
                    },                    
                    "_time_crashlog": "^([0-9]+.[0-9]+s)$",
                    "_failcount_crashlog":	"\d+"
                }
            },
            "soc": {
                "MCA": {
                    "_version": "0x[0-9a-f]+",
                    "_time_MCA_%s:1": "^([0-9]+.[0-9]+s)$",
                    "_failcount_%s:1": "\d+",
                    "UPI%d:6": {
                        "upi%d_ctl2": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "upi%d_ctl": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "upi%d_status": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "upi%d_addr": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "upi%d_misc": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },
                    "MSE%d:12":	{
                        "mse%d_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mse%d_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mse%d_status":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mse%d_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mse%d_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },
                    "MCHN%d:12":	{ 
                        "mchn%d_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mchn%d_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mchn%d_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mchn%d_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mchn%d_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },
                    "MC13":	{ 
                        "mc13_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc13_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc13_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc13_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc13_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },
                    "MC14":	{ 
                        "mc14_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc14_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc14_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc14_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc14_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },
                    "MC15":	{ 
                        "mc15_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc15_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc15_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc15_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc15_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },
                    "MC16":	{ 
                        "mc16_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc16_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc16_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc16_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc16_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },
                    "MC17":	{ 
                        "mc17_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc17_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc17_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc17_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc17_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },
                    "MC18":	{ 
                        "mc18_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc18_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc18_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc18_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc18_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },         
                    "MC19":	{ 
                        "mc19_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc19_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc19_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc19_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc19_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },    
                    "MC20":	{ 
                        "mc20_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc20_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc20_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc20_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc20_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },    
                    "MC21":	{ 
                        "mc21_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc21_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc21_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc21_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc21_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },                                                                                                                                                       
                    "MC22":	{ 
                        "mc22_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc22_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc22_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc22_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc22_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },                                                                                                                                                       
                    "MC23":	{ 
                        "mc23_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc23_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc23_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc23_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc23_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },      
                    "MC24":	{ 
                        "mc24_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc24_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc24_status":"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc24_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "mc24_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },                                                                                                                                                      
                    "B2CMI%d:12":	{
                        "b2cmi%d_ctl2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "b2cmi%d_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "b2cmi%d_status":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "b2cmi%d_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "b2cmi%d_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },                        
                    "CBO%d:cha_count_total": {
                        "cbo%d_status": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "cbo%d_addr": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "cbo%d_ctl": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "cbo%d_misc": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "cbo%d_misc2": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "cbo%d_misc3": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "cbo%d_misc4": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    },
                    "LLC%d:cha_count_total":	{
                        "llc%d_misc2":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "llc%d_ctl":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "llc%d_status":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "llc%d_addr":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "llc%d_misc":	"(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties":"false",
                    },
                    "additionalProperties": "false"
                },
                "uncore": {
                    "_version": "0x[0-9a-f]+",
                    "_time_Uncore_%s:1": "^([0-9]+.[0-9]+s)$"
                },
                "TOR": {
                    "_version": "0x[0-9a-f]+",
                    "_encoding":	"string",
					"_delimiter":	";",
					"_elements":	"0x100",
                    "cha%d:cha_count_total": "^(?:0x[0-9a-f]+(?:,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)(?:;(?:0x[0-9a-f]+(?:,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)){255}$",
                    "_time_TOR": "^([0-9]+.[0-9]+s)$",
                    "additionalProperties": "false"
                }
            }        
        }
    }
}

# Template for GNR
Template_GNR = copy.deepcopy(Template_Base)
Template_GNR["PROCESSORS"]["cpu%d:1"]["compute%d:die_mask"]["MCA"]["core%d:coremask_minus_crashcore"] = {
                        "thread%d:2": {
                            "MC%d:4": {
                                "additionalProperties":"false",
                                "mc%d_ctl": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                                "mc%d_status": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                                "mc%d_addr": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                                "mc%d_misc": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                                "mc%d_ctl2": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                                "additionalProperties": "false"
                            },
                            "additionalProperties": "false"
                        },
                        "additionalProperties": "false"
                    }
Template_GNR["PROCESSORS"]["cpu%d:1"]["compute%d:die_mask"]['RDIAMSR']["core%d:coremask_minus_crashcore"] = {
                        "thread%d:2": {
                            "RDIAMSR%s:1": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                            "additionalProperties": "false"
                        },
                        "RDIAMSR%s:1": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                        "additionalProperties": "false"
                    }
Template_GNR["PROCESSORS"]["cpu%d:1"]["compute%d:die_mask"]["big_core"] = {
                    "_version": "0x[0-9a-f]+",
                    "core%d:crashcore_mask": {
                        "thread1":{},
                        "thread0":{}
                    },
                    "_time_BigCore:0": "^([0-9]+.[0-9]+s)$"
                }

# Template for SRF
Template_SRF = copy.deepcopy(Template_Base)
Template_SRF["PROCESSORS"]["cpu%d:1"]["compute%d:die_mask"]["MCA"]["module%d:coremask_minus_crashcore"] =  {
                        "core%d:4": {
                            "MC%d:4": {
                                "additionalProperties":"false",
                                "mc%d_ctl": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                                "mc%d_status": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                                "mc%d_addr": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                                "mc%d_misc": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                                "mc%d_ctl2": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                                "additionalProperties": "false"
                            },
                            "additionalProperties": "false"
                        }
}
Template_SRF["PROCESSORS"]["cpu%d:1"]["compute%d:die_mask"]["atom_core"] = {
                        "_version": "0x[0-9a-f]+",
                        "module%d:crashcore_mask": {
                            "core%d:4":{},
                            "additionalProperties": "false"
                        },
                        "_time_%s:0": "^([0-9]+.[0-9]+s)$"                
                    }
Template_SRF["PROCESSORS"]["cpu%d:1"]["compute%d:die_mask"]["RDIAMSR"]["module%d:coremask_minus_crashcore"] =  {
                    "core%d:4":{
                    "RDIAMSR%s:1": "(0x[0-9a-f]+(,CC:0x[8,9][0-9a-f],RC:0x[0-9a-f])?|N/A)",
                    "additionalProperties": "false"
                    }
}   

Template_GNRD = copy.deepcopy(Template_GNR)
Template_GNRD["PROCESSORS"]["cpu%d:1"]["soc"]["MCA"].pop("UPI%d:6")