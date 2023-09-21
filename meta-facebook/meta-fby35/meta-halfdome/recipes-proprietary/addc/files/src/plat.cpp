#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include <syslog.h>

extern uint8_t fru;

constexpr int DELAY_POWER_OFF = 6000;
constexpr int DELAY_POWER_CYCLE = 5000;
constexpr int DELAY_POWER_ON = 1000;

static inline void triggerPowerButton(const int durationMs) {
    const std::string pwr_ctl = "/usr/bin/bic-util slot" +
        std::to_string(fru) + " 0x18 0x52 0x00 0x42 0x00 0x00 ";

    std::system((pwr_ctl + "0xff").c_str());
    std::system((pwr_ctl + "0xfe").c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
    std::system((pwr_ctl + "0xff").c_str());
}

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
    syslog(LOG_CRIT, "Dump completed, cold reset FRU: %u", fru);

    //ADDC Whitepaper : S0 Cold Reset trigger by PWRBUTTON_L
    triggerPowerButton(DELAY_POWER_OFF);
    std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_POWER_CYCLE));
    triggerPowerButton(DELAY_POWER_ON);
}
