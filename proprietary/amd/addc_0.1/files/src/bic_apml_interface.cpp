#include "bic_apml_interface.hpp"

#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <facebook/bic_xfer.h>
#include <openbmc/ipmi.h>

/* one byte mask for DDR bandwidth */
#define ONE_BYTE_MASK		0XFF
/* Teo byte mask */
#define TWO_BYTE_MASK		0xFFFF
/* Four byte mask used in raplcoreenergy, raplpackageenergy */
#define FOUR_BYTE_MASK		0xFFFFFFFF
/* LO_WORD_REG used in rapl package energy and read RAS
 * last transaction address */
#define LO_WORD_REG		0
/* HI_WORD_REG used in rapl package energy and read RAS
 * last transaction address */
#define HI_WORD_REG		1
/* READ MODE */
#define READ_MODE		1
/* WRITE MODE */
#define WRITE_MODE		0

#define MAX_RETRIES              5
// #define MAX_RETRY_INTERVAL_MSEC  128
#define IANA_SIZE 3

enum IPMB_APML_OEM_CMD
{
    CMD_OEM_APML_READ_DATA = 0x2C,
    CMD_OEM_APML_WRITE_DATA = 0x2D,
    CMD_OEM_APML_SEND_REQ = 0x2E,
    CMD_OEM_APML_GET_RES = 0x2F,
};

enum APML_INTERFACE_TYPE
{
    APML_RMI = 0x00,
    APML_TSI = 0x01,
};

enum APML_DATA_FUNC
{
    APML_DATA_WRITE = 0x00,
    APML_DATA_READ = 0x01,
};

enum APML_MSG_TYPE
{
	APML_MSG_TYPE_MAILBOX = 0x00,
	APML_MSG_TYPE_CPUID,
	APML_MSG_TYPE_MCA,
};

static uint8_t g_fruid = 0;

uint8_t get_current_target_fruid()
{
    return g_fruid;
}

void set_current_target_fruid(uint8_t fruid)
{
    g_fruid = fruid;
}

oob_status_t bic_ipmb_apml_read_byte(struct apml_message *msg, uint8_t intf_type)
{
    const uint8_t iana[IANA_SIZE] = {0x15, 0xa0, 0x00};
    uint8_t tbuf[256] = {0x00};
    uint8_t rbuf[256] = {0x00};
    uint8_t tlen = 0;
    uint8_t rlen = 255;
    int ret;

    memcpy(tbuf, iana, sizeof(iana));
    tbuf[3] = intf_type;
    tbuf[4] = msg->data_in.reg_in[0];
    tlen = 5;

    ret = bic_ipmb_wrapper(g_fruid, NETFN_OEM_1S_REQ, CMD_OEM_APML_READ_DATA, tbuf, tlen, rbuf, &rlen);
    if (ret != BIC_STATUS_SUCCESS)
        return OOB_UNKNOWN_ERROR;

    msg->data_out.reg_out[0] = rbuf[3];
    return OOB_SUCCESS;
}

oob_status_t bic_ipmb_apml_write_byte(struct apml_message *msg, uint8_t intf_type)
{
    const uint8_t iana[IANA_SIZE] = {0x15, 0xa0, 0x00};
    uint8_t tbuf[256] = {0x00};
    uint8_t rbuf[256] = {0x00};
    uint8_t tlen = 0;
    uint8_t rlen = 255;
    int ret;

    memcpy(tbuf, iana, sizeof(iana));
    tbuf[3] = intf_type;
    tbuf[4] = msg->data_in.reg_in[0];
    tbuf[5] = msg->data_in.reg_in[4];
    tlen = 6;

    ret = bic_ipmb_wrapper(g_fruid, NETFN_OEM_1S_REQ, CMD_OEM_APML_WRITE_DATA, tbuf, tlen, rbuf, &rlen);
    if (ret != BIC_STATUS_SUCCESS)
        return OOB_UNKNOWN_ERROR;

    return OOB_SUCCESS;
}

oob_status_t bic_ipmb_apml_request(struct apml_message *msg)
{
    const uint8_t iana[IANA_SIZE] = {0x15, 0xa0, 0x00};
    uint8_t tbuf[256] = {0x00};
    uint8_t rbuf[256] = {0x00};
    uint8_t tlen = 0;
    uint8_t rlen = 255;
    uint16_t retries = 0;
    int ret;

    memcpy(tbuf, iana, sizeof(iana));
    ((uint32_t*)(tbuf+5))[0] = msg->data_in.mb_in[0];
    if (msg->cmd == 0x1000)
    {
        tbuf[3] = APML_MSG_TYPE_CPUID;
        tbuf[4] = msg->data_in.reg_in[4];
        tbuf[9] = msg->data_in.reg_in[6];
        tlen = 10;
    }
    else if (msg->cmd == 0x1001)
    {
        tbuf[3] = APML_MSG_TYPE_MCA;
        tbuf[4] = msg->data_in.reg_in[4];
        tlen = 9;
    }
    else
    {
        tbuf[3] = APML_MSG_TYPE_MAILBOX;
        tbuf[4] = (uint8_t)msg->cmd;
        tlen = 9;
    }

    ret = bic_ipmb_wrapper(g_fruid, NETFN_OEM_1S_REQ, CMD_OEM_APML_SEND_REQ, tbuf, tlen, rbuf, &rlen);
    if (ret != BIC_STATUS_SUCCESS)
        return OOB_UNKNOWN_ERROR;

    memcpy(tbuf, iana, sizeof(iana));
    tbuf[3] = rbuf[3];
    tlen = 4;
    memset(rbuf, 0, sizeof(rbuf));
    rlen = 255;

    while (retries < MAX_RETRIES)
    {
        sleep(1);
        ret = bic_ipmb_wrapper(g_fruid, NETFN_OEM_1S_REQ, CMD_OEM_APML_GET_RES, tbuf, tlen, rbuf, &rlen);
        if (ret == BIC_STATUS_SUCCESS)
            break;

        retries++;
    }

    if (ret != BIC_STATUS_SUCCESS)
        return OOB_UNKNOWN_ERROR;

    if (msg->cmd == 0x1000 || msg->cmd == 0x1001)
    {
        msg->fw_ret_code = rbuf[3] ;
        msg->data_out.cpu_msr_out = ((uint64_t*)(rbuf + 4))[0];
        if (rbuf[3])
            ret = OOB_CPUID_MSR_ERR_BASE + msg->fw_ret_code;
        else
            ret = OOB_SUCCESS;
    }
    else
    {
        msg->data_out.mb_out[0] = ((uint32_t*)(rbuf + 4))[0];
        ret = OOB_SUCCESS;
    }

    return (oob_status_t)ret;
}

//===================================================================
// src/esmi_oob/apml.c
//===================================================================

oob_status_t sbrmi_xfer_msg(uint8_t socket_num, char *filename, struct apml_message *msg)
{
    oob_status_t ret;

    if (!(msg->cmd & 0xF000) || msg->cmd == 0x1000 || msg->cmd == 0x1001)
    {
        ret = bic_ipmb_apml_request(msg);
    }
    else if (msg->cmd == 0x1002 && msg->data_in.reg_in[7] == APML_DATA_READ)
    {
        ret = bic_ipmb_apml_read_byte(msg, (std::string(filename) == SBRMI) ? APML_RMI : APML_TSI);
    }
    else if (msg->cmd == 0x1002 && msg->data_in.reg_in[7] == APML_DATA_WRITE)
    {
        ret = bic_ipmb_apml_write_byte(msg, (std::string(filename) == SBRMI) ? APML_RMI : APML_TSI);
    }
    else
    {
        ret = OOB_NOT_SUPPORTED;
    }

    return ret;
}

oob_status_t esmi_oob_read_byte(uint8_t soc_num, uint8_t reg_offset, char *file_name, uint8_t *buffer)
{
    struct apml_message msg = {0};
    oob_status_t ret;

    /* NULL Pointer check */
    if (!buffer)
        return OOB_ARG_PTR_NULL;
    /* Readi/write register command is 0x1002 */
    msg.cmd = 0x1002;
    /* Assign register_offset to msg.data_in[0] */
    msg.data_in.reg_in[0] = reg_offset;
    /* Assign 1  to the msg.data_in[7] for the read operation */
    msg.data_in.reg_in[7] = 1;

    ret = sbrmi_xfer_msg(soc_num, file_name, &msg);
    if (ret)
        return ret;

    *buffer = msg.data_out.reg_out[0];

    return OOB_SUCCESS;
}

oob_status_t esmi_oob_write_byte(uint8_t soc_num, uint8_t reg_offset, char *file_name, uint8_t value)
{
    struct apml_message msg = {0};

    /* Read/Write register command is 0x1002 */
    msg.cmd = 0x1002;
    /* Assign register_offset to msg.data_in[0] */
    msg.data_in.reg_in[0] = reg_offset;

    /* Assign value to write to the data_in[4] */
    msg.data_in.reg_in[4] = value;

    /* Assign 0 to the msg.data_in[7] */
    msg.data_in.reg_in[7] = 0;

    return sbrmi_xfer_msg(soc_num, file_name, &msg);
}

oob_status_t esmi_oob_read_mailbox(uint8_t soc_num, uint32_t cmd, uint32_t input, uint32_t *buffer)
{
    struct apml_message msg = {0};
    oob_status_t ret = OOB_SUCCESS;

    /* NULL pointer check */
    if (!buffer)
        return OOB_ARG_PTR_NULL;

    msg.cmd = cmd;
    msg.data_in.mb_in[0] = input;

    msg.data_in.mb_in[1] = (uint32_t)READ_MODE << 24;
    ret = sbrmi_xfer_msg(soc_num, SBRMI, &msg);
    if (ret)
        return ret;

    *buffer = msg.data_out.mb_out[0];
    return OOB_SUCCESS;
}

//===================================================================
// src/esmi_oob/esmi_cpuid_msr.c
//===================================================================

oob_status_t esmi_oob_cpuid(uint8_t soc_num, uint32_t thread, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    uint32_t fn_eax, fn_ecx;
    oob_status_t ret;

    fn_eax = *eax;
    fn_ecx = *ecx;

    ret = esmi_oob_cpuid_eax(soc_num, thread, fn_eax, fn_ecx, eax);
    if (ret)
        return ret;
    
    ret = esmi_oob_cpuid_ebx(soc_num, thread, fn_eax, fn_ecx, ebx);
    if (ret)
        return ret;
    
    ret = esmi_oob_cpuid_ecx(soc_num, thread, fn_eax, fn_ecx, ecx);
    if (ret)
        return ret;

    return esmi_oob_cpuid_edx(soc_num, thread, fn_eax, fn_ecx, edx);
}

static oob_status_t esmi_oob_cpuid_fn(uint8_t soc_num, uint32_t thread, uint32_t fn_eax, uint32_t fn_ecx, uint8_t mode, uint32_t *value)
{
    struct apml_message msg = {0};
    uint8_t ext = 0, read_reg = 0;
    oob_status_t ret;

    if (!value)
        return OOB_ARG_PTR_NULL;

    /* cmd for CPUID is 0x1000 */
    msg.cmd = 0x1000;
    msg.data_in.cpu_msr_in = fn_eax;

    /* Assign thread number to data_in[4:5] */
    msg.data_in.cpu_msr_in = msg.data_in.cpu_msr_in
                 | ((uint64_t)thread << 32);

    /* Assign extended function to data_in[6][4:7] */
    if (mode == EAX || mode == EBX)
        /* read eax/ebx */
        read_reg = 0;
    else
        /* read ecx/edx */
        read_reg = 1;

    ext = (uint8_t)fn_ecx;
        ext = ext << 4 | read_reg;
        msg.data_in.cpu_msr_in = msg.data_in.cpu_msr_in | ((uint64_t) ext << 48);
    /* Assign 7 byte to READ Mode */
    msg.data_in.reg_in[7] = 1;
        ret = sbrmi_xfer_msg(soc_num, SBRMI, &msg);
        if (ret)
                return ret;

    if (mode == EAX || mode == ECX)
        /* Read low word/mbout[0] */
        *value = msg.data_out.mb_out[0];
    else
        /* Read high word/mbout[1] */
        *value = msg.data_out.mb_out[1];

    return OOB_SUCCESS;
}

oob_status_t esmi_oob_cpuid_eax(uint8_t soc_num,
                uint32_t thread, uint32_t fn_eax,
                uint32_t fn_ecx, uint32_t *eax)
{
    return esmi_oob_cpuid_fn(soc_num, thread, fn_eax, fn_ecx,
                 EAX, eax);
}

oob_status_t esmi_oob_cpuid_ebx(uint8_t soc_num,
                uint32_t thread, uint32_t fn_eax,
                uint32_t fn_ecx, uint32_t *ebx)
{
    return esmi_oob_cpuid_fn(soc_num, thread, fn_eax, fn_ecx,
                 EBX, ebx);
}

oob_status_t esmi_oob_cpuid_ecx(uint8_t soc_num,
                uint32_t thread, uint32_t fn_eax,
                uint32_t fn_ecx, uint32_t *ecx)
{
        return esmi_oob_cpuid_fn(soc_num, thread, fn_eax, fn_ecx,
                 ECX, ecx);
}

oob_status_t esmi_oob_cpuid_edx(uint8_t soc_num,
                uint32_t thread, uint32_t fn_eax,
                uint32_t fn_ecx, uint32_t *edx)
{
        return esmi_oob_cpuid_fn(soc_num, thread, fn_eax, fn_ecx,
                 EDX, edx);
}

//===================================================================
// src/esmi_oob/esmi_mailbox.c
//===================================================================

oob_status_t read_bmc_ras_mca_msr_dump(
    uint8_t soc_num, struct mca_bank mca_dump, uint32_t *buffer)
{
    uint32_t input;

    input = mca_dump.index << 16 | mca_dump.offset;
    return esmi_oob_read_mailbox(soc_num, READ_BMC_RAS_MCA_MSR_DUMP, input, buffer);
}

oob_status_t read_bmc_ras_mca_validity_check(
    uint8_t soc_num, uint16_t *bytes_per_mca, uint16_t *mca_banks)
{
    uint32_t output;
    oob_status_t ret;

    if ((!mca_banks) || (!bytes_per_mca))
        return OOB_ARG_PTR_NULL;

    ret = esmi_oob_read_mailbox(soc_num, READ_BMC_RAS_MCA_VALIDITY_CHECK, 0, &output);
    if (ret)
        return ret;

    *bytes_per_mca = output >> 16;
    *mca_banks = output & TWO_BYTE_MASK;

    return ret;
}

oob_status_t reset_on_sync_flood(uint8_t soc_num, uint32_t *ack_resp)
{
	/* At present, Only P0 handles this request */
	soc_num = 0;
	return esmi_oob_read_mailbox(soc_num, READ_BMC_RAS_RESET_ON_SYNC_FLOOD,
				     0, ack_resp);
}

//===================================================================
// src/esmi_oob/esmi_rmi.c
//===================================================================

oob_status_t read_sbrmi_ras_status(uint8_t soc_num,
                   uint8_t *buffer)
{
    oob_status_t ret;

    ret = esmi_oob_read_byte(soc_num,
                 SBRMI_RASSTATUS, SBRMI, buffer);
    if (ret)
        return ret;

    /* BMC should write 1 to clear them, make way for next update */
    return esmi_oob_write_byte(soc_num,
                   SBRMI_RASSTATUS, SBRMI, *buffer);
}

//===================================================================
// src/esmi_oob/esmi_mailbox_nda.c
//===================================================================
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