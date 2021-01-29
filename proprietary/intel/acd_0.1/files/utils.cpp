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

#include "utils.hpp"

#include "crashdump.hpp"

#ifdef USE_SYSTEMD
#include <boost/container/flat_map.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/message.hpp>
#else
#include <unistd.h>
#endif
#include <variant>

#ifdef __cplusplus
extern "C" {
#endif

#include <safe_mem_lib.h>
#include <safe_str_lib.h>

#ifdef __cplusplus
}
#endif

namespace crashdump
{
int getBMCVersionDBus(char* bmcVerStr, size_t bmcVerStrSize)
{
#ifdef USE_SYSTEMD
    using ManagedObjectType = boost::container::flat_map<
        sdbusplus::message::object_path,
        boost::container::flat_map<
            std::string, boost::container::flat_map<
                             std::string, std::variant<std::string>>>>;
#endif

    if (bmcVerStr == nullptr)
    {
        return 1;
    }

#ifdef USE_SYSTEMD
    sdbusplus::bus::bus dbus = sdbusplus::bus::new_default_system();
    sdbusplus::message::message getObjects = dbus.new_method_call(
        "xyz.openbmc_project.Software.BMC.Updater",
        "/xyz/openbmc_project/software", "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects");
    ManagedObjectType bmcUpdaterIntfs;
    try
    {
        sdbusplus::message::message resp = dbus.call(getObjects);
        resp.read(bmcUpdaterIntfs);
    }
    catch (sdbusplus::exception_t& e)
    {
        return 1;
    }

    for (const std::pair<
             sdbusplus::message::object_path,
             boost::container::flat_map<
                 std::string, boost::container::flat_map<
                                  std::string, std::variant<std::string>>>>&
             pathPair : bmcUpdaterIntfs)
    {
        boost::container::flat_map<
            std::string,
            boost::container::flat_map<
                std::string, std::variant<std::string>>>::const_iterator
            softwareVerIt =
                pathPair.second.find("xyz.openbmc_project.Software.Version");
        if (softwareVerIt != pathPair.second.end())
        {
            boost::container::flat_map<std::string, std::variant<std::string>>::
                const_iterator versionIt =
                    softwareVerIt->second.find("Version");
            if (versionIt != softwareVerIt->second.end())
            {
                const std::string* bmcVersion =
                    std::get_if<std::string>(&versionIt->second);
                if (bmcVersion != nullptr)
                {
                    size_t copySize =
                        std::min(bmcVersion->size(), bmcVerStrSize - 1);
                    bmcVersion->copy(bmcVerStr, copySize);
                    return 0;
                }
            }
        }
    }
#endif
    return 1;
}
} // namespace crashdump

int cd_snprintf_s(char* str, size_t len, const char* format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(str, len, format, args);
    va_end(args);
    return ret;
}

static uint32_t setMask(uint32_t msb, uint32_t lsb)
{
    uint32_t range = (msb - lsb) + 1;
    uint32_t mask = ((1u << range) - 1);
    return mask;
}

void setFields(uint32_t* value, uint32_t msb, uint32_t lsb, uint32_t setVal)
{
    uint32_t mask = setMask(msb, lsb);
    setVal = setVal << lsb;
    *value &= ~(mask << lsb);
    *value |= setVal;
}

uint32_t getFields(uint32_t value, uint32_t msb, uint32_t lsb)
{
    uint32_t mask = setMask(msb, lsb);
    value = value >> lsb;
    return (mask & value);
}

uint32_t bitField(uint32_t offset, uint32_t size, uint32_t val)
{
    uint32_t mask = (1u << size) - 1;
    return (val & mask) << offset;
}

cJSON* readInputFile(const char* filename)
{
    char* buffer = NULL;
    cJSON* jsonBuf = NULL;
    uint64_t length = 0;
    FILE* fp = fopen(filename, "r");

    if (fp == NULL)
    {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    if (length == -1L)
    {
        fclose(fp);
        return NULL;
    }
    fseek(fp, 0, SEEK_SET);
    buffer = (char*)calloc(length, sizeof(char));
    if (buffer)
    {
        fread(buffer, 1, length, fp);
    }
    fclose(fp);

    // Convert and return cJSON object from buffer
    jsonBuf = cJSON_Parse(buffer);
    FREE(buffer);
    return jsonBuf;
}

cJSON* getCrashDataSection(cJSON* root, char* section, bool* enable)
{
    *enable = false;
    cJSON* child = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(root, "crash_data"), section);

    if (child != NULL)
    {
        *enable = cJSON_IsTrue(cJSON_GetObjectItem(child, RECORD_ENABLE));
    }

    return child;
}

cJSON* getCrashDataSectionRegList(cJSON* root, char* section, char* regType,
                                  bool* enable)
{
    cJSON* child = getCrashDataSection(root, section, enable);

    if (child != NULL)
    {
        return cJSON_GetObjectItemCaseSensitive(
            cJSON_GetObjectItemCaseSensitive(child, regType), "reg_list");
    }

    return child;
}

int getCrashDataSectionVersion(cJSON* root, char* section)
{
    uint64_t version = 0;
    bool enable = false;
    cJSON* child = getCrashDataSection(root, section, &enable);

    if (child != NULL)
    {
        cJSON* jsonVer = cJSON_GetObjectItem(child, "_version");
        if ((jsonVer != NULL) && cJSON_IsString(jsonVer))
        {
            version = strtoull(jsonVer->valuestring, NULL, 16);
        }
    }

    return version;
}

cJSON* selectAndReadInputFile(crashdump::cpu::Model cpuModel, char** filename)
{
    char cpuStr[CPU_STR_LEN] = {0};
    char nameStr[NAME_STR_LEN] = {0};

    switch (cpuModel)
    {
        case crashdump::cpu::skx:
            strcpy_s(cpuStr, sizeof("skx"), "skx");
            break;
        case crashdump::cpu::cpx:
            strcpy_s(cpuStr, sizeof("cpx"), "cpx");
            break;
        case crashdump::cpu::clx:
            strcpy_s(cpuStr, sizeof("clx"), "clx");
            break;
        case crashdump::cpu::icx:
        case crashdump::cpu::icx2:
            strcpy_s(cpuStr, sizeof("icx"), "icx");
            break;
        default:
            fprintf(stderr, "Error selecting input file (CPUID 0x%x).\n",
                    cpuModel);
            return NULL;
    }

    cd_snprintf_s(nameStr, NAME_STR_LEN, OVERRIDE_INPUT_FILE, cpuStr);

    if (access(nameStr, F_OK) != -1)
    {
        fprintf(stderr, "Using override file - %s\n", nameStr);
    }
    else
    {
        cd_snprintf_s(nameStr, NAME_STR_LEN, DEFAULT_INPUT_FILE, cpuStr);
    }

    *filename = (char*)malloc(sizeof(nameStr));
    if (*filename == NULL)
    {
        return NULL;
    }
    strcpy_s(*filename, sizeof(nameStr), nameStr);

    return readInputFile(nameStr);
}

void updateRecordEnable(cJSON* root, bool enable)
{
    cJSON* logSection = NULL;
    logSection = cJSON_GetObjectItemCaseSensitive(root, RECORD_ENABLE);
    if (logSection == NULL)
    {
        cJSON_AddBoolToObject(root, RECORD_ENABLE, enable);
    }
}