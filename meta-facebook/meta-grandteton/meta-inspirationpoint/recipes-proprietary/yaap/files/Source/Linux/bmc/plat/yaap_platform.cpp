#include "plat/plat_def.h"
#include <openbmc/libgpio.h>
#include <map>
#include <iostream>
#include "infrastructure/debug.h"

class BmcGpio : public GpioHelper
{
public:
    BmcGpio(const std::string& name, bool  invert = false):
        GpioHelper(name), m_invert(invert)
    {}
    virtual int getValue()
    {
        gpio_value_t value;
        uint8_t rev, val;

        DEBUG_PRINT(DEBUG_VERBOSE, "BmcGpio::setValue(): bmc set gpio, name = %s, invert: %d", m_name.c_str(), m_invert);
        value = gpio_get_value_by_shadow(m_name.c_str());

        val = (uint8_t ) value;
        rev = (m_invert) ? ((val + 1) % 2) : val;
        DEBUG_PRINT(DEBUG_VERBOSE, "BmcGpio::getValue(): value = %u, real value: %u", rev, val);

        return rev;
    }

    virtual bool setValue(int value)
    {
        DEBUG_PRINT(DEBUG_VERBOSE, "BmcGpio::setValue(): bmc set gpio, name = %s, invert: %d", m_name.c_str(), m_invert);
        uint8_t rev = (m_invert) ? ((value + 1) % 2) : value;
        DEBUG_PRINT(DEBUG_VERBOSE, "BmcGpio::setValue(): bmc set gpio, value = %u, real value = %u", rev, value);

        gpio_value_t gpio_val = (gpio_value_t)rev;
        if(gpio_set_value_by_shadow(m_name.c_str(), gpio_val)) {
          return false;
        }

        usleep(10000);
        return true;
    }

    void setInvert(bool invert)
    {
        m_invert = invert;
    }

private:
    bool m_invert;
};

void plat_init(void)
{
    hdtSelName = "FM_JTAG_BMC_MUX_SEL_R1";
    hdtDbgReqName = "FM_DBP_CPU_PREQ_GF_N";
    PwrOkName = "FM_PWRGD_CPU1_PWROK";
    powerButtonName = "SYS_BMC_PWRBTN_OUT";
    resetButtonName = "RST_BMC_RSTBTN_OUT_R_N";
    warmResetButtonName = "RST_BMC_RSTBTN_OUT_R_N";
  //fpgaReservedButtonName = "FPGA_RSVD";
    jtagTRSTName = "JTAG_SCM_TRST_R_N";


    uint8_t fruid = hostFruId;

    std::shared_ptr<BmcGpio> hdtSelPin = std::make_shared<BmcGpio>(hdtSelName);
    hdtSelPin->setValue(1);
    std::shared_ptr<BmcGpio> jtagTrstPin = std::make_shared<BmcGpio>(jtagTRSTName);
    jtagTrstPin->setValue(1);

    // BMC GPIO
    GpioHelper::helperVec.push_back(std::make_shared<BmcGpio>(hdtDbgReqName));
    GpioHelper::helperVec.push_back(std::make_shared<BmcGpio>(PwrOkName));
    GpioHelper::helperVec.push_back(hdtSelPin);
    GpioHelper::helperVec.push_back(jtagTrstPin);
    GpioHelper::helperVec.push_back(std::make_shared<BmcGpio>(powerButtonName));
    GpioHelper::helperVec.push_back(std::make_shared<BmcGpio>(resetButtonName));
    GpioHelper::helperVec.push_back(std::make_shared<BmcGpio>(warmResetButtonName));
}
