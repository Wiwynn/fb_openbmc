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
#ifndef MODULES_H
#define MODULES_H
#include "RemoteModule.h"
#include "cmdprocessor.h"
#include "crashdump.h"
#include "dlfcn.h"
#include "logger_internal.h"

#define MAX_EXECUTION_COUNT 1000

typedef struct
{
    // Function pointer - rm_execute()
    int (*fp_execute)(RemoteModuleInfo*, RemoteModuleResponse*);
    // Function pointer - rm_init()
    int (*fp_init)(RemoteModuleInfo*);
    // Function pointer - rm_config()
    int (*fp_config)(RemoteModuleInfo*, RemoteModuleConfig*);
    //  Function pointer - rm_deinit()
    int (*fp_deinit)(RemoteModuleInfo*);
} ModuleInfo;

acdStatus loadModule(void** handle, char* moduleName);
acdStatus validateModule(void* handle);
acdStatus unloadModule(void* handle);
acdStatus moduleCallInit(RemoteModuleInfo* remoteModuleInfo);
acdStatus moduleCallConfig(RemoteModuleInfo* remoteModuleInfo,
                           RemoteModuleConfig* remoteModuleConfig);
acdStatus moduleCallExecute(RemoteModuleInfo* remoteModuleInfo,
                            RemoteModuleResponse* remoteModuleResponse);
acdStatus moduleCallDeinit(RemoteModuleInfo* rmInfo);
void resetModuleInfo(RemoteModuleInfo* rmInfo);
#endif // MODULE_EXAMPLE_H
