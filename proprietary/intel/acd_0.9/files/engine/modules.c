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
 *****************************************************************************/
#include "modules.h"

ModuleInfo moduleInfo;

acdStatus loadModule(void** handle, char* moduleName)
{
    /*** Load SO Module  ***/
    if (moduleName == NULL)
    {
        return ACD_MODULE_NOT_FOUND;
    }

    CRASHDUMP_PRINT(ERR, stderr, "Loading Module: %s\n", moduleName);
    *handle = dlopen(moduleName, RTLD_LOCAL | RTLD_LAZY);
    if (*handle > NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Loading module(%s) Successful\n",
                        moduleName);
        return ACD_SUCCESS;
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "Loading failed. %s\n", dlerror());
        return ACD_MODULE_NOT_FOUND;
    }
}

acdStatus validateModule(void* handle)
{
    // Find rm_init in module
    moduleInfo.fp_init = (int (*)(RemoteModuleInfo*))dlsym(handle, "rm_init");
    if (NULL == moduleInfo.fp_init)
    {
        return ACD_MODULE_INVALID_API;
    }
    // Find rm_config in module
    moduleInfo.fp_config =
        (int (*)(RemoteModuleInfo*, RemoteModuleConfig*))dlsym(handle,
                                                               "rm_config");
    if (NULL == moduleInfo.fp_config)
    {
        return ACD_MODULE_INVALID_API;
    }
    // Find rm_execute in module
    moduleInfo.fp_execute =
        (int (*)(RemoteModuleInfo*, RemoteModuleResponse*))dlsym(handle,
                                                                 "rm_execute");
    if (NULL == moduleInfo.fp_execute)
    {
        return ACD_MODULE_INVALID_API;
    }

    // Find rm_deinit() in module
    moduleInfo.fp_deinit =
        (int (*)(RemoteModuleInfo*))dlsym(handle, "rm_deinit");
    if (NULL == moduleInfo.fp_deinit)
    {
        return ACD_MODULE_INVALID_API;
    }
    return ACD_SUCCESS;
}

acdStatus moduleCallInit(RemoteModuleInfo* remoteModuleInfo)
{
    // Call init()
    if (moduleInfo.fp_init != NULL)
    {
        rmStatus result = (*moduleInfo.fp_init)(remoteModuleInfo);
        if (result == STATUS_SUCCESS)
        {
            return ACD_SUCCESS;
        }
    }

    return ACD_MODULE_INVALID_API;
}
acdStatus moduleCallConfig(RemoteModuleInfo* remoteModuleInfo,
                           RemoteModuleConfig* remoteModuleConfig)
{
    // Call config()
    if (moduleInfo.fp_config != NULL)
    {
        rmStatus result =
            (*moduleInfo.fp_config)(remoteModuleInfo, remoteModuleConfig);
        if (result == STATUS_SUCCESS)
        {
            return ACD_SUCCESS;
        }
    }
    return ACD_MODULE_INVALID_API;
}

acdStatus moduleCallExecute(RemoteModuleInfo* remoteModuleInfo,
                            RemoteModuleResponse* remoteModuleResponse)
{
    // Call execute()
    if (moduleInfo.fp_execute != NULL)
    {
        rmStatus result =
            (*moduleInfo.fp_execute)(remoteModuleInfo, remoteModuleResponse);
        if (result == STATUS_SUCCESS)
        {
            return ACD_SUCCESS;
        }
        else
        {
            return result;
        }
    }
    return ACD_MODULE_INVALID_API;
}

acdStatus moduleCallDeinit(RemoteModuleInfo* remoteModuleInfo)
{
    // Call deinit()
    if (moduleInfo.fp_deinit != NULL)
    {
        rmStatus result = (*moduleInfo.fp_deinit)(remoteModuleInfo);
        if (result == STATUS_SUCCESS)
        {
            return ACD_SUCCESS;
        }
        else
        {
            return result;
        }
    }
    return ACD_MODULE_INVALID_API;
}
acdStatus unloadModule(void* handle)
{
    acdStatus result = ACD_SUCCESS;

    CRASHDUMP_PRINT(ERR, stderr, "Unloading Module\n");
    if (handle != NULL)
    {
        dlclose(handle);
    }
    else
    {
        return ACD_INVALID_OBJECT;
    }

    if (result == 0)
    {
        return ACD_SUCCESS;
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "Unload failed %s\n", dlerror());
        return ACD_INVALID_OBJECT;
    }
}
void resetModuleInfo(RemoteModuleInfo* rmInfo)
{
    rmInfo->boolCount = 0;
    rmInfo->intCount = 0;
    rmInfo->strCount = 0;
}
