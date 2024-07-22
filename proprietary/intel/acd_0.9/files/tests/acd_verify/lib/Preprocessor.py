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

from lib.Metadata import Metadata
from lib.Tor import Tor
from lib.Uncore import Uncore
from lib.Mca import Mca
from lib.PM_info import PM_info
from lib.Address_map import Address_map
from lib.Big_core import Big_core
from lib.RDIAMSR import RDIAMSR
from lib.Crashlog import Crashlog
from lib.Atom_core import AtomCore
from lib.SchemaCheck import getSKU

class Preprocessor():
    def __init__(self, outputData, checks=None):
        if "METADATA" not in outputData and "_version" not in outputData["METADATA"]:
            raise Exception("The output data does not contain the required metadata")
        sku = getSKU(outputData["METADATA"]["_version"])
        cpus = self.getCPUs(outputData)
        dies = self.getDies(cpus)
        
        self.sections = {
            "metadata": Metadata.createMetadata(outputData, checks, dies)
        }

        if dies:
            self.sections['dies'] = dies

        for cpu in cpus:
            if dies and 'soc' not in dies[cpu]:
                self.sections[cpu] = {}
                # IOs
                for io in dies[cpu]['ios']:
                    self.sections[cpu][io] = {}
                    ioInfo = {
                        "mca": Mca.createMCA(cpus[cpu][io], checks),
                        "uncore": Uncore.createUncore(
                                cpus[cpu][io], cpu,
                                self.sections['metadata'].sectionInfo[cpu]['cpuid']),
                        "crashlog": Crashlog.createCrashlog(cpus[cpu][io], checks),
                    }
                    self.sections[cpu][io] = ioInfo
                # Compute
                for compute in dies[cpu]['computes']:
                    computeInfo = {
                        "mca": Mca.createMCA(cpus[cpu][compute], checks),
                        "big_core": Big_core.createBig_core(cpus[cpu][compute], checks),
                        "atom_core": AtomCore.createAtomCore(cpus[cpu][compute], checks),
                    }
                    self.sections[cpu][compute] = computeInfo
            elif dies and 'soc' in dies[cpu]:
                self.sections[cpu] = {}
                # ios
                for io in dies[cpu]['ios']:
                    self.sections[cpu][io] = {}
                    if io == "io0":
                        ioInfo = {
                            "mca": Mca.createMCA(cpus[cpu][io], checks),
                            "RDIAMSR": RDIAMSR.createRDIAMSR(cpus[cpu][io], checks),
                            "crashlog": Crashlog.createCrashlog(cpus[cpu][io], checks)
                        }
                        self.sections[cpu][io] = ioInfo
                    else:
                        ioInfo = {
                            "mca": Mca.createMCA(cpus[cpu][io], checks),
                            "crashlog": Crashlog.createCrashlog(cpus[cpu][io], checks)
                        }
                    self.sections[cpu][io] = ioInfo
                # compute
                for compute in dies[cpu]['computes']:
                    self.sections[cpu][compute] = {}
                    computeInfo = {
                        "mca": Mca.createMCA(cpus[cpu][compute], checks),
                        "RDIAMSR": RDIAMSR.createRDIAMSR(cpus[cpu][compute], checks=None),
                        "crashlog": Crashlog.createCrashlog(cpus[cpu][compute], checks=None)
                    }
                    if sku == "SRF":
                        computeInfo["atom_core"] = AtomCore.createAtomCore(cpus[cpu][compute], checks)
                    elif sku == "GNR":
                        computeInfo["big_core"] = Big_core.createBig_core(cpus[cpu][compute], checks)

                    self.sections[cpu][compute] = computeInfo
                # soc
                if "soc" in dies[cpu]:
                    self.sections[cpu]['soc'] = {}
                    socInfo = {
                        "mca": Mca.createMCA(cpus[cpu]["soc"], checks),
                        "uncore": Uncore.createUncore(
                                #cpus[cpu][io], cpu,
                                cpus[cpu]["soc"], cpu,
                                self.sections['metadata'].sectionInfo[cpu]['cpuid']),
                        "tor": Tor.createTor(cpus[cpu]["soc"])
                        
                    }
                    self.sections[cpu]['soc'] = socInfo
            else:
                if "address_map" in cpus[cpu]:
                    self.sections[cpu] = {
                        "tor": Tor.createTor(cpus[cpu]),
                        "uncore": Uncore.createUncore(
                            cpus[cpu], cpu,
                            self.sections['metadata'].sectionInfo[cpu]['cpuid']),
                        "mca": Mca.createMCA(cpus[cpu], checks),
                        "pm_info": PM_info.createPM_info(cpus[cpu]),
                        "address_map": Address_map.createAdress_map(cpus[cpu]),
                        "big_core": Big_core.createBig_core(cpus[cpu], checks)
                    }
                else:
                    self.sections[cpu] = {
                        "tor": Tor.createTor(cpus[cpu]),
                        "uncore": Uncore.createUncore(
                            cpus[cpu], cpu,
                            self.sections['metadata'].sectionInfo[cpu]['cpuid']),
                        "mca": Mca.createMCA(cpus[cpu], checks),
                        "pm_info": PM_info.createPM_info(cpus[cpu]),
                        "big_core": Big_core.createBig_core(cpus[cpu], checks)
                    }

    def getCPUs(self, outputData):
        cpus = {}
        for key in outputData["PROCESSORS"]:
            if "cpu" in key:
                cpus[key] = outputData["PROCESSORS"][key]

        return cpus

    def getDies(self, cpus):
        hasDies = False
        dies = {}
        for cpu in cpus:
            ios = [x for x in cpus[cpu].keys() if "io" in x]
            computes = [x for x in cpus[cpu].keys() if "compute" in x]
            if len(ios) > 0:
                hasDies = True
            if len(computes) > 0:
                hasDies = True

            if hasDies:
                dies[cpu] = {"ios": ios, "computes": computes}
                if 'soc' in cpus[cpu]:
                    dies[cpu]["soc"] = None
            else:
                dies = None
        return dies
