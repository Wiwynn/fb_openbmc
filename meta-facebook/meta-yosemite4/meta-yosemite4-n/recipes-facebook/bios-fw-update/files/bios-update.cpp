#include "bios-update.hpp"

#include "bios-usb-update.hpp"

#include <ctype.h>
#include <unistd.h>

#include <CLI/CLI.hpp>
#include <sdbusplus/bus.hpp>
#include <set>

#include <variant>
#include <vector>

constexpr auto HOST_STATE_SERVICE = "xyz.openbmc_project.State.Host";
constexpr auto HOST_STATE_OBJECT = "/xyz/openbmc_project/state/host";
constexpr auto HOST_STATE_INTERFACE = "xyz.openbmc_project.State.Host";

using PropertyValue = std::variant<std::string>;

enum class POWER : uint8_t
{
    OFF = 0x00,
    ON = 0x01,
};

uint8_t getSlotAddress(uint8_t slotId)
{
    // base 0
    return ((slotId - 1) << 2);
}

void cmd_begin_with_IANA(std::vector<uint8_t>& cmd)
{
    uint8_t iana[IANA_ID_SIZE];
    memcpy(iana, (uint8_t*)&IANA_ID, IANA_ID_SIZE);
    cmd.insert(cmd.begin(), iana, iana + IANA_ID_SIZE);
}

bool set_host_state(sdbusplus::bus_t& bus, POWER ctrl, uint8_t slotId)
{
    std::string service = std::string{HOST_STATE_SERVICE} +
                          std::to_string(slotId);
    std::string object = std::string{HOST_STATE_OBJECT} +
                         std::to_string(slotId);

    auto hostStateCall = bus.new_method_call(service.c_str(), object.c_str(),
                                             "org.freedesktop.DBus.Properties",
                                             "Set");

    PropertyValue value = (ctrl == POWER::ON)
                              ? "xyz.openbmc_project.State.Host.Transition.On"
                              : "xyz.openbmc_project.State.Host.Transition.Off";

    hostStateCall.append(HOST_STATE_INTERFACE, "RequestedHostTransition",
                         value);
    try
    {
        bus.call_noreply(hostStateCall);
    }
    catch (sdbusplus::exception_t& e)
    {
        std::cerr << "Failed to set property: " << e.what() << "\n";
        return false;
    }

    (ctrl == POWER::ON) ? sleep(1) : sleep(6); // wait until setting done.
    return true;
}

bool power_ctrl(sdbusplus::bus_t& bus, POWER ctrl, uint8_t slotId)
{
    set_host_state(bus, ctrl, slotId);

    // TODO: Check power status using PLDM tool
    return true;
}

bool BIOSupdater::run()
{
    int ret = 0;

    if (!power_ctrl(bus, POWER::OFF, slotId))
    {
        return false;
    }

    ret = update_bic_usb_bios(slotId, imagePath, cpuType);

    if (!power_ctrl(bus, POWER::ON, slotId))
    {
        return false;
    }

    return (ret < 0) ? false : true;
}

int main(int argc, char** argv)
{
    auto bus = sdbusplus::bus::new_default();
    std::string imagePath{};
    uint8_t slotId;
    std::string cpuType;

    CLI::App app{"Update the firmware BIOS via USB to BIC"};

    app.add_option("-f,--file", imagePath, "The path of bios image file.")
        ->required()
        ->check(CLI::ExistingFile);

    app.add_option("-s,--slot", slotId, "The number of slot to update.")
        ->required();

    app.add_option("-c, --cpu", cpuType, "BERGAMO or TURIN")
        ->required();

    CLI11_PARSE(app, argc, argv);

    if (cpuType != "BERGAMO" && cpuType != "TURIN") {
        std::cerr << "Wrong option: only support BERGAMO or TURIN cpu\n";
        return 0;
    }

    auto bios = BIOSupdater(bus, imagePath, slotId, cpuType);
    if (bios.run())
    {
        std::cerr << "BIOS update: success\n";
    }
    else
    {
        std::cerr << "BIOS update: fail\n";
    }

    return 0;
}
