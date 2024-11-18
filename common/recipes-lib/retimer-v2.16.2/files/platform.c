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
 * @file platform.c
 * @brief Implementation of platform specific functions for Linux
 */

#include "platform.h"

int asteraI2COpenConnection(int i2cBus, int slaveAddress)
{
    int handle;
    int quiet = 0;
    char filename[20];
    int size = sizeof(filename);

    snprintf(filename, size, "/dev/i2c/%d", i2cBus);
    filename[size - 1] = '\0';
    handle = open(filename, O_RDWR);

    if (handle < 0 && (errno == ENOENT || errno == ENOTDIR))
    {
        sprintf(filename, "/dev/i2c-%d", i2cBus);
        handle = open(filename, O_RDWR);
    }

    if (handle < 0 && !quiet)
    {
        if (errno == ENOENT)
        {
            fprintf(stderr,
                    "Error: Could not open file '/dev/i2c-%d' or '/dev/i2c/%d':"
                    " %s\n",
                    i2cBus, i2cBus, strerror(ENOENT));
        }
        else
        {
            fprintf(stderr, "Error: Could not open file '%s': %s\n", filename,
                    strerror(errno));
            if (errno == EACCES)
            {
                fprintf(stderr, "Run as root?\n");
            }
        }
    }
    asteraI2CSetSlaveAddress(handle, slaveAddress, 0);
    return handle;
}

int asteraI2CWriteBlockData(int handle, uint8_t cmdCode, uint8_t numBytes,
                            uint8_t* buf)
{
    return i2c_smbus_write_block_data(handle, cmdCode, numBytes, buf);
}

int asteraI2CReadBlockData(int handle, uint8_t cmdCode, uint8_t numBytes,
                           uint8_t* buf)
{
#ifdef SMBUS_BLOCK_READ_UNSUPPORTED
    int rc = i2c_smbus_read_i2c_block_data(handle, cmdCode, numBytes, buf);
#else
    (void)numBytes; // Unused when performing SMBus block read transaction
    int rc = i2c_smbus_read_block_data(handle, cmdCode, buf);
#endif
    return rc;
}

int asteraI2CWriteReadBlockData(int handle, uint8_t cmdCode, uint8_t numBytes,
                                uint8_t* buf)
{
    return i2c_smbus_block_process_call(handle, cmdCode, numBytes, buf);
}

int asteraI2CBlock(int handle)
{
    if (flock(handle, LOCK_EX) < 0)
    {
        fprintf(stderr, "Unable to acquire lock on retimer handle: %s\n",
                strerror(errno));
        return -errno;
    }
    return 0; // Equivalent to ARIES_SUCCESS
}

int asteraI2CUnblock(int handle)
{
    if (flock(handle, LOCK_UN) < 0)
    {
        fprintf(stderr, "Unable to release lock on retimer handle: %s\n",
                strerror(errno));
        return -errno;
    }
    return 0; // Equivalent to ARIES_SUCCESS
}

/**
 * @brief Set I2C slave address
 *
 * @param[in]  handle   I2C handle
 * @param[in]  address  Slave address
 * @param[in]  force    Override user provied slave address with default
 *                      I2C_SLAVE address
 * @return     int      Zero if success, else a negative value
 */
int asteraI2CSetSlaveAddress(int handle, int address, int force)
{
    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    if (ioctl(handle, force ? I2C_SLAVE_FORCE : I2C_SLAVE, address) < 0)
    {
        fprintf(stderr, "Error: Could not set address to 0x%02x: %s\n", address,
                strerror(errno));
        return -errno;
    }
    return 0; // Equivalent to ARIES_SUCCESS
}

/**
 * @brief Set I2C SMBus PEC flag
 *
 * @param[in]  handle   I2C handle
 * @param[in]  pec      SMBus PEC (packet error checking) flag
 * @return     int      Zero if success, else a negative value
 */
int asteraI2CSetPEC(int handle, int pec)
{
    if (ioctl(handle, I2C_PEC, pec) < 0)
    {
        fprintf(stderr, "Error: Could not set PEC to %d: %s\n", pec,
                strerror(errno));
        return -errno;
    }
    return 0; // Equivalent to ARIES_SUCCESS
}

/**
 * @brief Close I2C connection
 *
 * @param[in]  handle   I2C handle
 */
void asteraI2CCloseConnection(int handle)
{
    close(handle);
}
