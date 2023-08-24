#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include <syslog.h>

extern uint8_t fru;

void set_apml_mux_locked(uint8_t cpu) {
    (void)cpu;
}

void apml_mux_lock(uint8_t cpu) {
    (void)cpu;
}

void apml_mux_unlock(uint8_t cpu) {
    (void)cpu;
}

void triggerColdReset(void) {
    const std::string pwr_ctl = "/usr/bin/bic-util slot" +
        std::to_string(fru) + " 0x18 0x52 0x00 0x42 0x00 0x00 ";

    syslog(LOG_CRIT, "Dump completed, cold reset FRU: %u", fru);
    // toggle reset button
    std::system((pwr_ctl + "0xff").c_str());
    std::system((pwr_ctl + "0xfd").c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::system((pwr_ctl + "0xff").c_str());
}
