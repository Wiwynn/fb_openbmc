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

#ifdef BIC_PECI_INTF
extern int node_bus_id;
#endif

extern std::string crashdumpFruName;

void getBmcVersion(char *ver) {
  (void)ver;
}

void getBiosVersion(char *ver) {
  (void)ver;
}

void getSystemGuid(std::string &guid) {
  guid = "";
}

void platformInit(uint8_t fru) {
  (void)fru;
}
