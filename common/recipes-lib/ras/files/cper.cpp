/*
 * Copyright 2024-present Facebook. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "cper.hpp"

#include <syslog.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <unordered_map>
#include <format>
#include <bitset>
#include <memory>
#include "base64.h"

namespace cper
{

namespace fs = std::filesystem;
using ordered_json = nlohmann::ordered_json;

static constexpr auto aerMaskBits = 32;
static constexpr auto pcieDpcErrorId = 0x53;

// PCI Express Base Specification Revision 5.0 Version 1.0/Section 7.8.4
struct AerInfo
{
  uint32_t pcieExtendedCapabilityHeader;
  uint32_t uncorrectableErrorStatus;
  uint32_t uncorrectableErrorMask;
  uint32_t uncorrectableErrorSeverity;
  uint32_t correctableErrorStatus;
  uint32_t correctableErrorMask;
  uint32_t advancedErrorCapabilitiesAndControl;
  uint8_t  headerLog[16];
  uint32_t rootErrorCommand;
  uint32_t rootErrorStatus;
  uint32_t errorSourceIdentification;
  uint8_t  TLPPrefixLog[40];
};

struct PCIeErrorIdInfo{
  int errorId;
  std::string errorDesc;
};

static const PCIeErrorIdInfo uncorrectableErrorIdTable[aerMaskBits] = {
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {0x20, "Data Link Protocol Error"},
    {0x21, "Surprise Down Error"},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {0x22, "Poisoned TLP"},
    {0x23, "Flow Control Protocol Error"},
    {0x24, "Completion Timeout"},
    {0x25, "Completer Abort"},
    {0x26, "Unexpected Completion"},
    {0x27, "Receiver Buffer Overflow"},
    {0x28, "Malformed TLP"},
    {0x29, "ECRC Error"},
    {0x2A, "Unsupported Request"},
    {0x2B, "ACS Violation"},
    {0x2C, "Uncorrectable Internal Error"},
    {0x2D, "MC Blocked TLP"},
    {0x2E, "AtomicOp Egress Blocked"},
    {0x2F, "TLP Prefix Blocked Error"},
    {0x30, "Poisoned TLP Egress Blocked"},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
};

static const PCIeErrorIdInfo correctableErrorIdTable[aerMaskBits] = {
    {0x00, "Receiver Error"},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {0x01, "Bad TLP"},
    {0x02, "Bad DLLP"},
    {0x04, "Replay Rollover"},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {0x03, "Replay Time-out"},
    {0x05, "Advisory Non-Fatal"},
    {0x06, "Corrected Internal Error"},
    {0x07, "Header Log Overflow"},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
    {INVALID_ID, INVALID_ERROR_DESC},
};

std::string getErrorDescription(const uint8_t& errorId)
{
  if (errorId == pcieDpcErrorId)
  {
    return "DPC triggered by uncorrectable error";
  }

  auto errorIdInfo = correctableErrorIdTable;
  if (errorId >= 0x20) // Uncorrectable error ID starts at 0x20
  {
    errorIdInfo = uncorrectableErrorIdTable;
  }
  
  for (size_t i = 0; i < aerMaskBits; ++i)
  {
    if (errorId == errorIdInfo[i].errorId)
    {
      return errorIdInfo[i].errorDesc;
    }
  }
  return INVALID_ERROR_DESC;
}

std::string formatHex(uint32_t value, int width)
{
  std::ostringstream oss;
  oss << std::uppercase << std::hex << std::setfill('0')
      << std::setw(width) << value;
  return oss.str();
}

static std::string formatProcessorSEL(const int& socket,
                                      const std::string& errorType,
                                      const std::string& severity)
{
  std::ostringstream oss;
  oss << "Record: Facebook Unified SEL (0xFB), GeneralInfo: ProcessorErr"
      << "(0x" << formatHex(UNIFIED_PROC_ERR) << "), "
      << "Location: Socket " << formatHex(socket) << ", "
      << "Error Type: " << errorType << ", "
      << "Error Severity: " << severity;
  return oss.str();
}

static std::string formatMemorySEL(const int& sled,
                                   const int& socket,
                                   const int& channel,
                                   const std::string& errorType)
{
  std::ostringstream oss;
  oss << "Record: Facebook Unified SEL (0xFB), GeneralInfo: MEMORY_ECC_ERR"
      << "(0x" << formatHex(UNIFIED_MEM_ERR) << "), "
      << "DIMM Slot Location: "
      << "Sled " << formatHex(sled) << "/" 
      << "Socket " << formatHex(socket) << ", "
      << "Channel " << formatHex(channel) << ", "
      << "DIMM Failure Event: " << errorType;
  return oss.str();
}

static std::string formatPCIeSEL(const int& segment, const int& bus,
                                 const int& dev, const int& fun,
                                 const uint8_t& errorId1,
                                 const uint8_t& errorId2)
{
  auto errorDesc1 = getErrorDescription(errorId1);
  auto errorDesc2 = getErrorDescription(errorId2);

  std::ostringstream oss;
  oss << "Record: Facebook Unified SEL (0xFB), GeneralInfo: PCIeErr"
      << "(0x" << formatHex(UNIFIED_PCIE_ERR) << "), "
      << "Segment " << formatHex(segment) << "/"
      << "Bus " << formatHex(bus) << "/"
      << "Dev " << formatHex(dev) << "/"
      << "Fun " << formatHex(fun) << ", "
      << "ErrID2: 0x" << formatHex(errorId2) << "(" << errorDesc2 << "), "
      << "ErrID1: 0x" << formatHex(errorId1) << "(" << errorDesc1 << ")";
  return oss.str();
}

static std::string formatNvidiaSEL(const std::string& sectionType, 
                                   const std::string& severity,
                                   const ordered_json& sectionData)
{
  int errorType = sectionData["errorType"];
  int instanceBase = sectionData["errorInstance"];
  int numberRegs = sectionData["numberRegs"];
  std::string signature = sectionData["signature"];
  uint32_t regValue = 0;

  std::ostringstream oss;
  oss << "Section Type: " << sectionType << ", "
      << "Error Severity: " << severity << ", "
      << "Signature: " << signature << ", "
      << "Error Type: 0x" << formatHex(errorType) << ", "
      << "Instance Base: 0x" << formatHex(instanceBase);

  for (int reg = 0; reg < numberRegs; ++reg)
  {
    regValue = sectionData["registers"][reg]["value"];
    oss << ", Reg" << reg + 1 << " value: 0x" << formatHex(regValue);
  }
  return oss.str();
}

static int parsePCIeAerData(const ordered_json& aerdata,
                            uint8_t& errorId1, uint8_t& errorId2)
{
  auto encoded = aerdata.get<std::string>();

  int32_t decoded_size = 0;
  uint8_t* decoded_buf = base64_decode(encoded.c_str(), encoded.size(), &decoded_size);

  if (decoded_buf == nullptr)
  {
    return CPER_HANDLE_FAIL;
  }

  std::vector<uint8_t> decoded(decoded_buf, decoded_buf + decoded_size);

  errorId1 = INVALID_ID;
  errorId2 = INVALID_ID;
  std::bitset<aerMaskBits> statusBits;
  const PCIeErrorIdInfo* errorIdInfo = nullptr;
  auto aerInfo = reinterpret_cast<const AerInfo*>(decoded.data());

  if (aerInfo->uncorrectableErrorStatus)
  {
    statusBits = aerInfo->uncorrectableErrorStatus;
    errorIdInfo = uncorrectableErrorIdTable;
  }
  else if (aerInfo->correctableErrorStatus)
  {
    statusBits = aerInfo->correctableErrorStatus;
    errorIdInfo = correctableErrorIdTable;
  }
  else
  {
    return CPER_HANDLE_SUCCESS;
  }

  for (int i = 0; i < aerMaskBits; ++i)
  {
    if (statusBits[i])
    {
      if (errorId1 == INVALID_ID)
      {
        errorId1 = errorIdInfo[i].errorId;
      }
      else if (errorId2 == INVALID_ID)
      {
        errorId2 = errorIdInfo[i].errorId;
      }
      else
      {
        break; // only supports reporting two errors
      }
    }
  }

  return CPER_HANDLE_SUCCESS;
}

static int processorSectionHandler(const ordered_json& sectionDescriptor,
                                   const ordered_json& sectionData,
                                   std::string& errorMsg)
{
  if (!sectionDescriptor.contains("severity") ||
      !sectionData.contains("errorInfo"))
  {
    return CPER_HANDLE_FAIL;
  }

  std::string errorType = sectionData["errorInfo"][0]["errorType"]["name"];
  std::string severity = sectionDescriptor["severity"]["name"];

  errorMsg = formatProcessorSEL(0x00, errorType, severity);
  return CPER_HANDLE_SUCCESS;
}

static int memorySectionHandler(const ordered_json& sectionDescriptor,
                                const ordered_json& sectionData,
                                std::string& errorMsg)
{
  if (!sectionData.contains("memoryErrorType") ||
      !sectionData.contains("node") ||
      !sectionData.contains("device"))
  {
    return CPER_HANDLE_FAIL;
  }

  int socket = sectionData["node"];
  int channel = sectionData["device"];
  std::string errorType = sectionData["memoryErrorType"]["name"];
  
  errorMsg = formatMemorySEL(0x00, socket, channel, errorType);
  return CPER_HANDLE_SUCCESS;
}

static int pcieSectionHandler(const ordered_json& sectionDescriptor,
                              const ordered_json& sectionData,
                              std::string& errorMsg)
{
  if (!sectionData.contains("deviceID") ||
      !sectionData.contains("aerInfo"))
  {
    return CPER_HANDLE_FAIL;
  }

  const auto& aerdata = sectionData["aerInfo"]["data"];
  uint8_t errorId1, errorId2;

  if (parsePCIeAerData(aerdata, errorId1, errorId2))
  {
    return CPER_HANDLE_FAIL;
  }

  int segment = sectionData["deviceID"]["segmentNumber"];
  int bus = sectionData["deviceID"]["primaryOrDeviceBusNumber"];
  int dev = sectionData["deviceID"]["deviceNumber"];
  int func = sectionData["deviceID"]["functionNumber"];

  errorMsg = formatPCIeSEL(segment, bus, dev, func, errorId1, errorId2);
  return CPER_HANDLE_SUCCESS;
}

static int nvidiaSectionHandler(const ordered_json& sectionDescriptor,
                                const ordered_json& sectionData,
                                std::string& errorMsg)
{
  static const std::vector<std::string> procSignatures = {
    "MCF", "DCC-COH", "DCC-ECC", "CLink", "CCPLEXGIC", "CCPLEXSCF",
    "C2C", "C2C-IP-FAIL", "SOCHUB", "HSM",
  };

  static const std::vector<std::string> pcieSignatures = {
    "PCIe", "PCIe-DPC",
  };

  if (!sectionDescriptor.contains("severity") ||
      !sectionData.contains("severity") ||
      !sectionData.contains("numberRegs") ||
      !sectionData.contains("registers"))
  {
    return CPER_HANDLE_FAIL;
  }

  std::string sectionType = sectionDescriptor["sectionType"]["type"];
  std::string severity = sectionDescriptor["severity"]["name"];
  std::string signature = sectionData["signature"];

  if (std::find(procSignatures.begin(), procSignatures.end(), signature)
      != procSignatures.end())
  {
    errorMsg = formatProcessorSEL(0x00, signature, severity);
  }
  else if (std::find(pcieSignatures.begin(), pcieSignatures.end(), signature)
          != pcieSignatures.end())
  {
    // TODO: Populate the BDF and error IDs with valid values. 
    // The BDF is first assigned an initial value of 00 to establish the 
    // PCIe unified SEL until we confirm with NVIDIA how to retrieve the BDF 
    // from the NVIDIA CPER.
    int segment = 0x00, bus = 0x00, dev = 0x00, fun = 0x00;
    uint8_t errorId1 = INVALID_ID, errorId2 = INVALID_ID;

    if (signature == "PCIe-DPC")
    {
      errorId1 = pcieDpcErrorId;
    }

    errorMsg = formatPCIeSEL(segment, bus, dev, fun, errorId1, errorId2);
  }
  else
  {
    errorMsg = formatNvidiaSEL(sectionType, severity, sectionData);
  }
  
  return CPER_HANDLE_SUCCESS;
}

int __attribute__((weak))
addOemSectionHandlerMap(SectionHandlerMap& sectionHandlerMap)
{
  // override this function if you need to replace an existing handler
  // or add OEM-defined sections.
  return CPER_HANDLE_SUCCESS;
}

static SectionHandlerMap getSectionHandler()
{
  SectionHandlerMap sectionHandlerMap = {
    {"ARM", processorSectionHandler},
    {"Platform Memory", memorySectionHandler},
    {"PCIe", pcieSectionHandler},
    {"NVIDIA", nvidiaSectionHandler},
  };

  addOemSectionHandlerMap(sectionHandlerMap);

  return sectionHandlerMap;
}

std::string handleSectionData(const ordered_json& sectionDescriptor,
                              const ordered_json& sectionData)
{
  auto sectionHandlerMap = getSectionHandler();
  std::string sectionType = sectionDescriptor["sectionType"]["type"];
  std::string severity = sectionDescriptor["severity"]["name"];
  std::string errorMsg;

  auto it = sectionHandlerMap.find(sectionType);
  if (it != sectionHandlerMap.end())
  {
    try
    {
      if (it->second(sectionDescriptor, sectionData, errorMsg))
      {
        throw std::runtime_error("Invalid Section Data Format");
      }
      return errorMsg;
    }
    catch(const std::exception& err)
    {
      errorMsg = "Invalid Section Data Format";
      syslog(LOG_ERR, "Failed to handle section data. %s", err.what());
    }
  }
  else
  {
    errorMsg = "Unknown Section Type";
  }

  std::ostringstream oss;
  oss << "Error Type: " << sectionType << ", "
      << "Error Severity: " << severity << ", "
      << errorMsg;

  return oss.str();
}

int parseCperFile(const std::string& file, std::vector<std::string>& entries)
{
  try
	{
		std::ostringstream cmd;
		auto tempFile = file + ".json";

    // convert CPER raw data to JSON format for further parsing
		cmd << CPER_CONVERT << " to-json " << file << " --out " << tempFile;
		auto rc = std::system(cmd.str().c_str());
		if (WIFEXITED(rc) && (WEXITSTATUS(rc) != 0))
		{
      syslog(LOG_ERR, "Failed to execute %s, rc=%d", cmd.str().c_str(), rc);
      return CPER_HANDLE_FAIL;
		}

		if (!fs::exists(tempFile))
		{
      syslog(LOG_ERR, "CPER json file %s, does not exist", tempFile.c_str());
			return CPER_HANDLE_FAIL;
		}

		std::ifstream ifs(tempFile);
		std::remove(tempFile.c_str());
		ordered_json cperJson;
		ifs >> cperJson;

		if (!cperJson.contains("header") ||
				!cperJson.contains("sectionDescriptors") ||
				!cperJson.contains("sections"))
		{
      syslog(LOG_ERR, "Invalid CPER format found in file %s", tempFile.c_str());
      return CPER_HANDLE_FAIL;
		}

		size_t sectionCount = cperJson["header"]["sectionCount"];
    for (size_t i = 0; i < sectionCount; ++i)
		{
			auto& sectionDescriptor = cperJson["sectionDescriptors"][i];
			auto& sectionData = cperJson["sections"][i];
      entries.emplace_back(handleSectionData(sectionDescriptor, sectionData));
		}
	}
	catch(const std::exception& e)
	{
		syslog(LOG_ERR, "Failed to parse CPER file: %s, %s", file.c_str(), e.what());
    return CPER_HANDLE_FAIL;
	}

  return CPER_HANDLE_SUCCESS;
}

void limitDumpEntries()
{
  auto begin = fs::directory_iterator(CPER_DUMP_PATH);
  auto end = fs::directory_iterator{};
  auto totalDumps = std::count_if(begin, end, [](const auto& entry) {
      return fs::is_regular_file(entry);
  });

  if (totalDumps < CPER_DUMP_MAX_LIMIT)
  {
    return;
  }

  // get the oldest dumps
  std::ostringstream cmd;
  auto excessDumps = totalDumps - (CPER_DUMP_MAX_LIMIT - 1);
  cmd << "ls -t -1 " << CPER_DUMP_PATH << " | tail -n " << excessDumps;

  auto pipe_close = [](auto fd) { (void)pclose(fd); };
  std::unique_ptr<FILE, decltype(pipe_close)> pipe(
      popen(cmd.str().c_str(), "r"), pipe_close);

  if (pipe == nullptr)
  {
    throw std::runtime_error("Failed to run popen function!");
  }

  char buf[256] = {0};
  std::vector<std::string> oldestFiles;
  while (fgets(buf, sizeof(buf), pipe.get()) != nullptr)
  {
    std::string result = buf;
    result.erase(result.find_last_not_of(" \n\r\t") + 1);
    oldestFiles.push_back(result);
  }

  // delete the oldest dumps
  for (const auto& file : oldestFiles) {
    auto filePath = CPER_DUMP_PATH + file;
    try
    {
      fs::remove(filePath);
    }
    catch (const fs::filesystem_error& e)
    {
      syslog(LOG_ERR, "Failed to remove file: %s, %s", filePath.c_str(), e.what());
    }
  }
}

int createCperDumpEntry(const uint8_t& payloadId,
                        const uint8_t* data, const size_t& dataSize)
{
  if (!fs::exists(CPER_DUMP_PATH))
  {
    if (!fs::create_directories(CPER_DUMP_PATH))
    {
      syslog(LOG_ERR, "Failed to create %s directory", CPER_DUMP_PATH);
      return CPER_HANDLE_FAIL;
    }
  }

  try
  {
    limitDumpEntries();
  }
  catch(const std::exception& e)
  {
    syslog(LOG_ERR, "Failed to limit CPER dump entries. %s.", e.what());
  }

  // use unix timestamp (milliseconds) to generate the CPER raw data filename
  // e.g. slot1_cper_1520857056153
  std::ostringstream oss;
  uint64_t timestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
  oss << CPER_DUMP_PATH << "slot" << payloadId + 1 << "_cper_" << timestamp;
  auto faultLogFile = fs::path(oss.str());

  std::ofstream ofs;
  ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
  try
  {
    ofs.open(faultLogFile);
    ofs.write(reinterpret_cast<const char*>(data), dataSize);
    ofs.close();
  }
  catch (const std::ios_base::failure& e)
  {
    syslog(LOG_ERR, "Failed to save CPER to %s, %s.",
           faultLogFile.c_str(), e.what());
    return CPER_HANDLE_FAIL;
  }

  try
  {
    std::vector<std::string> entries;
    auto filename = faultLogFile.filename().string();

    if (parseCperFile(faultLogFile, entries) != CPER_HANDLE_SUCCESS)
    {
      throw std::runtime_error("Failed to parse CPER file: " + filename);
    }
    for (const auto& entry: entries)
    {
      syslog(LOG_CRIT, "SEL Entry: FRU: %u, %s, Log: %s",
						 payloadId + 1, entry.c_str(), filename.c_str());
    }
  }
  catch(const std::exception& e)
  {
    syslog(LOG_ERR, "Failed to create CPER dump entry, %s.", e.what());
    return CPER_HANDLE_FAIL;
  }

  return CPER_HANDLE_SUCCESS;
}

} // namespace cper
