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

#include "test_utils.hpp"

#include <stdio.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "safe_str_lib.h"

bool enableDebug = false;
void Logging(cJSON* cjson, char* description)
{
    if (enableDebug)
    {
        char* jsonStr = NULL;
        if (cjson != NULL)
        {
            jsonStr = cJSON_Print(cjson);
            printf("%s%s\n", description, jsonStr);
        }
        else
        {
            printf("%s(NULL)\n", description);
        }
        free(jsonStr);
    }
}

char* readTestFile(char* filename)
{
    char* buffer = NULL;
    FILE* fp = fopen(filename, "r");
    if (fp != NULL)
    {
        uint64_t length = 0;
        fseek(fp, 0, SEEK_END);
        length = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        buffer = (char*)calloc(length, sizeof(char));
        if (buffer)
        {
            size_t result = 0;
            result = fread(buffer, 1, length, fp);
            if (result != length)
            {
                printf("Size Error!\n");
            }
        }
        fclose(fp);
    }
    else
    {
        printf("Fail to open %s\n", filename);
    }
    return buffer;
}

char* removeQuotes(char* str)
{
    char* ptr = str;
    ptr++;
    ptr[strnlen_s(ptr, sizeof(ptr)) - 1] = 0;
    return ptr;
}

cJSON* getJsonObjectFromPath(const char* path, cJSON* root)
{
    cJSON* currentObject = root;
    char* currentPath = strdup(path);
    char* token;

    while ((token = strsep(&currentPath, "/")) != NULL)
    {
        int mismatch = 1;
        strcmp_s(token, 256, "", &mismatch);
        if (mismatch == 1)
        {
            continue;
        }

        if (cJSON_IsArray(currentObject))
        {
            int index = atoi(token);
            currentObject = cJSON_GetArrayItem(currentObject, index);
        }
        else
        {
            currentObject = cJSON_GetObjectItem(currentObject, token);
        }

        if (currentObject == NULL)
        {
            break;
        }
    }

    return currentObject;
}

void debugcJSON(cJSON* root, const char* savePath, bool verbose)
{
    char* jsonStr = NULL;
    if (root != NULL)
    {
        jsonStr = cJSON_Print(root);
        if (verbose)
        {
            printf("%s\n", jsonStr);
        }
        if (jsonStr != NULL)
        {
            if (savePath != NULL)
            {
                FILE* fp = fopen(savePath, "w");
                fprintf(fp, "%s", jsonStr);
                fclose(fp);
            }
        }
    }
    else
    {
        printf("cJSON is null!\n");
    }
    free(jsonStr);
}
