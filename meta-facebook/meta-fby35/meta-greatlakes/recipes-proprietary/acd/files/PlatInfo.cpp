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
#include "engine/utils.h"
}
#include <string>
#include <sstream>
#include <vector>

using namespace std;

#define MAX_BUF_SIZE 128

extern int node_bus_id;
extern string crashdumpFruName;

static void split(const string &s, vector<string> &sv, const char delim = ' ') {
  istringstream iss(s);
  string temp;

  sv.clear();
  while (getline(iss, temp, delim)) {
    sv.emplace_back(move(&temp[2]));
  }
}

static int get_fw_ver(const string &fru, const string &comp, string &ver) {
  char line[MAX_BUF_SIZE];
  FILE *fp;
  string cmd;

  cmd = "/usr/bin/fw-util " + fru + " --version " + comp;
  if (!(fp = popen(cmd.c_str(), "r")))
    return -1;

  if (fgets(line, sizeof(line), fp)) {
    pclose(fp);

    string str = line;
    auto start = str.find(":");
    if (start == string::npos)
      return -1;

    start = str.find_first_not_of(" ", start+1);
    if (start == string::npos)
      return -1;

    auto end = str.find("\n", start);
    if (end == string::npos) {
      end = str.size();
    }

    ver = str.substr(start, end - start);
  }

  return 0;
}

void getBmcVersion(char *ver, size_t) {
  string ver_str;

  if (get_fw_ver("bmc", "bmc", ver_str) == 0) {
    cd_snprintf_s(ver, SI_BMC_VER_LEN, "%s", ver_str.c_str());
  }
}

void getBiosVersion(char *ver, size_t) {
  string ver_str;
  string slot_str = "slot" + to_string(node_bus_id+1);

  if (get_fw_ver(slot_str, "bios", ver_str) == 0) {
    cd_snprintf_s(ver, SI_BIOS_ID_LEN, "%s", ver_str.c_str());
  }
}

void getSystemGuid(string &guid) {
  char line[MAX_BUF_SIZE], cmd[MAX_BUF_SIZE];
  FILE *fp;
  string slot_str = "slot" + to_string(node_bus_id+1);
  string guid_str("");

  snprintf(cmd, sizeof(cmd), "/usr/local/bin/guid-util %s sys --get 2>/dev/null", slot_str.c_str());
  if (!(fp = popen(cmd, "r")))
    return;

  if (fgets(line, sizeof(line), fp)) {
    pclose(fp);

    string str = line;
    auto start = str[0];
    if (start == string::npos)
      return;

    size_t pos = 0;
    while ((pos = str.find("\n", pos)) != string::npos) {
      str.erase (pos, 1);
    }

    vector<string> sv;
    split(str, sv, ':');

    // Hanle LSB
    swap(sv[0], sv[3]);
    swap(sv[1], sv[2]);
    swap(sv[4], sv[5]);
    swap(sv[6], sv[7]);
    for (int i = 0; i < (int)sv.size(); i++) {
      if (i == 4 || i == 6 || i == 8 || i == 10) {
        guid_str += "-";
      }
      guid_str += sv[i];
    }

    guid = guid_str;
  }
}

void platformInit(uint8_t fru) {
  node_bus_id = fru - 1;
  crashdumpFruName = "slot" + to_string(fru) + "_";
}
