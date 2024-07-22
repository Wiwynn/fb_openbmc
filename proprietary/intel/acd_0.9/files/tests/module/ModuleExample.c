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

#include <safe_str_lib.h>
#include <safe_mem_lib.h>
#include <string.h>

#include "engine/RemoteModule.h"

int iterationCount = 0;
int StoredNum = 0;
char jsonKeyValue[64];
char node[4][64];

rmStatus rm_init(RemoteModuleInfo* remoteModuleInfo)
{
    memset_s(node, sizeof(node), 0, sizeof(node));
    memset_s(jsonKeyValue, sizeof(jsonKeyValue), 0, sizeof(jsonKeyValue));
    StoredNum = 0;
    return STATUS_SUCCESS;
}

rmStatus rm_config(RemoteModuleInfo* remoteModuleInfo,
                   RemoteModuleConfig* remoteModuleConfig)
{
    for (int i = 0; i < 4; i++)
    {
        sprintf_s(node[i], 64, "node%d", 1);
    }
    // 4 levels
    sprintf_s(node[0], 64, "cpu0");
    sprintf_s(node[1], 64, "imc0");
    sprintf_s(node[2], 64, "ch0");
    sprintf_s(node[3], 64, "b0");

    remoteModuleConfig->executionCount = 320;
    remoteModuleConfig->apiVersion = VERSION_1;
    return STATUS_SUCCESS;
}
rmStatus rm_execute(RemoteModuleInfo* remoteModuleInfo,
                    RemoteModuleResponse* remoteModuleResponse)
{

    sprintf_s(jsonKeyValue, 64, "{ \"key%d\": %d }", StoredNum, StoredNum);
    remoteModuleResponse->jsonString = jsonKeyValue;
    if (StoredNum % 10 == 0)
    {
        sprintf_s(node[3], 64, "b%d", (StoredNum / 10));
    }
    if (StoredNum % 20 == 0)
    {
        sprintf_s(node[2], 64, "ch%d", (StoredNum / 20));
    }
    if (StoredNum % 80 == 0)
    {
        sprintf_s(node[1], 64, "imc%d", (StoredNum / 80));
    }
    if (StoredNum % 160 == 0)
    {
        sprintf_s(node[0], 64, "cpu%d", (StoredNum / 160));
    }
    for (int i = 0; i < 4; i++)
    {
        remoteModuleResponse->nodeLevel[i] = node[i];
    }
    StoredNum += 1;
    return STATUS_SUCCESS;
}

rmStatus rm_deinit(RemoteModuleInfo* remoteModuleInfo)
{
    return STATUS_SUCCESS;
}
