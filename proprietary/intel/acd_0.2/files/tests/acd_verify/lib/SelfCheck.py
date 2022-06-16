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
            for key in request["sections"]:
                if key == "metadata":
                    metadataObj = request["sections"]["metadata"]
                    if hasattr(metadataObj, 'getSelfCheckInfo'):
                        errorList["metadata"] = metadataObj.getSelfCheckInfo()

                if "cpu" in key:
                    errorList[key] = {}
                    for section in request["sections"][key]:
                        sectionObj = request["sections"][key][section]
                        if hasattr(sectionObj, 'getTableInfo'):
                            errorList[key][section] = sectionObj.getSelfCheckInfo()

            # print(errorList)
            report["selfCheck"] = errorList
            return True
