/*
 *
 * Copyright 2017-present Facebook. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "yaapBicWrap.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <facebook/bic.h>

#define BIC_IHDT_OEM_NETFN  0x38
#define BIC_IHDT_INIT_CMD   0x81
#define BIC_IHDT_REQ_CMD    0x82
#define BIC_IHDT_RSP_CMD    0x83

#define BIC_IHDT_SEGMENT_SIZE   240
#define BIC_IPMI_MAX_DATA_SIZE  250
#define SIZE_IANA_ID 3
uint8_t BIC_OEM_IANA_ID[SIZE_IANA_ID] = {0x15, 0xA0, 0x00}; // IANA ID

typedef struct BicIHDTPktHeader_t
{
    uint8_t     iana[3];

    uint8_t     ver;
    uint8_t     type;
    uint16_t    curr_page;
    uint16_t    total_page;
    uint32_t    total_length;
} __attribute__((packed)) BicIHDTPktHeader;

typedef struct BicIHDTReqPkt_t
{
    BicIHDTPktHeader    header;
    uint8_t             data[BIC_IPMI_MAX_DATA_SIZE];
} __attribute__((packed)) BicIHDTReqPkt;

typedef struct BicIHDTRspPkt_t
{
    BicIHDTPktHeader    header;
    uint8_t             data[BIC_IPMI_MAX_DATA_SIZE];
} __attribute__((packed)) BicIHDTRspPkt;



namespace {
void rawBufferDump(const uint8_t *buf, const size_t length)
{
    size_t i;

    printf("buffer length: %lu\n", (unsigned long) length);
    for (i = 0; i < length; i++) {
        printf("0x%02x ", buf[i]);
        if (i%16 == 15) printf("\n");
    }
    printf("\n");
}
}

namespace yaap_bic {

//==============================================================================
// BicWrapConfig
//==============================================================================
BicWrapConfigPtr BicWrapConfig::s_instance_sptr = NULL;


//==============================================================================
// BicWrapBackend
//==============================================================================
BicWrapBackend::BicWrapBackend(uint32_t fruid)
: m_fruid(fruid)
{}

/*----------------------------------------------------------------------------*/
BicWrapBackend::~BicWrapBackend()
{}

/*----------------------------------------------------------------------------*/
void printrawBufferHelper(int ret, char* title, uint8_t *txbuf, uint16_t txlen,
                  uint8_t *rxbuf, uint8_t rxlen) {
    if (ret != 0)
        rxlen = 0;

    if(BicWrapConfig::getInstancePtr()->isDebugEnabled())
    {
        printf("===== %s =====\n",title);
        printf("[txbuf]---------------------------------\n");
        rawBufferDump(txbuf, txlen);
        printf("----------------------------------------\n");
        printf("[rxbuf]---------------------------------\n");
        rawBufferDump(rxbuf, rxlen);
        printf("----------------------------------------\n");
    }
}

/*----------------------------------------------------------------------------*/
int BicWrapBackend::sendYaapHeader(
        uint32_t lock_id, uint32_t method_cnt,
        uint32_t total_page, uint16_t &curr_page)
{
    int ret;
    uint8_t txbuf[32];
    uint16_t txlen = sizeof(BicIHDTPktHeader)+8;
    uint8_t rxbuf[BIC_IPMI_MAX_DATA_SIZE];
    uint8_t rxlen = BIC_IPMI_MAX_DATA_SIZE;
    BicIHDTPktHeader *txheader = (BicIHDTPktHeader *)txbuf;

    memcpy(txheader->iana, BIC_OEM_IANA_ID, SIZE_IANA_ID);
    txheader->ver = 0;
    txheader->type = 0;
    txheader->curr_page = curr_page;
    txheader->total_page = total_page;
    txheader->total_length = 8;
    uint32_t tmp_lock_id = htonl(lock_id);
    uint32_t tmp_method_cnt = htonl(method_cnt);
    memcpy(&txbuf[13+0], &tmp_lock_id, sizeof(uint32_t));
    memcpy(&txbuf[13+4], &tmp_method_cnt, sizeof(uint32_t));

    ret = bic_ipmb_wrapper(m_fruid, BIC_IHDT_OEM_NETFN, BIC_IHDT_REQ_CMD,
            (uint8_t*) txbuf, txlen, rxbuf, &rxlen);

    printrawBufferHelper(ret, (char*)"sendYaapHeader",(uint8_t*) txbuf, txlen, rxbuf, rxlen);

    if (ret != 0)
    {
        printf("%s(): BIC IPMB failed, ret: %d\n", __func__, ret);
        goto error_out;
    }

    curr_page += 1;
    return E_SUCCESS;

error_out:
    return ret;
}

/*----------------------------------------------------------------------------*/
int BicWrapBackend::sendYaapMethodRequest(
        uint32_t instance_id, uint32_t method_id, uint32_t payload_sz,
        BasicDataBuffer payload,
        uint32_t total_page, uint16_t &curr_page)
{
    int ret;
    size_t processed_cnt;
    uint8_t txbuf[255] = {0};
    uint16_t txlen = 0;
    uint8_t rxbuf[BIC_IPMI_MAX_DATA_SIZE] = {0};
    uint8_t rxlen = BIC_IPMI_MAX_DATA_SIZE;
    BicIHDTPktHeader *txheader = (BicIHDTPktHeader *)txbuf;

    memcpy(txheader->iana, BIC_OEM_IANA_ID, SIZE_IANA_ID);
    txheader->ver = 0;
    txheader->type = 0;
    txheader->total_page = total_page;

    // send out method header
    txheader->curr_page = curr_page;
    txheader->total_length = 12;
    uint32_t tmp_instance_id = htonl(instance_id);
    uint32_t tmp_method_id = htonl(method_id);
    uint32_t tmp_payload_sz = htonl(payload_sz);
    memcpy(&txbuf[13+0], &tmp_instance_id, sizeof(uint32_t));
    memcpy(&txbuf[13+4], &tmp_method_id, sizeof(uint32_t));
    memcpy(&txbuf[13+8], &tmp_payload_sz, sizeof(uint32_t));
    txlen = sizeof(BicIHDTPktHeader) + 12;

    ret = bic_ipmb_wrapper(m_fruid, BIC_IHDT_OEM_NETFN, BIC_IHDT_REQ_CMD,
            (uint8_t*) txbuf, txlen, rxbuf, &rxlen);

    printrawBufferHelper(ret, (char*)"sendYaapMethodRequest Header",(uint8_t*) txbuf, txlen, rxbuf, rxlen);

    if (ret != 0)
    {
        printf("%s(): method header, BIC IPMB failed, ret: %d\n", __func__, ret);
        return ret;
    }

    curr_page += 1;

    // send out method payload
    if (payload_sz == 0)
    {
        // no more data need to send
        goto out;
    }

    processed_cnt = 0;
    do {
        size_t seg_sz = (payload_sz - processed_cnt);

        seg_sz = (seg_sz > BIC_IHDT_SEGMENT_SIZE) ? BIC_IHDT_SEGMENT_SIZE : seg_sz;
        txheader->curr_page = curr_page;
        txheader->total_length = seg_sz;
        memcpy(&txbuf[13+0], payload.data(), payload.size());
        txlen = sizeof(BicIHDTPktHeader) + seg_sz;

        ret = bic_ipmb_wrapper(m_fruid, BIC_IHDT_OEM_NETFN, BIC_IHDT_REQ_CMD,
                (uint8_t*) txbuf, txlen, rxbuf, &rxlen);

        printrawBufferHelper(ret, (char*)"sendYaapMethodRequest Payload",(uint8_t*) txbuf, txlen, rxbuf, rxlen);

        if (ret != 0)
        {
            printf("%s(): method payload, BIC IPMB failed, ret: %d\n", __func__, ret);
            goto error_out;
        }

        processed_cnt += seg_sz;
        curr_page += 1;
    }while(processed_cnt < payload_sz);

out:
    return E_SUCCESS;

error_out:
    return ret;
}

/*----------------------------------------------------------------------------*/
int BicWrapBackend::getYaapMethodResponse(
        BasicDataBuffer& res_buf, uint8_t res_type)
{
    int ret;
    uint8_t txbuf[32] = {0};
    uint16_t txlen = 0;
    uint8_t rxbuf[BIC_IPMI_MAX_DATA_SIZE] = {0};
    uint8_t rxlen = BIC_IPMI_MAX_DATA_SIZE;

    uint16_t curr_page = 0;
    uint16_t total_pages = 1;
    BicIHDTPktHeader *txheader = (BicIHDTPktHeader *)txbuf;
    BicIHDTPktHeader *rxheader;

    memcpy(txheader->iana, BIC_OEM_IANA_ID, SIZE_IANA_ID);
    txheader->ver = 0;
    txheader->type = res_type;
    txheader->total_length = 0;
    txlen = sizeof(BicIHDTPktHeader);
    rxlen = BIC_IPMI_MAX_DATA_SIZE;

    do {
        txheader->curr_page = curr_page;
        txheader->total_page = total_pages;
        rxlen = BIC_IPMI_MAX_DATA_SIZE;

        ret = bic_ipmb_wrapper(m_fruid, BIC_IHDT_OEM_NETFN, BIC_IHDT_RSP_CMD,
                                (uint8_t*) txbuf, txlen, rxbuf, &rxlen);

        printrawBufferHelper(ret, (char*)"getYaapMethodResponse",(uint8_t*) txbuf, txlen, rxbuf, rxlen);

        if (ret != 0)
        {
            printf("%s(): method payload, BIC IPMB failed, ret: %d\n", __func__, ret);
            return ret;
        }

        rxheader = (BicIHDTPktHeader *) rxbuf;
        curr_page = rxheader->curr_page;
        total_pages = rxheader->total_page;
        res_buf.insert(res_buf.end(), (rxbuf+13), (rxbuf+rxlen));
        curr_page += 1;

    } while (curr_page < total_pages);

    // printf("[debug]-------------------------------\n");
    // printf("    bufvec.size(): %u\n", res_buf.size());
    // rawBufferDump(res_buf.data(), res_buf.size());
    // printf("--------------------------------------\n");

    return E_SUCCESS;
}

/*----------------------------------------------------------------------------*/
uint32_t BicWrapBackend::checkTotalPages(BasicDataBuffer buffer)
{
    size_t curr_idx;
    size_t buf_sz = buffer.size();
    uint8_t *buf_data = buffer.data();
    uint32_t total_page = 0;

    // yaaprd header
    //uint32_t lock_id, method_cnt;
    //lock_id = ntohl(*((uint32_t*)(buf_data+0)));
    //method_cnt = ntohl(*((uint32_t*)(buf_data+4)));
    total_page += 1;

    curr_idx = 8;
    do {
        // method header
        //uint32_t instance_id = ntohl(*((uint32_t*)(buf_data+curr_idx+0)));
        //uint32_t method_id = ntohl(*((uint32_t*)(buf_data+curr_idx+4)));
        uint32_t payload_size = ntohl(*((uint32_t*)(buf_data+curr_idx+8)));
        total_page += 1;

        // method payload
        total_page += (payload_size/BIC_IHDT_SEGMENT_SIZE);
        total_page += (payload_size%BIC_IHDT_SEGMENT_SIZE) ? 1:0;

        curr_idx += (payload_size+12);
    } while (curr_idx < buf_sz);

    return total_page;
}


//==============================================================================
// Other functions
//==============================================================================

bool sendRawData(int fd, BasicDataBuffer &buffer)
{
    size_t bytes_sent = 0;
    ssize_t retval;
    int flags = 0;

    do {
        retval = send(fd, ((const char*)buffer.data() + bytes_sent), (buffer.size() - bytes_sent), flags);
        if (retval == -1)
        {
            perror("send()");
            return false;
        }
        bytes_sent += retval;
    } while (bytes_sent < buffer.size());

    return true;
}

} // namespace bic
