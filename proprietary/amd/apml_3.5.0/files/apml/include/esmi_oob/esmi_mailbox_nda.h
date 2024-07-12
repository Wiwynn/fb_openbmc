/*
 * Copyright (c) 2023, Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef INCLUDE_APML_MAILBOX_NDA_H_
#define INCLUDE_APML_MAILBOX_NDA_H_

#include <stdbool.h>
#include "apml_err.h"

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
	SET_BMC_RAS_ACTION = 0x66,
	GET_BMC_RAS_ACTION_STATUS,
} esb_mailbox_nda_commmands;

/**
 * @brief data for status query payload.
 * Members contatin repair entry number.
 */
struct status_query_payload {
	uint8_t repair_entry_num : 3;	//!< Repair entry number
};

/**
 * @brief data input for the get BMC RAS action status.
 * Members contatin repair entry number and ras action id.
 */
struct get_ras_action_data_in {
	struct status_query_payload pay_load;	//!< It contains repair entry number
	uint8_t ras_action_id   : 4;    //!< RAS action id
};

/**
 * @brief Output data for the get BMC RAS action status.
 * Members contatin repair result, repair entry number and ras action id.
 */
struct ras_action_status {
	uint8_t repair_result;                  //!< Repair result for the requested entry number
	uint8_t repair_entry_num : 3;		//!< Repair entry number
	uint8_t ras_action_id : 4;		//!< RAS action ID
};

/**
 * @brief data for the action payload.
 * Members contatin pay load, ofset, repair entry number.
 */
struct action_payload {
	uint16_t pay_load;			//!< 2 byte data payload
	uint8_t offset : 5;			//!< 2 byte offset
	uint8_t repair_entry_num : 3;		//!< repair entry number
};

/**
 * @brief data input for set ras action status.
 * Members contatin struct action_payload, ras action ID
 * and eom flag.
 */
struct set_ras_action_data_in {
	struct action_payload payload;		//!< It contains payload, offset and repair entry number
	uint8_t ras_act_id : 4;                 //!< RAS action ID
	bool eom_flag;                          //!< End of msg flag
};

/**
 *  @brief Set BMC RAS action status
 *
 *  @details This function will set BMC RAS action status for the
 *  specified action ID.
 *  Supported platforms: \ref Fam-1Ah_Mod-00h-0Fh
 *
 *  @param[in] soc_num Socket index.
 *
 *  @param[in] data_in struct set_ras_action_data_in containing
 *  struct action_payload, ras action ID and eom flag.
 *
 *  @param[out] status response for ras action.
 */
oob_status_t set_bmc_ras_action_status(uint8_t soc_num,
				       struct set_ras_action_data_in data_in,
				       uint32_t *status);

/**
 *  @brief Get BMC RAS action status
 *
 *  @details This function will read BMC RAS action status for the
 *  specified action ID.
 *  Supported platforms: \ref Fam-1Ah_Mod-00h-0Fh
 *
 *  @param[in] soc_num Socket index.
 *
 *  @param[in] data_in struct get_ras_action_data_in containing
 *  struct status_query_payload and ras action id.
 *
 *  @param[out] status struct ras_action_status containing repair result
 *  and struct get_ras_action_data_in.
 */
oob_status_t get_bmc_ras_action_status(uint8_t soc_num,
				       struct get_ras_action_data_in data_in,
				       struct ras_action_status *status);

/*
 * @} */  // end of MailboxNDAMsg
/****************************************************************************/

#endif  // INCLUDE_ESMI_OOB_MAILBOX_NDA_H_
