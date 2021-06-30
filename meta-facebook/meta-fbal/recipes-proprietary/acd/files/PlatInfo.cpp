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

#include "CrashdumpSections/MetaData.hpp"

using namespace std;

static int get_fw_ver(const string &fru, const string &comp, string &ver) {
  char line[128];
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

  if (get_fw_ver("mb", "bios", ver_str)) {
    return -1;
  }

  cd_snprintf_s(ver, SI_JSON_STRING_LEN, "%s", ver_str.c_str());
  return 0;
}

int getMeVersion(char *ver) {
  string ver_str;

  if (get_fw_ver("mb", "me", ver_str)) {
    return -1;
  }

  cd_snprintf_s(ver, SI_JSON_STRING_LEN, "%s", ver_str.c_str());
  return 0;
}

int getSystemGuid(string &guid) {
  char line[128];
  FILE *fp;

  if (!(fp = popen("/usr/bin/ipmitool mc guid 2>/dev/null | grep GUID", "r")))
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

    guid = str.substr(start, end - start);
  } while (0);
  pclose(fp);

  return 0;
}

int getPpinData(int cpu, char *ppin) {
  int ret = -1;
  char buf[128];
  FILE *fp;

  cd_snprintf_s(buf, sizeof(buf), "/usr/local/bin/cfg-util cpu%d_ppin 2>/dev/null", cpu);
  if (!(fp = popen(buf, "r")))
    return ret;

  do {
    if (!fgets(buf, sizeof(buf), fp))
      break;

    string str = buf;
    auto end = str.find("\n");
    if (end == string::npos) {
      end = str.size();
    }

    cd_snprintf_s(ppin, SI_JSON_STRING_LEN, "0x%s", str.substr(0, end).c_str());
    ret = 0;
  } while (0);
  pclose(fp);

  return ret;
}

void platformInit(uint8_t fru) {
  (void)fru;
}
