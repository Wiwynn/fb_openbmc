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


class Summary(Region):
    def processRequest(self, request, report):
        if "summary" in request["regions"]:
            report["summary"] = {}

            for key in request["sections"]:
                if key == "metadata":
                    metadataObj = request["sections"]["metadata"]
                    if metadataObj:
                        if hasattr(metadataObj, 'getSummaryInfo'):
                            report["summary"]["metadata"] =\
                                metadataObj.getSummaryInfo()

                elif ("cpu" in key) and ('dies' not in report):
                    report["summary"][key] = {}
                    self.processSectionOf(
                        request["sections"][key],
                        report["summary"][key])

                elif ("cpu" in key) and ('dies' in report):
                    report["summary"][key] = {}
                    for die in request["sections"][key]:
                        report["summary"][key][die] = {}
                        self.processSectionOf(
                            request["sections"][key][die],
                            report["summary"][key][die])

            return True

    def processSectionOf(self, requestSections, reportSections):
        for section in requestSections:
            sectionObj = requestSections[section]
            if hasattr(sectionObj, 'getSummaryInfo'):
                reportSections[section] =\
                    sectionObj.getSummaryInfo()
