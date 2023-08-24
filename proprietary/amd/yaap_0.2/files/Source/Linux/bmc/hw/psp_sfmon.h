#include <openbmc/libgpio.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include "jtag_hw.h"
#include "infrastructure/debug.h"

/** @class pspSfMonitor
 *  @brief Responsible for catching GPIO state change
 *  condition.
 */

class pspSfMonitor
{
  public:
    pspSfMonitor() = delete;
    ~pspSfMonitor() = default;
    pspSfMonitor(const pspSfMonitor&) = delete;
    pspSfMonitor& operator=(const pspSfMonitor&) = delete;
    pspSfMonitor(pspSfMonitor&&) = delete;
    pspSfMonitor& operator=(pspSfMonitor&&) = delete;

    /** @brief Constructs pspSfMonitor object.
     *
     *  @param[in] line        - GPIO line from libgpiod
     *  @param[in] io          - io service
     *  @param[in] jtagHw      - coolreset object
     */
    // pspSfMonitor( gpiod::line& line,
    pspSfMonitor(
                  bmc::JtagHw *cpuDebugJtagHw
                ):gpioEventDescriptor(io_service)
    {
        // gpioLine = line;
        coolresetJtagHw = cpuDebugJtagHw;
        DEBUG_PRINT(DEBUG_INFO, "pspSfMonitor Constructor()\n");
    };

  private:
    /** @brief boost IO service */
    boost::asio::io_service io_service;

    /** @brief GPIO line */
    // gpiod::line gpioLine;

    /** @brief GPIO line */
    bmc::JtagHw *coolresetJtagHw;

    /** @brief GPIO event descriptor */
    boost::asio::posix::stream_descriptor gpioEventDescriptor;

    /** @brief Schedule an event handler for GPIO event to trigger */
    void scheduleEventHandler();

    /** @brief Handle the GPIO event and starts configured target */
    void gpioEventHandler();

  public:
    /** @brief register handler for gpio event
     *
     *  @return  - 0 on success and -1 otherwise
     */
    void requestGPIOEvents();
};
