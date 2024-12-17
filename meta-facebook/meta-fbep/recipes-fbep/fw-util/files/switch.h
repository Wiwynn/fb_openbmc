#ifndef _SWITCH_H_
#define _SWITCH_H_

#include "fw-util.h"
#include "spiflash.h"
#include <syslog.h>
#include <openbmc/pal.h>

class PAXComponent : public GPIOSwitchedSPIMTDComponent {
  uint8_t _paxid;
  public:
    PAXComponent(const std::string& fru, const std::string& comp, uint8_t paxid, const std::string& shadow)
      : GPIOSwitchedSPIMTDComponent(fru, comp, "switch0", "spi4.0", shadow, true, false), _paxid(paxid) {}
    int print_version() override;
    int update(const std::string& image) override;
};

#endif
