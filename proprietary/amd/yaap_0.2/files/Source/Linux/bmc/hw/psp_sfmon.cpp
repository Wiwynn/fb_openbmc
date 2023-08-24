#include "psp_sfmon.h"

void pspSfMonitor::scheduleEventHandler()
{

    // DEBUG_PRINT(DEBUG_INFO, "pspSfMonitor scheduleEventHandler()\n");
    // gpioEventDescriptor.async_wait(
    //     boost::asio::posix::stream_descriptor::wait_read,
    //     [this](const boost::system::error_code& ec) {
    //         if (ec)
    //         {
    //             std::string msg = "event handler error" +
    //                               std::string(ec.message());
    //             DEBUG_PRINT(DEBUG_FATAL, "%s\n", msg.c_str());
    //             return;
    //         }
    //         gpioEventHandler();
    //     });
}

void pspSfMonitor::gpioEventHandler()
{
    // gpiod::line_event gpioLineEvent = gpioLine.event_read();

    // DEBUG_PRINT(DEBUG_INFO, "PSP Soft Fuse Notify Event Received\n");

    // if (gpioLineEvent.event_type == gpiod::line_event::RISING_EDGE)
    // {
    //     coolresetJtagHw->coolreset();
    // }

    // /* Schedule a wait event */
    // scheduleEventHandler();
}

void pspSfMonitor::requestGPIOEvents()
{

    // DEBUG_PRINT(DEBUG_INFO, "pspSfMonitor requestGPIOEvents()\n");
    // /* Request an event to monitor for respected gpio line */
    // try {
    //     gpioLine.request( { "yaapd", gpiod::line_request::EVENT_BOTH_EDGES });
    // }
    // catch (std::exception &exc) {
    //     DEBUG_PRINT(DEBUG_FATAL, "Failed to request gpioLineEvent\n");
    //     //return -1;
    // }

    // int gpioLineFd = gpioLine.event_get_fd();
    // if (gpioLineFd < 0)
    // {
    //     DEBUG_PRINT(DEBUG_FATAL, "Failed to get fd for gpioLineEvent\n");
    //     //return -1;
    // }


    // /* Assign line fd to descriptor for monitoring */
    // gpioEventDescriptor.assign(gpioLineFd);

    // /* Schedule a wait event */
    // scheduleEventHandler();

    // io_service.run();
}
