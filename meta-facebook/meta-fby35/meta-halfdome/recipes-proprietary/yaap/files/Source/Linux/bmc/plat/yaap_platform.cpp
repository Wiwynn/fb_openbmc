#include "plat/plat_def.h"

#include <map>
#include <iostream>
#include <facebook/bic.h>
#include <facebook/bic_ipmi.h>
#include <facebook/bic_xfer.h>
#include "infrastructure/debug.h"

class BicGpio : public GpioHelper
{
public:
    BicGpio(const std::string& name, uint8_t fruid, uint8_t gpio_num, bool  invert = false):
        GpioHelper(name), m_fruid(fruid), m_gpio_num(gpio_num), m_invert(invert)
    {}
    virtual int getValue()
    {
        uint8_t val_ipmb, val;
        DEBUG_PRINT(DEBUG_VERBOSE, "BicGpio::getValue(): bic get gpio, fru = %u, name = %s, invert: %d", m_fruid, m_name.c_str(), m_invert);
        if (bic_get_one_gpio_status(m_fruid, m_gpio_num, &val_ipmb))
        {
            DEBUG_PRINT(DEBUG_VERBOSE, "BicGpio::getValue(): bic get gpio failed, fru = %u, name = %s", m_fruid, m_name.c_str());
            return -1;
        }

        val = (m_invert) ? ((val_ipmb + 1) % 2) : val_ipmb;
        DEBUG_PRINT(DEBUG_VERBOSE, "BicGpio::getValue(): value = %u, value-ipmb: %u", val, val_ipmb);
        return val;
    }
    virtual bool setValue(int value)
    {
        DEBUG_PRINT(DEBUG_VERBOSE, "BicGpio::setValue(): bic set gpio, fru = %u, name = %s, invert: %d", m_fruid, m_name.c_str(), m_invert);
        uint8_t val = (m_invert) ? ((value + 1) % 2) : value;
        DEBUG_PRINT(DEBUG_VERBOSE, "BicGpio::setValue(): bic set gpio, value = %u, real value = %u", value, val);
        if (bic_set_gpio(m_fruid, m_gpio_num, val))
        {
            DEBUG_PRINT(DEBUG_VERBOSE, "BicGpio::setValue(): bic set gpio failed, fru = %u, name = %s", m_fruid, m_name.c_str());
            return false;
        }
        return true;
    }

    void setInvert(bool invert)
    {
        m_invert = invert;
    }

private:
    uint8_t m_fruid;
    uint8_t m_gpio_num;
    bool m_invert;
};

void plat_init(void)
{
    hdtSelName = "HD_BIC_JTAG_MUX_SEL";
    hdtDbgReqName = "HD_HDT_BIC_DBREQ_R_N";
    PwrOkName = "HD_PWRGD_CPU_LVC3";
    powerButtonName = "DUMMY_ASSERT_PWR_BTN";
    resetButtonName = "DUMMY_ASSERT_RST_BTN";
    warmResetButtonName = "DUMMY_ASERT_WARM_RST_BTN";
    fpgaReservedButtonName = "DUMMY_FPGA_RSVD";
    jtagTRSTName = "HD_HDT_BIC_TRST_R_N";

    uint8_t test_fruid = hostFruId;

    // BIC GPIO
    std::shared_ptr<BicGpio> hdtDbgReqPin = std::make_shared<BicGpio>(hdtDbgReqName, test_fruid, HD_HDT_BIC_DBREQ_R_N);
    std::shared_ptr<BicGpio> PwrOkPin = std::make_shared<BicGpio>(PwrOkName, test_fruid, HD_PWRGD_CPU_LVC3);
    std::shared_ptr<BicGpio> hdtSelPin = std::make_shared<BicGpio>(hdtSelName, test_fruid, HD_BIC_JTAG_MUX_SEL, true);
    std::shared_ptr<BicGpio> jtagTrstPin = std::make_shared<BicGpio>(jtagTRSTName, test_fruid, HD_HDT_BIC_TRST_R_N);

    GpioHelper::helperVec.push_back(hdtDbgReqPin);
    GpioHelper::helperVec.push_back(PwrOkPin);
    GpioHelper::helperVec.push_back(hdtSelPin);
    GpioHelper::helperVec.push_back(jtagTrstPin);

    hdtSelPin->setValue(1);
    jtagTrstPin->setValue(1);

    // DUMMY GPIO
    GpioHelper::helperVec.push_back(std::make_shared<DummyGpio>(powerButtonName, 1));
    GpioHelper::helperVec.push_back(std::make_shared<DummyGpio>(resetButtonName, 1));
    GpioHelper::helperVec.push_back(std::make_shared<DummyGpio>(warmResetButtonName, 1));
    GpioHelper::helperVec.push_back(std::make_shared<DummyGpio>(fpgaReservedButtonName, 1));
}
