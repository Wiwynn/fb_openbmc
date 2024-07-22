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


class Crashlog(Section):
    def __init__(self, jOutput, checks=None):
        Section.__init__(self, jOutput, "crashlog")
        self.verifySection()

    @classmethod
    def createCrashlog(cls, jOutput, checks = None):
        if "crashlog" in jOutput:
            return cls(jOutput, checks)
        else:
            warnings.warn(
                f"crashlog section was not found in this file"
            )
            return None