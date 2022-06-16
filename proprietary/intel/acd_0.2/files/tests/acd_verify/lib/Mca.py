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

import collections
import warnings
import json
import os


class Mca(Section):
    def __init__(self, jOutput):
        Section.__init__(self, jOutput, "MCA")

        self.nCores = 0
        self.nRegsCores = 0
        self.nRegsUncore = 0
        self.verifySection()
        self.rootNodes = f"cores: {self.nCores}"

    @classmethod
    def createMCA(cls, jOutput):
        if "MCA" in jOutput:
            return cls(jOutput)
        else:
            warnings.warn(
                f"MCA section was not found in this file"
            )
            return None

    def searchCore(self, key, value):
        if type(value) == dict:
            for vKey in value:
                self.searchCore(f"{key}.{vKey}", value[vKey])
        elif (type(value) == str) and not value.startswith('_'):
            self.nRegsCores += 1
            if self.eHandler.isError(value):
                error = self.eHandler.extractError(value)
                if "core" not in self.eHandler.errors:
                    self.eHandler.errors["core"] = {}
                self.eHandler.errors["core"][key] = error

    def searchUncore(self, key, value):
        if type(value) == dict:
            for vKey in value:
                self.searchUncore(f"{key}.{vKey}", value[vKey])
        elif (type(value) == str) and not value.startswith('_'):
            self.nRegsUncore += 1
            if self.eHandler.isError(value):
                error = self.eHandler.extractError(value)
                if "uncore" not in self.eHandler.errors:
                    self.eHandler.errors["uncore"] = {}
                self.eHandler.errors["uncore"][key] = error

    def verifySection(self):
        for key in self.sectionInfo:
            if key.startswith("core"):
                self.nCores += 1
                self.searchCore(key, self.sectionInfo[key])
            elif key.startswith("uncore"):
                self.searchUncore(key, self.sectionInfo[key])
            # Considering core and uncore as the valid sections
            #  for the MCA region of the output file
            elif not key.startswith('_'):
                warnings.warn(f"Key {key}, not expected in {self.sectionName}")
        self.makeSelfCheck()

    def getTableInfoCore(self):
        tableInfo = {
            "Section": "MCA_Core",
            "rootNodes": self.rootNodes,
            "regs": self.nRegsCores
        }
        if "core" in self.eHandler.errors:
            coreErrors = self.eHandler.errors["core"]
            level = dict(collections.Counter(coreErrors.values()))
            for error in level:
                if error not in tableInfo:
                    tableInfo[error] = level[error]
                elif error in tableInfo:
                    tableInfo[error] = tableInfo[error] + level[error]

        return tableInfo

    def getTableInfoUncore(self):
        tableInfo = {
            "Section": "MCA_Uncore",
            "rootNodes": "",
            "regs": self.nRegsUncore
        }
        if "uncore" in self.eHandler.errors:
            coreErrors = self.eHandler.errors["uncore"]
            level = dict(collections.Counter(coreErrors.values()))
            for error in level:
                if error not in tableInfo:
                    tableInfo[error] = level[error]
                elif error in tableInfo:
                    tableInfo[error] = tableInfo[error] + level[error]

        return tableInfo

    def getTableInfo(self):
        return [self.getTableInfoCore(), self.getTableInfoUncore()]

    def getErrorList(self):
        coreErrors = {}
        uncoreErrors = {}
        if "core" in self.eHandler.errors:
            coreErrors = self.eHandler.errors["core"]

        if "uncore" in self.eHandler.errors:
            uncoreErrors = self.eHandler.errors["uncore"]

        return [coreErrors, uncoreErrors]

    def getCompareInfo(self, compareSections, ignoreList=False):
        # Load Ignore list
        __location__ = os.path.realpath(
            os.path.join(os.getcwd(), os.path.dirname(__file__)))
        f = open(os.path.join(__location__, 'compareIgnore.json'))
        jCompare = json.load(f)
        self.ignoreList = jCompare[self.sectionName]

        # sectionInfo is equal in both files
        if compareSections.sectionInfo == self.sectionInfo:
            pass
        # Something is diff
        else:
            for key in compareSections.sectionInfo:
                # First check key(reg) exists in both files
                if key not in self.sectionInfo:
                    # [file process, file compare]
                    self.diffList[key] = ["Not present", ""]
        return self.diffList
