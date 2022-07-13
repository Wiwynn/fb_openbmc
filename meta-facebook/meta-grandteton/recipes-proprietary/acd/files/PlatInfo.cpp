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

using namespace std;

#define MAX_BUF_SIZE 128

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

void getBmcVersion(char *ver) {
  string ver_str;

  if (get_fw_ver("bmc", "bmc", ver_str) == 0) {
    cd_snprintf_s(ver, SI_JSON_STRING_LEN, "%s", ver_str.c_str());
  }
}

void getBiosVersion(char *ver) {
  string ver_str;

  if (get_fw_ver("mb", "bios", ver_str) == 0) {
    cd_snprintf_s(ver, SI_JSON_STRING_LEN, "%s", ver_str.c_str());
  }
}

void getSystemGuid(string &guid) {
  char line[MAX_BUF_SIZE];
  FILE *fp;

  if (!(fp = popen("/usr/bin/ipmitool mc guid 2>/dev/null | grep GUID", "r")))
    return;

  if (fgets(line, sizeof(line), fp)) {
    pclose(fp);

    string str = line;
    auto start = str.find(":");
    if (start == string::npos)
      return;

    start = str.find_first_not_of(" ", start+1);
    if (start == string::npos)
      return;

    auto end = str.find("\n", start);
    if (end == string::npos) {
      end = str.size();
    }

    guid = str.substr(start, end - start);
  }
}

void platformInit(uint8_t fru) {
  (void)fru;
}
