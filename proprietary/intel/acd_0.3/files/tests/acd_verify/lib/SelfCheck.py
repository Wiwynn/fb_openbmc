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

from lib.Region import Region


class SelfCheck(Region):
    def processRequest(self, request, report):
        if "selfCheck" in request["regions"]:
            errorList = {}
            report["timeInfo"] = {}
            for key in request["sections"]:
                if key == "metadata":
                    metadataObj = request["sections"]["metadata"]
                    if hasattr(metadataObj, 'getSelfCheckInfo'):
                        errorList["metadata"] = metadataObj.getSelfCheckInfo()
                    report["timeInfo"]["metadata"] =\
                        metadataObj.getTimeInfo()

                elif ("cpu" in key) and ('dies' not in report):
                    errorList[key] = {}
                    report["timeInfo"][key] = {}
                    self.processSectionOf(
                        request["sections"][key],
                        errorList[key],
                        report["timeInfo"][key])

                elif ("cpu" in key) and ('dies' in report):
                    errorList[key] = {}
                    report["timeInfo"][key] = {}
                    for die in request["sections"][key]:
                        errorList[key][die] = {}
                        report["timeInfo"][key][die] = {}
                        self.processSectionOf(
                            request["sections"][key][die],
                            errorList[key][die],
                            report["timeInfo"][key][die])

            report["selfCheck"] = errorList
            return True

    def processSectionOf(self, requestSections, errorListSections, socketTimeInfo):
        for section in requestSections:
            sectionObj = requestSections[section]
            hasCheckFunc = hasattr(sectionObj,
                                    'getSelfCheckInfo')
            if hasCheckFunc:
                errorListSections[section] =\
                    sectionObj.getSelfCheckInfo()
           
            hasGetTimeInfo = hasattr(sectionObj,
                                    'getTimeInfo')
            if hasGetTimeInfo:
                socketTimeInfo[section] = sectionObj.getTimeInfo()
