#include "plat/plat_def.h"

#include <map>
#include <iostream>
#include <iomanip>
#include <openbmc/ipmi.h>
#include "infrastructure/debug.h"

constexpr auto BIC_CMD_OEM_GET_SET_GPIO = 0x41;
// SD BIC GPIO number
constexpr auto PWRGD_CPU_LVC3 = 3;
constexpr auto HDT_BIC_DBREQ_R_N = 22;
constexpr auto HDT_BIC_TRST_R_N = 39;
constexpr auto BIC_JTAG_MUX_SEL = 50;

std::string uint8ToHexString(uint8_t value) {
    std::stringstream stream;
    stream << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(value);
    return stream.str();
}

std::string uint8ArrayToHexString(const uint8_t* array, int length) {
    std::ostringstream oss;
    for (int i = 0; i < length; ++i) {
        oss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(array[i]);
        if (i != length - 1) {
            oss << " ";
        }
    }
    return oss.str();
}

int pldm_bic_get_one_gpio_status(uint8_t slot_id, uint8_t gpio_num, uint8_t *value)
{
    const uint8_t iana[3] = {0x15, 0xa0, 0x00};
    FILE *fp;
    char buffer[1024];
    auto eid = slot_id * 10;
    auto netfn = (uint8_t)(NETFN_OEM_1S_REQ << 2);
    auto txbuf = uint8ArrayToHexString(iana, 3) + " 00 " + uint8ToHexString(gpio_num);

    auto command = "pldmtool raw -m " + std::to_string(eid) + " -d 0x80 0x3f 0x01 0x15 0xA0 0x00 "
            + uint8ToHexString(netfn) + " " + uint8ToHexString(BIC_CMD_OEM_GET_SET_GPIO)
            + " " + txbuf;
    fp = popen(command.c_str(), "r");
    if (fp == NULL) {
        perror("popen() failed");
        return -1;
    }
    std::string output;
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        output += buffer;
    }
    pclose(fp);

    std::vector<uint8_t> rx_data;
    size_t rx_pos = output.find("Rx:");
    if (rx_pos != std::string::npos) {
        std::istringstream rx_stream(output.substr(rx_pos + 4));
        int hex_byte;
        while (rx_stream >> std::hex >> hex_byte) {
            rx_data.push_back(hex_byte);
        }
        // byte 9 is CC code
        if (rx_data[9] != CC_SUCCESS) {
            return -1;
        }
        // last byte is GPIO value
        *value = rx_data.back();
        return 0;
    }
}

int pldm_bic_set_gpio(uint8_t slot_id, uint8_t gpio_num, uint8_t value)
{
    const uint8_t iana[3] = {0x15, 0xa0, 0x00};
    FILE *fp;
    char buffer[1024];
    auto eid = slot_id * 10;
    auto netfn = (uint8_t)(NETFN_OEM_1S_REQ << 2);
    auto txbuf = uint8ArrayToHexString(iana, 3) + " 01 " + uint8ToHexString(gpio_num) + " " + uint8ToHexString(value);

    auto command = "pldmtool raw -m " + std::to_string(eid) + " -d 0x80 0x3f 0x01 0x15 0xA0 0x00 "
            + uint8ToHexString(netfn) + " " + uint8ToHexString(BIC_CMD_OEM_GET_SET_GPIO)
            + " " + txbuf;
    fp = popen(command.c_str(), "r");
    if (fp == NULL) {
        perror("popen() failed");
        return -1;
    }
    std::string output;
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        output += buffer;
    }
    pclose(fp);

    std::vector<uint8_t> rx_data;
    size_t rx_pos = output.find("Rx:");
    if (rx_pos != std::string::npos) {
        std::istringstream rx_stream(output.substr(rx_pos + 4));
        int hex_byte;
        while (rx_stream >> std::hex >> hex_byte) {
            rx_data.push_back(hex_byte);
        }
        // byte 9 is completion code
        if (rx_data[9] != CC_SUCCESS) {
            return -1;
        }

        return 0;
    }
}

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
        if (pldm_bic_get_one_gpio_status(m_fruid, m_gpio_num, &val_ipmb))
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
        if (pldm_bic_set_gpio(m_fruid, m_gpio_num, val))
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
    hdtSelName = "BIC_JTAG_MUX_SEL";
    hdtDbgReqName = "HDT_BIC_DBREQ_R_N";
    PwrOkName = "PWRGD_CPU_LVC3";
    powerButtonName = "DUMMY_ASSERT_PWR_BTN";
    resetButtonName = "DUMMY_ASSERT_RST_BTN";
    warmResetButtonName = "DUMMY_ASERT_WARM_RST_BTN";
    fpgaReservedButtonName = "DUMMY_FPGA_RSVD";
    jtagTRSTName = "HDT_BIC_TRST_R_N";

    uint8_t test_fruid = hostFruId;

    // BIC GPIO
    std::shared_ptr<BicGpio> hdtDbgReqPin = std::make_shared<BicGpio>(hdtDbgReqName, test_fruid, HDT_BIC_DBREQ_R_N);
    std::shared_ptr<BicGpio> PwrOkPin = std::make_shared<BicGpio>(PwrOkName, test_fruid, PWRGD_CPU_LVC3);
    std::shared_ptr<BicGpio> hdtSelPin = std::make_shared<BicGpio>(hdtSelName, test_fruid, BIC_JTAG_MUX_SEL, true);
    std::shared_ptr<BicGpio> jtagTrstPin = std::make_shared<BicGpio>(jtagTRSTName, test_fruid, HDT_BIC_TRST_R_N);

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
