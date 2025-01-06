#ifndef _EXPANSION_H_
#define _EXPANSION_H_
#include "server.h"

#include <cstdint>
#include <string>

class ExpansionBoard : public Server {
  private:
    uint8_t slot_id = 0;
    std::string fru;
    std::string board_name;
    uint8_t fw_comp = 0;
  public:
    ExpansionBoard(uint8_t _slot_id, const std::string& _fru, const std::string& _board_name, uint8_t _fw_comp)
      : Server(_slot_id, _fru), slot_id(_slot_id), fru(_fru), board_name(_board_name), fw_comp(_fw_comp) {}
    // Throws exception if not
    void ready();
};

#endif
