/*
 * reimter-util
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <cstdio>
#include <openbmc/pal.h>
#include <openbmc/kv.hpp>
#include <openbmc/misc-utils.h>
#include <openbmc/aries_common.h>
#include <CLI/CLI.hpp>
#include <iostream>
#include <string>
#include <openbmc/plat.h>
#include <facebook/bic.h>
#include <facebook/bic_ipmi.h>

extern "C" {
extern void plat_rt_preinit(void *);
}

using namespace std;

static constexpr auto RETIMER_ID = 0x0;
static constexpr auto HEARTBEAT = 0x1;
static constexpr auto CODE_LOAD = 0x2;

static int mb_retimer_lock(const std::string& fru)
{
  return single_instance_lock_blocked(std::string(fru + "_retimer").c_str());
}

static void mb_retimer_unlock(int lock)
{
  if (lock >= 0) { single_instance_unlock(lock); }
}

static bool lock_and_execute(const std::string& fru, std::function<void()> func)
{
  int lock = -1;
  if ((lock = mb_retimer_lock(fru)) < 0)
  {
    std::cerr << "Cannot get retimer lock for " << fru << std::endl;
    return false;
  }
  func();
  mb_retimer_unlock(lock);
  return true;
}

static int get_retimer_type(std::string fru)
{
  uint8_t slot_id = 0;
  if (pal_get_fru_id((char *)(fru.c_str()), &slot_id))
  {
    std::cout<< "failed to get slot_id 1. \n";
    return -1;
  }
  return pal_get_retimer_type(slot_id);
}

static void do_print_health(const std::string& fru) 
{
  lock_and_execute(fru, [&]() {
    uint8_t health = 0;
    auto retimer_type = get_retimer_type(fru);
    uint8_t slot_id = 0;
    if (pal_get_fru_id(const_cast<char*>(fru.c_str()), &slot_id))
    {
      std::cout<< "failed to get slot_id \n";
      return;
    }

    if (retimer_type == RETIMER_AL_PT4080L)
    {
      struct retimer_config config = {
        .slot_id = slot_id,
        .type = ARIES_PTX08,
        .retimer_bus = RETIMER_SWITCH_BUS,       //0'base
        .retimer_addr = AL_RETIMER_ADDR,     //8 bits
        .retimer_width = 4,           // javaisland only use 4
      };
      if (bic_set_sensor_monitor_state(slot_id, false, NONE_INTF)) {
        throw runtime_error("failed to disable BIC sensor polling");
      }
      plat_rt_preinit((void *)(&config));
      AriesGetHealth(&health);
      if (bic_set_sensor_monitor_state(slot_id, true, NONE_INTF)) {
        throw runtime_error("failed to enable BIC sensor polling");
      }
      std::cout << "heartbeat: " 
            << ((health & HEARTBEAT) ? "good" : "bad") << std::endl;
      std::cout << "code load status: "
              << ((health & CODE_LOAD) ? "good" : "bad") << std::endl;
    } else if (retimer_type == RETIMER_TI_DS160PT801) {
      std::cout<< "Only support AL \n";
    } else {
      std::cerr << "Get retimer health failed: Invalid retimer type.\n";
    }
  });
}

static void do_margin(const std::string& fru)
{
  lock_and_execute(fru, [&]() {
    auto retimer_type = get_retimer_type(fru);
    uint8_t slot_id = 0;
    if (pal_get_fru_id(const_cast<char*>(fru.c_str()), &slot_id))
    {
      std::cout<< "failed to get slot_id \n";
      return;
    }
    
    if (retimer_type == RETIMER_AL_PT4080L)
    {
      struct retimer_config config = {
        .slot_id = slot_id,
        .type = ARIES_PTX08,
        .retimer_bus = RETIMER_SWITCH_BUS,       //0'base
        .retimer_addr = AL_RETIMER_ADDR,     //8 bits
        .retimer_width = 4,           // javaisland only use 4
      };
      if (bic_set_sensor_monitor_state(slot_id, false, NONE_INTF)) {
        throw runtime_error("failed to disable BIC sensor polling");
      }
      plat_rt_preinit((void *)(&config));
      AriesMargin();
      if (bic_set_sensor_monitor_state(slot_id, true, NONE_INTF)) {
        throw runtime_error("failed to enable BIC sensor polling");
      }
    }
    else if (retimer_type == RETIMER_TI_DS160PT801)
    {
      std::cout<< "Only support AL \n";
    }
    else
    {
      std::cerr << "Margin test failed: Invalid retimer type.\n";
    }
  });
}

static void do_print_link(const std::string& fru)
{
  lock_and_execute(fru, [&]() {
    auto retimer_type = get_retimer_type(fru);
    uint8_t slot_id = 0;
    if (pal_get_fru_id(const_cast<char*>(fru.c_str()), &slot_id))
    {
      std::cout<< "failed to get slot_id \n";
      return;
    }

    if (retimer_type == RETIMER_AL_PT4080L)
    {
      struct retimer_config config = {
        .slot_id = slot_id,
        .type = ARIES_PTX08,
        .retimer_bus = RETIMER_SWITCH_BUS,       //0'base
        .retimer_addr = AL_RETIMER_ADDR,     //8 bits
        .retimer_width = 4,           // javaisland only use 4
      };
      if (bic_set_sensor_monitor_state(slot_id, false, NONE_INTF)) {
        throw runtime_error("failed to disable BIC sensor polling");
      }
      plat_rt_preinit((void *)(&config));
      AriesPrintState();
      if (bic_set_sensor_monitor_state(slot_id, true, NONE_INTF)) {
        throw runtime_error("failed to enable BIC sensor polling");
      }
    }
    else if (retimer_type == RETIMER_TI_DS160PT801)
    {
      std::cout<< "Only support AL \n";
    }
    else
    {
      std::cerr << "Dump link status failed: Invalid retimer type.\n";
    }
  });
}

int main(int argc, char* argv[]) {
    std::string fru;

    CLI::App app("Retimer Helper Utility");
    app.failure_message(CLI::FailureMessage::help);

    app.add_option("FRU", fru, "FRU name: slot1, slot2, slot3 or slot4")
        ->check(CLI::IsMember({"slot1", "slot2", "slot3", "slot4"}))->required();
    app.require_subcommand(1);

    /* Get health */
    auto health = app.add_subcommand("health", "Get retimer health");
    health->callback([&]() { do_print_health(fru); });
    
    /* Perform margin Test */
    auto margin = app.add_subcommand("margin", "Perform margin test");
    margin->callback([&]() { do_margin(fru); });

    /* Dump link status */
    auto link = app.add_subcommand("link", "Dump link status");
    link->callback([&]() { do_print_link(fru); });

    CLI11_PARSE(app, argc, argv);


 
    return 0;
}