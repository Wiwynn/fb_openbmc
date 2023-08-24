#ifndef __BIC_APML_INTERFACE_HPP__
#define __BIC_APML_INTERFACE_HPP__

#include <cstdint>

extern "C" {
#include "apml.h"
#include "esmi_cpuid_msr.h"
#include "esmi_mailbox.h"
#include "esmi_mailbox_nda.h"
#include "esmi_rmi.h"
}

uint8_t get_current_target_fruid();
void set_current_target_fruid(uint8_t fruid);

#endif /* __BIC_APML_INTERFACE_HPP__ */