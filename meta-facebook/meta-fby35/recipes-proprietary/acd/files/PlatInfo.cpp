/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2019 Intel Corporation.
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("License"). Unless the License provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit
 * this software or the related documents without Intel's prior written
 * permission.
 *
 * This software and the related documents are provided as is, with no express
 * or implied warranties, other than those that are expressly stated in the
 * License.
 *
 ******************************************************************************/

extern "C" {
#include <stdio.h>
}
#include <string>
#include <sstream>
#include <unistd.h>
#include "CrashdumpSections/MetaData.hpp"
#include <syslog.h>

using namespace std;

#define KEY_BLK_SLED_CYCLE "/tmp/cache_store/to_blk_sled_cycle"
#define MAX_BUF_SIZE 128
#define PID_SIZE 16

enum {
  ENABLE_MONITOR = 0,
  DISABLE_MONITOR,
};
enum {
  FLAG_UNLOCK = 0,
  FLAG_LOCK,
};

extern int node_bus_id;

void split(const std::string& s, std::vector<std::string>& sv, const char delim = ' ') {
    sv.clear();
    std::istringstream iss(s);
    std::string temp;

    while (std::getline(iss, temp, delim)) {
        sv.emplace_back(std::move(&temp[2]));
    }

    return;
}

static int get_fw_ver(string fru, string comp, string &ver) {
  char line[MAX_BUF_SIZE];
  FILE *fp;
  string cmd;

  cmd = "/usr/bin/fw-util " + fru + " --version " + comp;
  if (!(fp = popen(cmd.c_str(), "r")))
    return -1;

  do {
    if (!fgets(line, sizeof(line), fp))
      break;

    string str = line;
    auto start = str.find(":");
    if (start == string::npos)
      break;

    start = str.find_first_not_of(" ", start+1);
    if (start == string::npos)
      break;

    auto end = str.find("\n", start);
    if (end == string::npos) {
      end = str.size();
    }

    ver = str.substr(start, end - start);
  } while (0);
  pclose(fp);

  return 0;
}

int getBmcVersion(char *ver) {
  string ver_str;

  if (get_fw_ver("bmc", "bmc", ver_str)) {
    return -1;
  }

  cd_snprintf_s(ver, SI_JSON_STRING_LEN, "%s", ver_str.c_str());
  return 0;
}

int getBiosVersion(char *ver) {
  string ver_str;
  string slot_str = "slot" + to_string(node_bus_id+1);

  if (get_fw_ver(slot_str, "bios", ver_str)) {
    return -1;
  }

  cd_snprintf_s(ver, SI_JSON_STRING_LEN, "%s", ver_str.c_str());
  return 0;
}

int getMeVersion(char *ver) {
  string ver_str;
  string slot_str = "slot" + to_string(node_bus_id+1);

  if (get_fw_ver(slot_str, "me", ver_str)) {
    return -1;
  }

  cd_snprintf_s(ver, SI_JSON_STRING_LEN, "%s", ver_str.c_str());
  return 0;
}

int getSystemGuid(string &guid) {
  char line[MAX_BUF_SIZE], cmd[MAX_BUF_SIZE];
  FILE *fp;
  string slot_str = "slot" + to_string(node_bus_id+1);
  string guid_str("");

  snprintf(cmd, sizeof(cmd), "/usr/local/bin/guid-util %s sys --get 2>/dev/null", slot_str.c_str());

  if (!(fp = popen(cmd, "r")))
    return -1;

  do {
    if (!fgets(line, sizeof(line), fp))
      break;

    string str = line;
    auto start = str[0];
    if (start == string::npos)
      break;

    size_t pos = 0;
    while (( pos = str.find ("\n",pos)) != string::npos ) {
      str.erase (pos, 1);
    }

    std::vector<std::string> sv;
    split(str, sv, ':');

    // Hanle LSB
    std::swap(sv[0],sv[3]);
    std::swap(sv[1],sv[2]);
    std::swap(sv[4],sv[5]);
    std::swap(sv[6],sv[7]);
    for (int i = 0; i < (int)sv.size(); i++ ) {
      if ( i == 4 || i == 6 || i == 8 || i == 10 ) {
        guid_str += "-";
      }
      guid_str += sv[i];
    }

    guid = guid_str;
  } while (0);
  pclose(fp);

  return 0;
}

int getPpinData(int, char*) {
  return -1;
}

void platformInit(uint8_t fru) {
  node_bus_id = fru - 1;
}

