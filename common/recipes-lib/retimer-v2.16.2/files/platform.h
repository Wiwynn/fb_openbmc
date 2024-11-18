/*
 * Copyright 2020 Astera Labs, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file platform.h
 * @brief Definition of platform specific functions for Linux
 */

#ifndef ASTERA_ARIES_SDK_PLATFORM_H_
#define ASTERA_ARIES_SDK_PLATFORM_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/file.h>

#include <linux/i2c-dev.h>
// #include <i2c/smbus.h>
#include <openbmc/obmc-i2c.h>

int asteraI2CSetSlaveAddress(int handle, int address, int force);

int asteraI2CSetPEC(int handle, int pec);

void asteraI2CCloseConnection(int handle);

#endif /* ASTERA_ARIES_SDK_PLATFORM_H_ */
