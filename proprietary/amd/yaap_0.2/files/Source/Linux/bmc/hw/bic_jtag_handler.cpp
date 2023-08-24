#include "bic_jtag_handler.h"
#include "infrastructure/debug.h"
#include <facebook/bic_ipmi.h>
#include <facebook/bic_xfer.h>

#define IANA_SIZE 3
#define MIN( a , b ) ( ( a ) < ( b ) ) ? ( a ) : ( b )
#define MAX( a , b ) ( ( a ) > ( b ) ) ? ( a ) : ( b )
#define MAX_TRANSFER_BITS 0x400

namespace bmc
{

typedef struct {
    uint8_t tmsbits;
    uint8_t count;
} TmsCycle;

static uint8_t IANA[IANA_SIZE] = {0x15, 0xA0, 0x00};

// this is the complete set TMS cycles for going from any TAP state to any other TAP state, following a “shortest path” rule
static const TmsCycle tmsCycleLookup[][16] = {
/*   start*/ /*TLR      RTI      SelDR    CapDR    SDR      Ex1DR    PDR      Ex2DR    UpdDR    SelIR    CapIR    SIR      Ex1IR    PIR      Ex2IR    UpdIR    destination*/
/*     TLR*/{ {0x00,0},{0x00,1},{0x02,2},{0x02,3},{0x02,4},{0x0a,4},{0x0a,5},{0x2a,6},{0x1a,5},{0x06,3},{0x06,4},{0x06,5},{0x16,5},{0x16,6},{0x56,7},{0x36,6} },
/*     RTI*/{ {0x07,3},{0x00,0},{0x01,1},{0x01,2},{0x01,3},{0x05,3},{0x05,4},{0x15,5},{0x0d,4},{0x03,2},{0x03,3},{0x03,4},{0x0b,4},{0x0b,5},{0x2b,6},{0x1b,5} },
/*   SelDR*/{ {0x03,2},{0x03,3},{0x00,0},{0x00,1},{0x00,2},{0x02,2},{0x02,3},{0x0a,4},{0x06,3},{0x01,1},{0x01,2},{0x01,3},{0x05,3},{0x05,4},{0x15,5},{0x0d,4} },
/*   CapDR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x00,0},{0x00,1},{0x01,1},{0x01,2},{0x05,3},{0x03,2},{0x0f,4},{0x0f,5},{0x0f,6},{0x2f,6},{0x2f,7},{0xaf,8},{0x6f,7} },
/*     SDR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x07,4},{0x00,0},{0x01,1},{0x01,2},{0x05,3},{0x03,2},{0x0f,4},{0x0f,5},{0x0f,6},{0x2f,6},{0x2f,7},{0xaf,8},{0x6f,7} },
/*   Ex1DR*/{ {0x0f,4},{0x01,2},{0x03,2},{0x03,3},{0x02,3},{0x00,0},{0x00,1},{0x02,2},{0x01,1},{0x07,3},{0x07,4},{0x07,5},{0x17,5},{0x17,6},{0x57,7},{0x37,6} },
/*     PDR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x07,4},{0x01,2},{0x05,3},{0x00,0},{0x01,1},{0x03,2},{0x0f,4},{0x0f,5},{0x0f,6},{0x2f,6},{0x2f,7},{0xaf,8},{0x6f,7} },
/*   Ex2DR*/{ {0x0f,4},{0x01,2},{0x03,2},{0x03,3},{0x00,1},{0x02,2},{0x02,3},{0x00,0},{0x01,1},{0x07,3},{0x07,4},{0x07,5},{0x17,5},{0x17,6},{0x57,7},{0x37,6} },
/*   UpdDR*/{ {0x07,3},{0x00,1},{0x01,1},{0x01,2},{0x01,3},{0x05,3},{0x05,4},{0x15,5},{0x00,0},{0x03,2},{0x03,3},{0x03,4},{0x0b,4},{0x0b,5},{0x2b,6},{0x1b,5} },
/*   SelIR*/{ {0x01,1},{0x01,2},{0x05,3},{0x05,4},{0x05,5},{0x15,5},{0x15,6},{0x55,7},{0x35,6},{0x00,0},{0x00,1},{0x00,2},{0x02,2},{0x02,3},{0x0a,4},{0x06,3} },
/*   CapIR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x07,4},{0x07,5},{0x17,5},{0x17,6},{0x57,7},{0x37,6},{0x0f,4},{0x00,0},{0x00,1},{0x01,1},{0x01,2},{0x05,3},{0x03,2} },
/*     SIR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x07,4},{0x07,5},{0x17,5},{0x17,6},{0x57,7},{0x37,6},{0x0f,4},{0x0f,5},{0x00,0},{0x01,1},{0x01,2},{0x05,3},{0x03,2} },
/*   Ex1IR*/{ {0x0f,4},{0x01,2},{0x03,2},{0x03,3},{0x03,4},{0x0b,4},{0x0b,5},{0x2b,6},{0x1b,5},{0x07,3},{0x07,4},{0x02,3},{0x00,0},{0x00,1},{0x02,2},{0x01,1} },
/*     PIR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x07,4},{0x07,5},{0x17,5},{0x17,6},{0x57,7},{0x37,6},{0x0f,4},{0x0f,5},{0x01,2},{0x05,3},{0x00,0},{0x01,1},{0x03,2} },
/*   Ex2IR*/{ {0x0f,4},{0x01,2},{0x03,2},{0x03,3},{0x03,4},{0x0b,4},{0x0b,5},{0x2b,6},{0x1b,5},{0x07,3},{0x07,4},{0x00,1},{0x02,2},{0x02,3},{0x00,0},{0x01,1} },
/*   UpdIR*/{ {0x07,3},{0x00,1},{0x01,1},{0x01,2},{0x01,3},{0x05,3},{0x05,4},{0x15,5},{0x0d,4},{0x03,2},{0x03,3},{0x03,4},{0x0b,4},{0x0b,5},{0x2b,6},{0x00,0} },
};

static void jtagIpmbBufferPrint(uint8_t *buffer, size_t length)
{
    DEBUG_PRINT(DEBUG_VERBOSE,"========================================");
    DEBUG_BUFFER(DEBUG_VERBOSE, buffer, length);
    DEBUG_PRINT(DEBUG_VERBOSE,"========================================");
}

STATUS jtagIpmbWrapper(
    uint8_t fru, uint8_t netfn, uint8_t cmd,
    uint8_t *txbuf, size_t txlen,
    uint8_t *rxbuf, size_t *rxlen)
{
    uint8_t u8_rxlen;

    if (txbuf == NULL || rxbuf == NULL) {
        return ST_ERR;
    }

    if (bic_ipmb_wrapper(fru, netfn, cmd, txbuf, txlen, rxbuf, &u8_rxlen)) {
        DEBUG_PRINT(DEBUG_ERROR, "bic_ipmb_wrapper failed, slot%u\n", fru);
        DEBUG_PRINT(DEBUG_ERROR,"[DEBUG][TX_BUF]");
        DEBUG_BUFFER(DEBUG_ERROR, txbuf, txlen);
        return ST_ERR;
    }

    *rxlen = u8_rxlen;
    return ST_OK;
}

STATUS jtagIpmbShiftWrapper(
    uint8_t fru,
    uint32_t write_bit_length, uint8_t *write_data,
    uint32_t read_bit_length, uint8_t *read_data,
    uint32_t last_transaction)
{
    uint8_t tbuf[256] = {0x00};
    uint8_t rbuf[256] = {0x00};
    uint8_t write_len_bytes = ((write_bit_length+7) >> 3);
    size_t rlen = sizeof(rbuf);
    size_t tlen = 0;

    // tbuf[0:2]   = IANA ID
    // tbuf[3]     = write bit length (LSB)
    // tbuf[4]     = write bit length (MSB)
    // tbuf[5:n-1] = write data
    // tbuf[n]     = read bit length (LSB)
    // tbuf[n+1]   = read bit length (MSB)
    // tbuf[n+2]   = last transactions
    memcpy(tbuf, IANA, IANA_SIZE);
    tbuf[3] = write_bit_length & 0xFF;
    tbuf[4] = (write_bit_length >> 8) & 0xFF;
    if (write_data) {
        memcpy(&tbuf[5], write_data, write_len_bytes);
    }
    tbuf[5 + write_len_bytes] = read_bit_length & 0xFF;
    tbuf[6 + write_len_bytes] = (read_bit_length >> 8) & 0xFF;
    tbuf[7 + write_len_bytes] = last_transaction;

    tlen = write_len_bytes + 8;
    if (jtagIpmbWrapper(fru, NETFN_OEM_1S_REQ, CMD_OEM_1S_JTAG_SHIFT, tbuf, tlen, rbuf, &rlen) != ST_OK) {
        return ST_ERR;
    }
    if (read_bit_length && read_data && (rlen > IANA_SIZE)) {
        memcpy(read_data, &rbuf[IANA_SIZE], (rlen - IANA_SIZE));
    }

    return ST_OK;
}

STATUS jtagIpmbSetTapState(
    uint8_t fru,
    enum yaap::hal::jtagState curState,
    enum yaap::hal::jtagState endState)
{
    uint8_t tbuf[8] = {0x00};
    uint8_t rbuf[8] = {0x00};
    size_t tlen = 5;
    size_t rlen = sizeof(rbuf);

    // Jtag state is tap_state already.
    if (endState != JTAG_STATE_TLR && curState == endState) {
        return ST_OK;
    }

    memcpy(tbuf, IANA, IANA_SIZE);

    if (endState == JTAG_STATE_TLR) {
        tbuf[3] = 8;
        tbuf[4] = 0xff;
    } else {
        tbuf[3] = tmsCycleLookup[curState][endState].count;
        tbuf[4] = tmsCycleLookup[curState][endState].tmsbits;
    }

    // add delay count for 2 special cases
    if ((endState == JTAG_STATE_RTI) || (endState == JTAG_STATE_PAUSE_DR)) {
        tbuf[3] += 5;
    }

    DEBUG_PRINT(DEBUG_VERBOSE,"[DEBUG] jtagIpmbSetTapState(): fru: %u, curState: %d, endState: %d, count: %u, tms: 0x%02x", fru, curState, endState, tbuf[3], tbuf[4]);
    if (jtagIpmbWrapper(fru, NETFN_OEM_1S_REQ, CMD_OEM_1S_SET_TAP_STATE, tbuf, tlen, rbuf, &rlen) != ST_OK) {
        DEBUG_PRINT(DEBUG_ERROR, "Failed to set tap state, fru: %u, curState: %d, endState: %d",
                    fru, curState, endState);
        return ST_ERR;
    }

    return ST_OK;
}

STATUS jtagIpmbResetState(uint8_t fru)
{
    uint8_t tbuf[8] = {0x00};
    uint8_t rbuf[8] = {0x00};
    size_t tlen = 5;
    size_t rlen = sizeof(rbuf);

    memcpy(tbuf, IANA, IANA_SIZE);
    tbuf[3] = 8;
    tbuf[4] = 0xff;

    DEBUG_PRINT(DEBUG_VERBOSE,"[DEBUG] jtagIpmbResetState(): fru: %u", fru);
    if (jtagIpmbWrapper(fru, NETFN_OEM_1S_REQ, CMD_OEM_1S_SET_TAP_STATE, tbuf, tlen, rbuf, &rlen) != ST_OK) {
        DEBUG_PRINT(DEBUG_ERROR, "Failed to reset tap state");
        return ST_ERR;
    }

    return ST_OK;
}


BicJtagHandler::BicJtagHandler(uint8_t fruid) :
    m_fruid(fruid),
    m_state(yaap::hal::JTAG_STATE_TLR),
    m_freq(8250000)
{}

BicJtagHandler::~BicJtagHandler()
{}

uint32_t BicJtagHandler::shift(enum jtagOp operation, uint8_t *inData, uint8_t *outData, int bits, [[maybe_unused]] int preTck, [[maybe_unused]] int postTck, enum jtagState termState)
{
    int last_transaction = 0;
    int transfer_bit_length = bits;
    int write_bit_length = bits;
    int read_bit_length = bits;
    int this_write_bit_length = MIN(write_bit_length, MAX_TRANSFER_BITS);
    int this_read_bit_length  = MIN(read_bit_length, MAX_TRANSFER_BITS);
    uint8_t *tdi_buffer = inData;
    uint8_t *tdo_buffer = outData;
    STATUS ret;

    DEBUG_PRINT(DEBUG_INFO, "YAAP IOCTL SHIFT Operation:%d, BITs Count: 0x%x, TermState: 0x%x\n", operation, bits, termState);
    
    // go to shift state
    enum jtagState shiftState = (operation == yaap::hal::JTAG_OP_SHIFT_DR) ? JTAG_STATE_SHIFT_DR : JTAG_STATE_SHIFT_IR;
    DEBUG_PRINT(DEBUG_VERBOSE, "[DEBUG] m_state = %u, shiftState = %u", m_state, shiftState);
    if (m_state != shiftState) {
        if (jtagIpmbSetTapState(m_fruid, m_state, shiftState) != ST_OK) {
            DEBUG_PRINT(DEBUG_ERROR, "Failed to go state %d (shiftState)", shiftState);
            return E_SERVER_ERROR;
        }
    }
    
    // shift
    while (transfer_bit_length)
    {
        int this_write_bit_length = MIN(write_bit_length, MAX_TRANSFER_BITS);
        int this_read_bit_length  = MIN(read_bit_length, MAX_TRANSFER_BITS);

        // check if we entered illegal state
        if (this_write_bit_length < 0 || this_read_bit_length < 0 ||
            last_transaction == 1 ||
            (this_write_bit_length == 0 && this_read_bit_length == 0)) {
            DEBUG_PRINT(DEBUG_ERROR, "slot=%d, ERROR: invalid read write length. read=%d, write=%d, last_transaction=%d\n",
                    m_fruid, this_read_bit_length, this_write_bit_length,
                    last_transaction);
            return E_SERVER_ERROR;
        }

        transfer_bit_length -= MAX(this_write_bit_length, this_read_bit_length);
        write_bit_length -= this_write_bit_length;
        read_bit_length  -= this_read_bit_length;
        last_transaction = (transfer_bit_length <= 0) ? 1 : 0;

        ret = jtagIpmbShiftWrapper(m_fruid,
                                   this_write_bit_length, tdi_buffer,
                                   this_read_bit_length, tdo_buffer,
                                   last_transaction);

        if (ret != ST_OK) {
            DEBUG_PRINT(DEBUG_ERROR, "%s: ERROR, jtagIpmbShiftWrapper failed, slot%d",
                   __FUNCTION__, m_fruid);
            return E_SERVER_ERROR;
        }

        tdi_buffer += (this_write_bit_length >> 3);
        tdo_buffer += (this_read_bit_length >> 3);
    }

    m_state = (operation == yaap::hal::JTAG_OP_SHIFT_DR) ? JTAG_STATE_EXIT1_DR : JTAG_STATE_EXIT1_IR;

    DEBUG_PRINT(DEBUG_VERBOSE,"[DEBUG] inData (bits: %u)", bits);
    jtagIpmbBufferPrint(inData, ((bits+7) >> 3));
    DEBUG_PRINT(DEBUG_VERBOSE,"[DEBUG] outData (bits: %u)", bits);
    jtagIpmbBufferPrint(outData, ((bits+7) >> 3));

    // go to end_tap_state as requested
    DEBUG_PRINT(DEBUG_VERBOSE, "[DEBUG] m_state = %u, termState = %u", m_state, termState);
    if (m_state != termState) {
        if (jtagIpmbSetTapState(m_fruid, m_state, termState) != ST_OK) {
            DEBUG_PRINT(DEBUG_ERROR, "Failed to go state %d (endState)", termState);
            return E_SERVER_ERROR;
        }
    }

    m_state = termState;
    return E_SUCCESS;
}

uint32_t BicJtagHandler::resetTap(void)
{
    DEBUG_PRINT(DEBUG_INFO, "tapState reset");
    if (jtagIpmbResetState(m_fruid) != ST_OK) {
        DEBUG_PRINT(DEBUG_ERROR, "Failed to reset state");
        return E_SERVER_ERROR;
    }

    m_state = yaap::hal::JTAG_STATE_TLR;
    return E_SUCCESS;
}

uint32_t BicJtagHandler::tapStateGet(enum jtagState& state)
{
    state = m_state;
    return E_SUCCESS;
}

uint32_t BicJtagHandler::tapStateSet(enum jtagState state)
{
    DEBUG_PRINT(DEBUG_INFO, "tapState to Set %d", state);
    if (jtagIpmbSetTapState(m_fruid, m_state, state) != ST_OK) {
        DEBUG_PRINT(DEBUG_ERROR, "Failed to set state %d", state);
        return E_SERVER_ERROR;
    }

    m_state = state;
    return E_SUCCESS;
}

uint32_t BicJtagHandler::tckFreqSet(uint32_t& freqHz)
{
    DEBUG_PRINT(DEBUG_INFO, "input freq is %u", freqHz);
    m_freq = freqHz;
    return E_SUCCESS;
}

uint32_t BicJtagHandler::tckFreqGet(uint32_t& freqHz)
{
    freqHz = m_freq;
    return E_SUCCESS;
}

} // namespace bmc
