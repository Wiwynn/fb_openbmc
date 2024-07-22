###############################################################################
# INTEL CONFIDENTIAL                                                          #
#                                                                             #
# Copyright 2021 Intel Corporation.                                           #
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

from lib.Section import Section

import warnings


class Tor(Section):
    def __init__(self, jOutput):
        Section.__init__(self, jOutput, "TOR")

        self.chas = 0
        self.verifySection()
        self.rootNodes = f"chas: {self.chas}"

    @classmethod
    def createTor(cls, jOutput):
        if "TOR" in jOutput:
            return cls(jOutput)
        else:
            warnings.warn(
                f"Tor section was not found in this file"
            )
            return None

    def extractStringify(self):
        if self.sectionInfo.get("_encoding") != "string":
            warnings.warn(f"Encoding for {self.sectionName} is not string")
        if self.sectionInfo.get("_delimiter", None) == None:
            warnings.warn(f"Delimiter for {self.sectionName} is not set")
        delimiter = self.sectionInfo["_delimiter"]
        #"_elements":	"0x100",
        if self.sectionInfo.get("_elements", "0x100") != "0x100":
            warnings.warn(f"Elements for {self.sectionName} is not 0x100")
        elements = 256
        for k, v in list(self.sectionInfo.items()):  
            if k.startswith("cha"):
                values = v.split(';')
                if len(values) != 256:
                    warnings.warn(f"Values for {k} in {self.sectionName} are not 256")
                chaDict = {}
                
                for i in range(0, len(values), 8):
                    indexDict = {}
                    for j in range(8):
                        actualIndex = i // 8
                        subIndex = j
                        
                        subIndexKey = f"subindex{subIndex}"
                        value = values[i+j] if i+j < len(values) else "N/A"
                        indexDict[subIndexKey] = value
                    chaDict[f"index{actualIndex}"] = indexDict
                self.sectionInfo[k] = chaDict
        
    def verifySection(self):
        self.extractStringify()
        for key in self.sectionInfo:
            if key.startswith("cha"):
                self.chas += 1
            elif not key.startswith('_'):
                warnings.warn(f"Key {key}, not expected in {self.sectionName}")
            self.search(key, self.sectionInfo[key])

        if self.chas == 0:
            warnings.warn("No chas found in TOR")

        self.makeSelfCheck()

    def getCompareInfo(self, compareSections, ignoreList=False):
        return {}
