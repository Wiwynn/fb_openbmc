#ifndef _NIC_MCTP_H_
#define _NIC_MCTP_H_

#include "nic.h"

class MCTPNicComponent : public NicComponent {
  protected:
    uint8_t _eid;
  public:
    MCTPNicComponent(const std::string& fru, const std::string& comp, const std::string& key, uint8_t eid)
      : NicComponent(fru, comp, key), _eid(eid) {}
};

class MCTPOverSMBusNicComponent : public MCTPNicComponent {
  protected:
    uint8_t _bus_id;
  public:
    MCTPOverSMBusNicComponent(const std::string& fru, const std::string& comp, const std::string& key, uint8_t eid, uint8_t bus)
      : MCTPNicComponent(fru, comp, key, eid), _bus_id(bus) {}
    int get_version(json& j);
    int update(const std::string& image) override;
};
#endif
