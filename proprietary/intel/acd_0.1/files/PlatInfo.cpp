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

using namespace std;

int getBmcVersion(char *ver) {
  ver = ver;
  return 0;
}

int getBiosVersion(char *ver) {
  ver = ver;
  return 0;
}

int getMeVersion(char *ver) {
  ver = ver;
  return -1;
}

int getSystemGuid(string &guid) {
  guid = guid;
  return 0;
}

int getPpinData(int cpu, char *ppin) {
  cpu = cpu;
  ppin = ppin;
  return -1;
}

void platformInit(uint8_t fru) {
  fru = fru;
}
