##################################################################################
#
# INTEL CONFIDENTIAL
#
# Copyright 2021 Intel Corporation.
#
# This software and the related documents are Intel copyrighted materials, and
# your use of them is governed by the express license under which they were
# provided to you ("License"). Unless the License provides otherwise, you may
# not use, modify, copy, publish, distribute, disclose or transmit this
# software or the related documents without Intel's prior written permission.
#
# This software and the related documents are provided as is, with no express
# or implied warranties, other than those that are expressly stated in
# the License.
#
##################################################################################

import os.path
import json

def is_binary_present():
    if not os.path.exists("build/bin/bafi") and not os.path.exists("build/bin/bafi.exe"):
        raise IOError("Binary not found")


def determine_system():
    if os.path.exists("build/bin/bafi"):
        return "build/bin/./bafi"
    else:
        return "build\\bin\\bafi.exe"

def append_differences(json_differences, path, generated_value, original_value):
    json_differences.append({ "Property": path, "Generated crashdump": generated_value,
    "Original crashdump": original_value})


def json_extract(dump, dump2):
    arr = []
    key = ""

    def extract(dump, dump2, arr, key):
        if isinstance(dump, dict) and isinstance(dump2, dict):
            for [k, v], [k2, v2] in zip(dump.items(), dump2.items()):
                if k != "_total_time":
                    if isinstance(v, (dict, list)) and isinstance(v2, (dict, list)):
                        key += k + '.'
                        extract(v, v2, arr, key)
                        key = ".".join(key.split(".")[:-2])
                        if key != "":
                            key += '.'
                    elif v != v2:
                        key += k
                        append_differences(arr, key, v, v2)
                        key = ".".join(key.split(".")[:-1])
                        key += '.'
        elif isinstance(dump, list) and isinstance(dump2, list):
            for item, item2 in zip(dump, dump2):
                if isinstance(item, str) and isinstance(item2, str):
                    if item != item2:
                        append_differences(arr, key, item, item2)
                        key = ".".join(key.split(".")[:-1])
                        key += '.'
                else:
                    extract(item, item2, arr, key)
                    key = ".".join(key.split(".")[:-1])
                    key += '.'
        return arr

    values = extract(dump, dump2, arr, key)
    return values


def check_differences(generated_crashdump, original_crashdump):
    generated_crashdump = json.loads(generated_crashdump)
    with open(original_crashdump, 'r') as file:
        original_crashdump = file.read().replace('\n', '')
    original_crashdump = json.loads(original_crashdump)
    json_differences = json_extract(generated_crashdump, original_crashdump)

    if json_differences == []:
        return []
    else:
        return json.dumps(json_differences, indent = 4)