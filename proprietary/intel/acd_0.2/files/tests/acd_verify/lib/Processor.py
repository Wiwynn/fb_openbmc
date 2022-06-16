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

from lib.Summary import Summary
from lib.Table import Table
from lib.SelfCheck import SelfCheck
from lib.OutFileCompare import OutFileCompare
from lib.Region import EndOfReport


class Processor():
    def __init__(self, sections, compareSections=None,
                 ignoreList=False, reportRegions=None):
        self.sections = sections

        self.compareSections = compareSections
        self.ignoreList = ignoreList
        self.reportRegions = None
        if not reportRegions:
            self.reportRegions = ["summary", "table", "selfCheck"]
            if compareSections:
                self.reportRegions.append("compare")
        else:
            self.reportRegions = reportRegions
            if compareSections and ("compare" not in reportRegions):
                self.self.reportRegions.append("compare")

        self.report = self.fillReport()

    def fillReport(self):
        request = {
            "regions": None,
            "sections": self.sections,
            "compare": self.compareSections,
            "ignoreList": self.ignoreList
        }
        report = {}
        handler = OutFileCompare(SelfCheck(Table(Summary(EndOfReport))))
        for region in self.reportRegions:
            request["regions"] = [region]
            handler.handle(request, report)

        return report
