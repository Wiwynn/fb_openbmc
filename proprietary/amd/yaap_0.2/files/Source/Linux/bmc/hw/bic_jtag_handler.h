#ifndef BMC_HW_BIC_JTAG_HANDLER_H
#define BMC_HW_BIC_JTAG_HANDLER_H

#include "aspeed.h"
#include "hal/jtag.h"

using namespace yaap::hal;

namespace bmc
{

typedef enum
{
    ST_OK,
    ST_ERR,
    ST_TIMEOUT
} STATUS;

STATUS jtagIpmbWrapper(
    uint8_t fru, uint8_t netfn, uint8_t cmd,
    uint8_t *txbuf, size_t txlen,
    uint8_t *rxbuf, size_t *rxlen);

STATUS jtagIpmbShiftWrapper(
    uint8_t fru,
    uint32_t write_bit_length, uint8_t *write_data,
    uint32_t read_bit_length, uint8_t *read_data,
    uint32_t last_transaction);

STATUS jtagIpmbSetTapState(
    uint8_t fru,
    enum yaap::hal::jtagState curState,
    enum yaap::hal::jtagState endState);

STATUS jtagIpmbResetState(uint8_t fru);


class BicJtagHandler : public IJtag, public IJtag_tapState, public IJtag_tckFrequency
{
public:
    BicJtagHandler(uint8_t fruid);
    virtual ~BicJtagHandler();

    // IJtag
    uint32_t shift(enum jtagOp operation, uint8_t *inData, uint8_t *outData, int bits, int preTck, int postTck, enum jtagState termState);
    uint32_t resetTap(void);

    // IJtag_tapState
    uint32_t tapStateGet(enum jtagState& state);
    uint32_t tapStateSet(enum jtagState state);

    // IJtag_tckFrequency
    uint32_t tckFreqSet(uint32_t& freqHz);
    uint32_t tckFreqGet(uint32_t& freqHz);

private:
    uint8_t                     m_fruid;
    enum yaap::hal::jtagState   m_state;
    uint32_t                    m_freq;
};

} // namespace bmc
#endif
