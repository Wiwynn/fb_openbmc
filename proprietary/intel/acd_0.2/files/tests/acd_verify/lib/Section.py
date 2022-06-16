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

import collections
import json
import os

from lib.Error import Error


class Section():
    def __init__(self, jOutput, sectionName):
        self.sectionName = sectionName

        self.sectionInfo = jOutput[sectionName]
        self.expectedOutputStruct = None
        self.eHandler = Error()
        self.rootNodes = 0
        self.nRegs = 0
        self.healthCheckErrors = []
        self.ignoreList = []
        self.diffList = {}

    def search(self, key, value):
        valueIsValidType = ((type(value) == str) or (type(value) == bool) or
                            (type(value) == int))
        if type(value) == dict:
            for vKey in value:
                self.search(f"{key}.{vKey}", value[vKey])
        elif (valueIsValidType and not value.startswith('_')):
            self.nRegs += 1  # count regs
            if self.eHandler.isError(value):
                error = self.eHandler.extractError(value)
                self.eHandler.errors[key] = error

    def verifySection(self):
        for key in self.sectionInfo:
            self.search(key, self.sectionInfo[key])
        self.makeSelfCheck()

    def getTableInfo(self):
        tableInfo = {
            "Section": self.sectionName,
            "rootNodes": self.rootNodes,
            "regs": self.nRegs
        }

        level = dict(collections.Counter(self.eHandler.errors.values()))
        for error in level:
            if error not in tableInfo:
                tableInfo[error] = level[error]
            elif error in tableInfo:
                tableInfo[error] = tableInfo[error] + level[error]

        return tableInfo

    def getErrorList(self):
        return self.eHandler.errors

    def makeSelfCheck(self):
        # Version Check
        if "_version" in self.sectionInfo:
            version = self.sectionInfo["_version"]
            if not self.eHandler.isError(version):
                self.checkVersion(version)
        else:
            self.healthCheckErrors.append("_version key not present in " +
                                          self.sectionName)

    def checkVersion(self, sVersion):
        version = sVersion.lstrip("0x0")
        # Revision - 7:0 [3:0]
        revision = version[:1] if (version[1] == '0') else version[:2]
        revision = revision.upper()
        # Product - 23:12
        product = version[2:4] if (version[1] == '0') else version[3:5]
        product = product.upper()

        _revision = {
            "address_map": "D",
            "big_core": "4",
            "MCA": "3E",
            "PM_info": "C",
            "TOR": "9",
            "uncore": "8"
        }

        _validProducts = ("2A", "2B", "1A", "1B", "1C",
                          "2C", "2D", "33", "22", "23",
                          "34")

        if self.sectionName in _revision:
            if not (revision == _revision[self.sectionName]):
                # Revision doesn't match
                errMessage = f"Revision for {self.sectionName}'s version " + \
                            "doesn't match with section detected"
                self.healthCheckErrors.append(errMessage)
            if not (product in _validProducts):
                # Product invalid
                errMessage = f"Product 0x{product} in {self.sectionName}'s " +\
                            "version is an invalid product"
                self.healthCheckErrors.append(errMessage)

    def getSelfCheckInfo(self):
        return self.healthCheckErrors

    def getCompareInfo(self, compareSections, ignoreList=False):
        # Load Ignore list
        __location__ = os.path.realpath(
            os.path.join(os.getcwd(), os.path.dirname(__file__)))
        if ignoreList:
            f = open(os.path.join(__location__, 'compareIgnore.json'))
            jCompare = json.load(f)
            self.ignoreList = jCompare[self.sectionName]
        else:
            self.ignoreList = []

        # sectionInfo is equal in both files
        if compareSections.sectionInfo == self.sectionInfo:
            pass
        # Something is diff
        else:
            for key in compareSections.sectionInfo:
                # First check key(reg) exists in both files
                if key not in self.sectionInfo:
                    # [file process, file compare]
                    valueComp = ""
                    if (type(compareSections.sectionInfo[key]) == str):
                        valueComp = compareSections.sectionInfo[key]
                    self.diffList[key] = ["Not present", valueComp]
                # When key is in both files
                else:
                    sectionComp = compareSections.sectionInfo[key]
                    sectionProc = self.sectionInfo[key]

                    self.__recCompare(key, sectionComp, sectionProc)
        return self.diffList

    def __recCompare(self, parent, sectionA, sectionB):
        # sectionA --> fCompare
        # sectionB --> fProccess
        if type(sectionA) == type(sectionB):
            if type(sectionA) == dict:
                # Loop through fCompare
                for key in sectionA:
                    # Not in file processed
                    if key not in sectionB:
                        # [file process, file compare]
                        valueComp = ""
                        if (type(sectionA[key]) == str):
                            valueComp = sectionA[key]
                        self.diffList[key] = ["Not present", valueComp]
                    else:
                        self.__recCompare(
                            f"{parent}.{key}", sectionA[key], sectionB[key])
            elif type(sectionA) == list:
                if sectionA == sectionB:
                    pass
                # List is diff
                else:
                    if parent not in self.ignoreList:
                        # [file process, file compare]
                        self.diffList[parent] = [sectionB, sectionA]
            else:
                if sectionA == sectionB:
                    pass
                else:
                    if parent not in self.ignoreList:
                        # [file process, file compare]
                        self.diffList[parent] = [sectionB, sectionA]
        # Diff types
        else:
            if parent not in self.ignoreList:
                # [file process, file compare]
                self.diffList[parent] = [type(sectionB), type(sectionA)]

    def getDiffNum(self):
        return len(self.diffList)
