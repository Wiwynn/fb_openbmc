###############################################################################
# INTEL CONFIDENTIAL                                                          #
#                                                                             #
# Copyright 2023 Intel Corporation.                                           #
#                                                                             #
# This software and the related documents are Intel copyrighted materials,    #
# and your use of them is governed by the express license under which they    #
# were provided to you ("License"). Unless the License provides otherwise,    #
# you may not use, modify, copy, publish, distribute, disclose or transmit    #
# this software or the related documents without Intel's prior written        #
# permission.                                                                 #
#                                                                             #
# This software and the related documents are provided as is, with no express #
# or implied warranties, other than those that are expressly stated in the    #
# License.                                                                    #
###############################################################################
import json
import os
import re

import importlib

import sys
import io


def collect_all_messages_chk(fileName, acdData):
    old_stdout = sys.stdout
    new_stdout = io.StringIO()
    sys.stdout = new_stdout
    
    try:
        checkJSONSchema(fileName, acdData)
    finally:
        sys.stdout = old_stdout
        all_messages = new_stdout.getvalue()
        print(all_messages)

    return all_messages


# read filename's data and return data of json
def read_json(filename=None):
    json_object = None
    crashfile = open(filename, 'r')
    try:
        json_object = json.load(crashfile)
        crashfile.close()
    except Exception as e:
        json_object = None
        print(filename, str(e))
    return json_object

###############################################################################
# Support to check:                                                           #
# 1. hierachy according to template                                           #
# 2. io and compute section number according to die_mask                      #
# 3. MCA CHA individual banks according to cha_count                          #
# 4. MCA Core ID according to core_mask minus crashcore_mask                  #
# 5. big_core Core ID according to crashcore_mask                             #
# 6. CPU ID sequnece in both METADATA and PROCESSORS                          #
# 7. values' syntax                                                           #
# 8. validity of crashcore_mask v.s core_mask                                 #
# 9. additionalProperties check                                               #
# 10. terminate if die_mask/core_mask/cha_count is 0 ???                      #
###############################################################################


def getBool(mask, data, template, parent=[], note_dict={}):
    data_key_list = []
    if "additionalProperties" in template.keys() and template["additionalProperties"] == "false":
        data_key_list = list(data.keys())
        
    for key in template:
        if ":" in key:
            rangeInKey(mask, data, template, parent, note_dict, data_key_list, key)
                
        elif isinstance(template[key], dict):
            try:
                # parent.append(key)
                # getBool(mask, data[key], template[key], parent, note_dict)
                # parent.pop()
                if key in data:
                    parent.append(key)
                    getBool(mask, data[key], template[key], parent, note_dict)
                    parent.pop()
                else:
                    print("Schema Check WARNING: Missing %s" % ".".join(parent) + "." + key)
            except Exception as e:
                #print("Schema Check WARNING: Missing %s" % ".".join(parent))
                print(f"Exception: {e}, parent: {parent}")
                parent.pop()
            if key in data_key_list:
                data_key_list.remove(key)
        else:
            if key == "additionalProperties":
                continue
            reg_name = key
            if "%d" in key:
                index = re.search("\d+$",parent[-1])
                if index:
                    reg_name = key.replace("%d",index.group(0))

            #print(parent, key)
            reg = template[key]
            try:
                if reg_name in data:
                    data_val = data[reg_name]
                    if isinstance(data_val, int) and reg == "\d+":
                        continue
                    temp = re.match(reg, str(data_val), re.I | re.M)
                    if temp is None or temp.group() != data_val:
                        print("Schema Check ERROR: %s.%s's value(%s) syntax error." % (".".join(parent), reg_name, data_val))
                else:
                    print("Schema Check ERROR: Missing %s.%s" % (".".join(parent), reg_name))
            except Exception as e:
                #print("Schema Check ERROR: Missing %s.%s" % (".".join(parent), reg_name))
                print(f"Exception: {parent}, {reg_name}: {e}")
            if reg_name in data_key_list:
                data_key_list.remove(reg_name)
    
    if data_key_list:
        print("Schema Check ERROR: Unexpected key(s) {} in {}".format(data_key_list, ".".join(parent)))
    return note_dict

def rangeInKey(mask, data, template, parent, note_dict, data_key_list, key):
    key_reg = key.split(":")
    sta_sign = key_reg[0][0:-2]
    label = key_reg[0][-2:]
    new_temp = ""

    new_temp = ""

    def getDieMask(mask, parent, sta_sign):
        parent.append("die_mask")
        new_temp = ".".join(parent).replace("PROCESSORS","METADATA")
        if mask.get(new_temp, None) is None:
            print("Schema Check ERROR: %s is missing in %s"%(new_temp,".".join(parent)))
            exit(1)
        cnt = mask.get(new_temp)
        num = 0
        if sta_sign.startswith("compute"):
            cnt = int(cnt, 16) & 0xfe0#bit 9~11
        elif sta_sign.startswith("io"):
            cnt = int(cnt, 16) & 0x1f #bit 0~8
        num = str(bin(cnt)).count('1')
        parent.pop()
        return cnt, num
    
    def getChaCount(mask, parent):
        parent.append("cha_count_total")
        new_temp = ".".join(parent).replace("PROCESSORS","METADATA").replace(".soc.MCA","").replace(".soc.TOR","")
        if mask.get(new_temp, None) is None:
            print("Schema Check ERROR: %s is missing in %s"%(new_temp,".".join(parent)))
            exit(1)
        num = int(mask.get(new_temp),16)
        parent.pop()
        return num


    def getCrashCoreMask(mask, parent):
        parent.append("crashcore_mask")
        cnt = "0"
        new_temp = ".".join(parent).replace("PROCESSORS","METADATA").replace(".big_core","").replace(".atom_core", "")
                    #print(new_temp,mask)
        if mask.get(new_temp, None) is None:
            print("Schema Check ERROR: %s is missing in %s"%(new_temp,".".join(parent)))
            exit(1)
        cnt = mask.get(new_temp)
        num = 0
        num = str(bin(int(cnt,16))).count('1')
        parent.pop()
        return cnt, num

    def getCoremaskMinusCrashcore(mask, parent):
        parent.append("coremask_minus_crashcore")
        new_temp = ".".join(parent).replace("PROCESSORS","METADATA").replace(".RDIAMSR", "").replace(".MCA","")
        if mask.get(new_temp, None) is None:
            print("Schema Check ERROR: %s is missing in %s"%(new_temp,".".join(parent)))
            exit(1)
        cnt = mask.get(new_temp)
        num = 0
        num = str(bin(int(cnt,16))).count('1')
        parent.pop()
        return cnt, num

    
            
    if key_reg[1].isdigit():
        num = int(key_reg[1])
    elif key_reg[1] == "die_mask":
        cnt, num = getDieMask(mask, parent, sta_sign)
    elif key_reg[1] == "cha_count_total":
        num = getChaCount(mask, parent)
    elif key_reg[1] == "coremask_minus_crashcore":
        cnt, num = getCoremaskMinusCrashcore(mask, parent)                  
    elif key_reg[1] == "crashcore_mask":
        cnt, num = getCrashCoreMask(mask, parent)            
    else:
        print("Schema Check ERROR: Unknown mask %s"%key_reg[1])
        exit(1)
        
    data_num = 0
    for data_key in data.keys():
                # data_key.replace(sta_sign, "")
        if data_key.startswith(sta_sign) and ((label == "%d" and data_key.replace(sta_sign, "").isdigit()) or label == "%s"):
            if data_key in data_key_list:
                data_key_list.remove(data_key)
            data_num += 1
            if isinstance(template[key], dict):
                        #print(re.sub("^%s"%sta_sign, "",data_key))
                data_key_num = int(re.sub("^%s"%sta_sign, "",data_key))
                parent.append(sta_sign)
                new_temp = ".".join(parent)
                parent.pop()
                note_dict[new_temp] = note_dict.get(new_temp, 0x0) | (1 << data_key_num)
                parent.append(data_key)
                getBool(mask, data[data_key], template[key], parent, note_dict)
                parent.pop()
            else:
                if key == "additionalProperties":
                    continue
                reg = template[key]
                data_val = data[data_key]
                if isinstance(data_val, int) and reg == "\d+":
                    continue                           
                temp = re.match(reg, str(data_val), re.I | re.M)
                if temp == None or temp.group() != data_val:
                    print("Schema Check ERROR: %s_%s's value(%s) syntax error." % (".".join(parent), data_key, data_val))
    if "die_mask" == key_reg[1]:
        if data_num != num:
            print("Schema Check ERROR: %s need %d %s, but have %d!" % (".".join(parent), num, sta_sign, data_num))
    elif "mask" in key_reg[1]:
        if new_temp not in note_dict.keys():
            if int(cnt,16) != 0:
                print("Schema Check ERROR: %s is %s but no Core data in %s"%(key_reg[1],cnt,".".join(parent)))
        elif note_dict[new_temp] != int(cnt,16): #core_mask crashcore_mask/coremask_minus_crashcore
            mismatch = note_dict[new_temp] ^ int(cnt,16)
            mis_list = []
            for i in range((len(hex(mismatch))-2)*4+1): #len(hex to bin)  F -> 1111
                if (1 << i) & mismatch != 0:
                    mis_list.append(str(i))
            print("Schema Check ERROR: %s mismatch. Core ID %s in %s"%(key_reg[1], " ".join(mis_list), ".".join(parent)))
    else:
        if data_num < num:
            print("Schema Check ERROR: %s need %d %s, but only have %d!" % (".".join(parent), num, sta_sign, data_num))

def getSKU(version):
    product_hex = version.replace("0x", "")
    version_bin = format(int(product_hex, 16), '032b')
    product_bin = version_bin[8:20]
    product_id = int(product_bin, 2)
    product_map = {
        0x2f: "GNR",
        0x84: "GNRD",
        0x82: "SRF",
        0x1c: "SPR",
        0x79: "EMR",
        0x6f: "SPR HBM"
    }
    return product_map.get(product_id, "Unknown")

def getMask(data):
    mask = {}
    metaData = data.get("METADATA", {})
    errorList = []
   
   
    def startsWithCPU(mask, errorList, key, cpuData):
        if cpuData.get("cha_count_total", None) is None:
            print(f"Schema Check ERROR: {key}.cha_count_total is missing!")
            exit()
        chaCount = cpuData.get("cha_count_total")
        mask[f"METADATA.{key}.cha_count_total"] = chaCount

        for cpuKey, cpuValue in cpuData.items():
            if cpuKey.startswith("compute"):
                testMask = {}
                name = f"{key}.{cpuKey}"

                if cpuValue.get("core_mask", None) is None:
                    print(f"Schema Check ERROR: {name}.core_mask is missing!")
                    exit()
                coreMask = cpuValue.get("core_mask")

                if cpuValue.get("crashcore_mask", None) is None:
                    if cpuValue.get("final_crashcore_mask", None) is None:
                        print(f"Schema Check ERROR: {name}.crashcore_mask is missing but also final_crashcore_mask is missing!")
                        exit()
                crashcoreMask = cpuValue.get("final_crashcore_mask", cpuValue.get("crashcore_mask"))

                mask[f"METADATA.{name}.core_mask"] = coreMask
                mask[f"METADATA.{name}.crashcore_mask"] = crashcoreMask
                mask[f"METADATA.{name}.coremask_minus_crashcore"] = hex(int(coreMask, 16) ^ int(crashcoreMask, 16))

                testMask[f"METADATA.{name}.core_mask"] = coreMask

                for testMaskKey, testMaskValue in testMask.items():
                    if testMaskValue == 0:
                        errorList.append(testMaskKey)

                if mask[f"METADATA.{name}.coremask_minus_crashcore"] != hex(int(coreMask, 16) - int(crashcoreMask, 16)):
                    print(f"Schema Check ERROR: {name}.crashcore_mask({crashcoreMask}) doesn't align with core_mask({coreMask})!")

    if not metaData:
        errorList.append("METADATA")
        return mask, errorList

    for key, cpuData in metaData.items():
        if key.startswith("cpu"):
            startsWithCPU(mask, errorList, key, cpuData)

            if cpuData.get("die_mask", None) is None:
                print(f"Schema Check ERROR: METADATA.{key}.die_mask is missing!")
                exit()
            dieMask = cpuData.get("die_mask")
            mask[f"METADATA.{key}.die_mask"] = dieMask
            if dieMask == 0:
                errorList.append(f"METADATA.{key}.die_mask")

    return mask, errorList

def checkJSONSchema(fileName, acdData):
    """Checks the JSON schema in the provided fileName with the given data."""
    
    def getCpuIds(binaryString):
        """Returns a list of CPU IDs that are set in the given binary string."""
        return [id.start() for id in re.finditer("1", binaryString[::-1])]

    version = acdData.get("METADATA", {}).get("_version", "0x0")
    sku = getSKU(version)
    if sku == "Unknown":
        print("Schema Check ERROR: Unknown SKU!")
        return
    module_name = f"lib.Template"
    try:
        if sku == "GNR": 
            template_module = importlib.import_module(module_name).Template_GNR
            print(f"Schema Check: {fileName} is {sku}!")
        elif sku == "SRF":
            template_module = importlib.import_module(module_name).Template_SRF
            print(f"Schema Check: {fileName} is {sku}!")
        elif sku == "GNRD":
            template_module = importlib.import_module(module_name).Template_GNRD
            print(f"Schema Check: {fileName} is {sku}!")
        else:
            print(f"Schema Check ERROR: {fileName} - Unknown CPU. Skipped.")
            return
    except Exception as e:
        print(f"Schema load template: {e}")
        return

    mask, errorList = getMask(acdData)
    if errorList:
        print(f"\nSchema Check Terminated: {fileName} missing {', '.join(errorList)}!\n")
        return
        
    note_dict = getBool(mask, acdData, template_module, parent=[], note_dict={})
    
    metaCpuNum = bin(note_dict["METADATA.cpu"])
    if metaCpuNum.count("0") > 1:
        cpuList = getCpuIds(metaCpuNum)
        print(f"Schema Check ERROR: {fileName} METADATA CPU id {cpuList} sequence error!")
        
    procCpuNum = bin(note_dict["PROCESSORS.cpu"])
    if procCpuNum.count("0") > 1:
        cpuList = getCpuIds(procCpuNum)
        print(f"Schema Check ERROR: {fileName} PROCESSORS CPU id {cpuList} sequence error!")
        
    if note_dict["PROCESSORS.cpu"] != note_dict["METADATA.cpu"]:
        metaCpuCount = metaCpuNum.count("1")
        procCpuCount = procCpuNum.count("1")
        print(f"Schema Check ERROR: {fileName} METADATA({metaCpuCount}) and PROCESSORS({procCpuCount}) CPU number mismatch!\n")
    
    del mask
    del note_dict
