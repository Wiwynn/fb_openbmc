/**
 * @file platform.h
 * @brief Definition of platform functions for the SDK.
 */

#ifndef COMMON_API_H_
#define COMMON_API_H_

#include "aries_api.h"
#include "aries_margin.h"
#include "aries_link.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHECK_ERR_LOGGING(rc)                                              \
{                                                                          \
    if (rc != ARIES_SUCCESS)                                               \
    {                                                                      \
        ASTERA_ERROR("Unexpected return code: %d", rc);                    \
    }                                                                      \
}

#define CHECK_ERR_CLOSE(rc, fd)                                            \
{                                                                          \
    if (rc != ARIES_SUCCESS)                                               \
    {                                                                      \
        ASTERA_ERROR("Unexpected return code: %d", rc);                    \
        if (fd)                                                            \
        {                                                                  \
             close(fd);                                                    \
        }                                                                  \
    }                                                                      \
}

#define CHECK_ERR_RETURN(rc, fd)                                            \
{                                                                          \
    if (rc != ARIES_SUCCESS)                                               \
    {                                                                      \
        ASTERA_ERROR("Unexpected return code: %d", rc);                    \
        if (fd)                                                            \
        {                                                                  \
             close(fd);                                                    \
        }                                                                  \
        return rc;                                                         \
    }                                                                      \
}


// Fill in the basic config to ariesDeice and I2cDriver structure
AriesErrorType SetupAriesDevice (AriesDeviceType* ariesDevice,
             AriesI2CDriverType* i2cDriver);

// Perform FW update
// AriesErrorType AriestFwUpdate(int bus, int addr, const char* fp);

// Get FW update
AriesErrorType AriesGetFwVersion(uint16_t* version);

// Get FW Health
AriesErrorType AriesGetHealth(uint8_t *health);

// Get Current Temp
// AriesErrorType AriesGetTemp(AriesDeviceType* ariesDevice,
//         AriesI2CDriverType* i2cDriver, int bus, int addr, float* temp);

// Logs eye results to a file
AriesErrorType AriesMargin();

AriesErrorType AriesPrintState();

#ifdef __cplusplus
}
#endif

#endif
