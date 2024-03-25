#pragma once

#include <sdbusplus/bus.hpp>

constexpr size_t IANA_ID_SIZE = 3;
constexpr uint32_t IANA_ID = 0x00A015;
constexpr size_t SUCCESS = 0;
constexpr int MAX_RETRY_TIME = 3;

constexpr size_t NETFN_OEM_1S_REQ = 0x38;
constexpr size_t CMD_OEM_1S_UPDATE_FW = 0x9;
constexpr size_t CMD_OEM_1S_MSG_OUT = 0x02;

class BIOSupdater
{
  public:
    explicit BIOSupdater(sdbusplus::bus_t& bus, const std::string& imagePath,
                         const uint8_t slotId, const std::string& cpuType) :
        bus(bus),
        imagePath(imagePath), slotId(slotId), cpuType(cpuType)
    {}

    /** @brief Update bios according to the USB file path.
     *
     *  @return Success or Fail
     */
    bool run();

  private:
    /** @brief Persistent sdbusplus DBus bus connection. */
    sdbusplus::bus_t& bus;

    /** The image path. */
    const std::string& imagePath;

    /** The slot Id for update*/
    const uint8_t slotId;

    /** BERGAMO or TURNIN cpu will use different offset to update */
    const std::string& cpuType;
};
