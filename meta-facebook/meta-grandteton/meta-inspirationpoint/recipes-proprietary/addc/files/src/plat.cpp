#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

extern "C" {
#include <syslog.h>
#include <openbmc/misc-utils.h>
}

static int lock_fd = -1;

void set_apml_mux_locked(uint8_t cpu) {
    std::ofstream gpioFile("/tmp/gpionames/FM_APML_MUX2_SEL_R/value");
    if (!gpioFile) {
        syslog(LOG_ERR, "Failed to open GPIO FM_APML_MUX2_SEL_R");
        return;
    }

    gpioFile << (cpu ? '1' : '0');
    gpioFile.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void apml_mux_lock(uint8_t cpu) {
    int fd = single_instance_lock_blocked("apml_mux");

    if (fd >= 0) {
        lock_fd = fd;
    }

    set_apml_mux_locked(cpu);
}

void apml_mux_unlock(uint8_t cpu) {
    (void)cpu;

    if (lock_fd >= 0) {
        int fd = lock_fd;
        lock_fd = -1;
        single_instance_unlock(fd);
    }
}

void triggerColdReset(void) {
    static bool done = false;

    if (done) {
        return;
    }
    done = true;

    std::ofstream gpioFile("/tmp/gpionames/RST_BMC_RSTBTN_OUT_R_N/value");
    if (!gpioFile) {
        syslog(LOG_ERR, "Failed to open GPIO RST_BMC_RSTBTN_OUT_R_N");
        return;
    }

    syslog(LOG_CRIT, "Dump completed, cold reset FRU: 1");
    gpioFile << '0' << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    gpioFile << '1' << std::flush;
    gpioFile.close();
}
