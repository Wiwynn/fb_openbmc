/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2022 Intel Corporation.
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("License"). Unless the License provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit
 * this software or the related documents without Intel's prior written
 * permission.
 *
 * This software and the related documents are provided as is, with no express
 * or implied warranties, other than those that are expressly stated in the
 * License.
 *
 ******************************************************************************/

#include "test_crashdump.hpp"

#include <fstream>
using namespace ::testing;

void initCrashdump()
{
    if (nullptr != crashdump::conn.get())
    {
        crashdump::conn =
            std::make_shared<sdbusplus::asio::connection>(crashdump::io);

        crashdump::conn->request_name(crashdump::crashdumpService);
        crashdump::server =
            std::make_shared<sdbusplus::asio::object_server>(crashdump::conn);

        crashdump::storedLogIfaces.reserve(crashdump::numStoredLogs);

        std::shared_ptr<sdbusplus::asio::dbus_interface> ifaceStored =
            crashdump::server->add_interface(
                crashdump::crashdumpPath, crashdump::crashdumpStoredInterface);
    }
}

TestCrashdump::TestCrashdump(Model model)
{
    initializeModelMap();
    initCrashdump();

    libPeciMock = std::make_unique<NiceMock<LibPeciMock>>();

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    // Initialize cpuInfo
    cpus[0].clientAddr = 48;
    cpus[0].model = model;
    cpus[0].coreMask = 0x0000db7e;
    cpus[0].chaCount = 0;
    cpus[0].crashedCoreMask = 0x0;

    root = cJSON_CreateObject();
    setupInputFiles();
}

TestCrashdump::TestCrashdump(Model model, std::string section)
{
    initializeModelMap();
    initCrashdump();

    libPeciMock = std::make_unique<NiceMock<LibPeciMock>>();

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    // Initialize cpuInfo
    cpus[0].clientAddr = 48;
    cpus[0].model = model;
    cpus[0].coreMask = 0x0000db7e;
    cpus[0].chaCount = 0;
    cpus[0].crashedCoreMask = 0x0;

    root = cJSON_CreateObject();

    // Some models use a base model input file: Ex SRF uses GNR
    if (model == cd_srf)
    {
        cpus[0].model = (Model)cd_gnr;
    }
    setupInputFilesBySection(section);
}

TestCrashdump::TestCrashdump(Model model, int delay)
{
    initializeModelMap();
    initCrashdump();

    libPeciMock = std::make_unique<NiceMock<LibPeciMockWithDelay>>(delay);

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    // Initialize cpuInfo
    cpus[0].clientAddr = 48;
    cpus[0].model = model;
    cpus[0].coreMask = 0x0000db7e;
    cpus[0].chaCount = 0;
    cpus[0].crashedCoreMask = 0x0;
    cpus[0].chaCountRead = {};
    cpus[0].cpuidRead = {};
    cpus[0].coreMaskRead = {};

    root = cJSON_CreateObject();
    setupInputFiles();
}

TestCrashdump::TestCrashdump(Model model, int delay, int numberOfCpus)
{
    initializeModelMap();
    initCrashdump();

    libPeciMock = std::make_unique<NiceMock<LibPeciMockWithDelay>>(delay);
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    if (numberOfCpus > MAX_CPUS)
    {
        numberOfCpus = MAX_CPUS;
    }
    for (int c = 0; c < numberOfCpus; c++)
    {
        cpus[c].clientAddr = 48 + c;
        cpus[c].model = model;
        cpus[c].coreMask = 0x0000db7e;
        cpus[c].chaCount = 0;
        cpus[c].crashedCoreMask = 0x0;
        cpus[c].chaCountRead = {};
        cpus[c].cpuidRead = {};
        cpus[c].coreMaskRead = {};
    }

    root = cJSON_CreateObject();
    setupInputFiles();
}

TestCrashdump::TestCrashdump(CPUInfo* cpus)
{
    initializeModelMap();
    initCrashdump();

    for (int i = 0; i < MAX_CPUS; i++)
    {
        this->cpus[i].clientAddr = cpus[i].clientAddr;
        this->cpus[i].model = cpus[i].model;
        this->cpus[i].coreMask = cpus[i].coreMask;
        this->cpus[i].crashedCoreMask = cpus[i].crashedCoreMask;
        this->cpus[i].chaCount = cpus[i].chaCount;
        this->cpus[i].chaCountRead = cpus[i].chaCountRead;
        this->cpus[i].cpuidRead = cpus[i].cpuidRead;
        this->cpus[i].coreMaskRead = cpus[i].coreMaskRead;
    }

    libPeciMock = std::make_unique<NiceMock<LibPeciMock>>();
    root = cJSON_CreateObject();
    setupInputFiles();
}

TestCrashdump::~TestCrashdump()
{
    libPeciMock.reset();
    removeInputFiles();
}

void TestCrashdump::initializeModelMap()
{
    cpuModelMap = {{cd_srf, "srf"}, {cd_gnr, "gnr"},  {cd_spr, "spr"},
                   {cd_icx, "icx"}, {cd_icx2, "icx"}, {cd_icxd, "icx"},
                   {cd_emr, "emr"}};
}

void TestCrashdump::removeInputFiles()
{
    std::filesystem::remove_all(targetPath);
}

void TestCrashdump::setupInputFiles()
{
    try
    {
        std::string fileName =
            "crashdump_input_" + cpuModelMap[this->cpus[0].model] + ".json";
        std::filesystem::path file =
            std::filesystem::current_path() / ".." / fileName;
        copyInputFilesToDefaultLocation(file);
        crashdump::loadInputFiles(this->cpus, &inputFileInfo, false);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

void TestCrashdump::copyInputFilesToDefaultLocation(
    std::filesystem::path sourceFile)
{
    try
    {
        bool status = std::filesystem::create_directories(targetPath);

        if (status)
        {
            std::filesystem::copy(sourceFile, targetPath,
                                  std::filesystem::copy_options::recursive);
        }
    }
    catch (std::exception& e)
    {
        std::cout << e.what();
    }
}

void TestCrashdump::setupInputFilesBySection(std::string section)
{
    try
    {
        std::string OriginalFileName =
            "crashdump_input_" + cpuModelMap[this->cpus[0].model] + ".json";
        std::filesystem::path file =
            std::filesystem::current_path() / ".." / OriginalFileName;
        disableAllSections(section, file);
        std::filesystem::path tmpFile =
            std::filesystem::current_path() / ".." / "Temp.json";
        removeInputFiles();
        copyInputFilesToDefaultLocationBySection(tmpFile, OriginalFileName);
        crashdump::loadInputFiles(this->cpus, &inputFileInfo, false);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

void TestCrashdump::copyInputFilesToDefaultLocationBySection(
    std::filesystem::path sourceFile, std::string originalFileName)
{
    try
    {
        bool status = std::filesystem::create_directories(targetPath);
        if (status)
        {
            std::filesystem::copy(sourceFile, targetPath,
                                  std::filesystem::copy_options::recursive);
            std::filesystem::path tempFile = targetPath / "Temp.json";
            std::filesystem::path outputFile = targetPath / originalFileName;
            std::filesystem::rename(tempFile, outputFile);
        }
    }
    catch (std::exception& e)
    {
        std::cout << e.what();
    }
}

void TestCrashdump::disableAllSections(std::string section,
                                       std::filesystem::path sourceFile)
{
    try
    {
        std::fstream fileIn;
        fileIn.open(sourceFile, std::fstream::in);
        std::fstream fileOut;
        std::string str;
        std::filesystem::path outputFile =
            std::filesystem::current_path() / ".." / "Temp.json";
        fileOut.open(outputFile, std::fstream::out);
        while (std::getline(fileIn, str))
        {
            if (str.find(section) != std::string::npos)
            {
                fileOut << str << '\n';
                std::getline(fileIn, str);
                if (str.find("RecordEnable") != std::string::npos)
                {
                    fileOut << str << '\n';
                    std::getline(fileIn, str);
                }
            }
            else
            {
                if (str.find("RecordEnable") != std::string::npos)
                {
                    fileOut << "        \"RecordEnable\": false," << '\n';
                    std::getline(fileIn, str);
                }
            }
            fileOut << str << '\n';
        }
        fileIn.close();
        fileOut.close();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

std::unique_ptr<LibPeciMock> TestCrashdump::libPeciMock;

EPECIStatus peci_CrashDump_Discovery(uint8_t target, uint8_t subopcode,
                                     uint8_t param0, uint16_t param1,
                                     uint8_t param2, uint8_t u8ReadLen,
                                     uint8_t* pData, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_CrashDump_Discovery(
        target, subopcode, param0, param1, param2, u8ReadLen, pData, cc);
}

EPECIStatus peci_CrashDump_Discovery_dom(uint8_t target, uint8_t domainId,
                                         uint8_t subopcode, uint8_t param0,
                                         uint16_t param1, uint8_t param2,
                                         uint8_t u8ReadLen, uint8_t* pData,
                                         uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_CrashDump_Discovery_dom(
        target, domainId, subopcode, param0, param1, param2, u8ReadLen, pData,
        cc);
}

EPECIStatus peci_CrashDump_GetFrame(uint8_t target, uint16_t param0,
                                    uint16_t param1, uint16_t param2,
                                    uint8_t u8ReadLen, uint8_t* pData,
                                    uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_CrashDump_GetFrame(
        target, param0, param1, param2, u8ReadLen, pData, cc);
}

EPECIStatus peci_CrashDump_GetFrame_dom(uint8_t target, uint8_t domainId,
                                        uint16_t param0, uint16_t param1,
                                        uint16_t param2, uint8_t u8ReadLen,
                                        uint8_t* pData, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_CrashDump_GetFrame_dom(
        target, domainId, param0, param1, param2, u8ReadLen, pData, cc);
}

EPECIStatus peci_Ping(uint8_t target)
{
    return TestCrashdump::libPeciMock->peci_Ping(target);
}

EPECIStatus peci_GetCPUID(const uint8_t clientAddr, CPUModel* cpuModel,
                          uint8_t* stepping, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_GetCPUID(clientAddr, cpuModel,
                                                     stepping, cc);
}

void peci_Unlock(int peci_fd)
{
    return TestCrashdump::libPeciMock->peci_Unlock(peci_fd);
}

EPECIStatus peci_Lock(int* peci_fd, int timeout_ms)
{
    return TestCrashdump::libPeciMock->peci_Lock(peci_fd, timeout_ms);
}

EPECIStatus peci_RdEndPointConfigPciLocal_seq(uint8_t target, uint8_t u8Seg,
                                              uint8_t u8Bus, uint8_t u8Device,
                                              uint8_t u8Fcn, uint16_t u16Reg,
                                              uint8_t u8ReadLen,
                                              uint8_t* pPCIData, int peci_fd,
                                              uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_RdEndPointConfigPciLocal_seq(
        target, u8Seg, u8Bus, u8Device, u8Fcn, u16Reg, u8ReadLen, pPCIData,
        peci_fd, cc);
}

EPECIStatus peci_RdEndPointConfigPciLocal(uint8_t target, uint8_t u8Seg,
                                          uint8_t u8Bus, uint8_t u8Device,
                                          uint8_t u8Fcn, uint16_t u16Reg,
                                          uint8_t u8ReadLen, uint8_t* pPCIData,
                                          uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_RdEndPointConfigPciLocal(
        target, u8Seg, u8Bus, u8Device, u8Fcn, u16Reg, u8ReadLen, pPCIData, cc);
}

EPECIStatus peci_RdEndPointConfigPciLocal_seq_dom(
    uint8_t target, uint8_t domainId, uint8_t u8Seg, uint8_t u8Bus,
    uint8_t u8Device, uint8_t u8Fcn, uint16_t u16Reg, uint8_t u8ReadLen,
    uint8_t* pPCIData, int peci_fd, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_RdEndPointConfigPciLocal_seq_dom(
        target, domainId, u8Seg, u8Bus, u8Device, u8Fcn, u16Reg, u8ReadLen,
        pPCIData, peci_fd, cc);
}

EPECIStatus peci_RdEndPointConfigPciLocal_dom(uint8_t target, uint8_t domainId,
                                              uint8_t u8Seg, uint8_t u8Bus,
                                              uint8_t u8Device, uint8_t u8Fcn,
                                              uint16_t u16Reg,
                                              uint8_t u8ReadLen,
                                              uint8_t* pPCIData, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_RdEndPointConfigPciLocal_dom(
        target, domainId, u8Seg, u8Bus, u8Device, u8Fcn, u16Reg, u8ReadLen,
        pPCIData, cc);
}

EPECIStatus peci_WrEndPointPCIConfigLocal(uint8_t target, uint8_t u8Seg,
                                          uint8_t u8Bus, uint8_t u8Device,
                                          uint8_t u8Fcn, uint16_t u16Reg,
                                          uint8_t DataLen, uint32_t DataVal,
                                          uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_WrEndPointPCIConfigLocal(
        target, u8Seg, u8Bus, u8Device, u8Fcn, u16Reg, DataLen, DataVal, cc);
}

EPECIStatus peci_WrEndPointPCIConfigLocal_dom(uint8_t target, uint8_t domainId,
                                              uint8_t u8Seg, uint8_t u8Bus,
                                              uint8_t u8Device, uint8_t u8Fcn,
                                              uint16_t u16Reg, uint8_t DataLen,
                                              uint32_t DataVal, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_WrEndPointPCIConfigLocal_dom(
        target, domainId, u8Seg, u8Bus, u8Device, u8Fcn, u16Reg, DataLen,
        DataVal, cc);
}

EPECIStatus peci_RdPkgConfig(uint8_t target, uint8_t u8Index, uint16_t u16Value,
                             uint8_t u8ReadLen, uint8_t* pPkgConfig,
                             uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_RdPkgConfig(
        target, u8Index, u16Value, u8ReadLen, pPkgConfig, cc);
}

EPECIStatus peci_RdPkgConfig_dom(uint8_t target, uint8_t domainId,
                                 uint8_t u8Index, uint16_t u16Value,
                                 uint8_t u8ReadLen, uint8_t* pPkgConfig,
                                 uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_RdPkgConfig_dom(
        target, domainId, u8Index, u16Value, u8ReadLen, pPkgConfig, cc);
}

EPECIStatus peci_WrPkgConfig(uint8_t target, uint8_t u8Index, uint16_t u16Param,
                             uint32_t u32Value, uint8_t u8WriteLen, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_WrPkgConfig(
        target, u8Index, u16Param, u32Value, u8WriteLen, cc);
}

EPECIStatus peci_RdIAMSR(uint8_t target, uint8_t threadID, uint16_t MSRAddress,
                         uint64_t* u64MsrVal, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_RdIAMSR(target, threadID,
                                                    MSRAddress, u64MsrVal, cc);
}

EPECIStatus peci_RdIAMSR_dom(uint8_t target, uint8_t domainId, uint8_t threadID,
                             uint16_t MSRAddress, uint64_t* u64MsrVal,
                             uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_RdIAMSR_dom(
        target, threadID, domainId, MSRAddress, u64MsrVal, cc);
}

EPECIStatus peci_RdEndPointConfigMmio(uint8_t target, uint8_t u8Seg,
                                      uint8_t u8Bus, uint8_t u8Device,
                                      uint8_t u8Fcn, uint8_t u8Bar,
                                      uint8_t u8AddrType, uint64_t u64Offset,
                                      uint8_t u8ReadLen, uint8_t* pMmioData,
                                      uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_RdEndPointConfigMmio(
        target, u8Seg, u8Bus, u8Device, u8Fcn, u8Bar, u8AddrType, u64Offset,
        u8ReadLen, pMmioData, cc);
}

EPECIStatus peci_RdEndPointConfigMmio_dom(uint8_t target, uint8_t domainId,
                                          uint8_t u8Seg, uint8_t u8Bus,
                                          uint8_t u8Device, uint8_t u8Fcn,
                                          uint8_t u8Bar, uint8_t u8AddrType,
                                          uint64_t u64Offset, uint8_t u8ReadLen,
                                          uint8_t* pMmioData, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_RdEndPointConfigMmio_dom(
        target, domainId, u8Seg, u8Bus, u8Device, u8Fcn, u8Bar, u8AddrType,
        u64Offset, u8ReadLen, pMmioData, cc);
}

EPECIStatus peci_RdEndPointConfigMmio_seq(
    uint8_t target, uint8_t u8Seg, uint8_t u8Bus, uint8_t u8Device,
    uint8_t u8Fcn, uint8_t u8Bar, uint8_t u8AddrType, uint64_t u64Offset,
    uint8_t u8ReadLen, uint8_t* pMmioData, int peci_fd, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_RdEndPointConfigMmio_seq(
        target, u8Seg, u8Bus, u8Device, u8Fcn, u8Bar, u8AddrType, u64Offset,
        u8ReadLen, pMmioData, peci_fd, cc);
}

EPECIStatus peci_Telemetry_Discovery(uint8_t target, uint8_t subopcode,
                                     uint8_t param0, uint16_t param1,
                                     uint8_t param2, uint8_t u8ReadLen,
                                     uint8_t* pData, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_Telemetry_Discovery(
        target, subopcode, param0, param1, param2, u8ReadLen, pData, cc);
}

EPECIStatus peci_Telemetry_Discovery_dom(uint8_t target, uint8_t domainId,
                                         uint8_t subopcode, uint8_t param0,
                                         uint16_t param1, uint8_t param2,
                                         uint8_t u8ReadLen, uint8_t* pData,
                                         uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_Telemetry_Discovery_dom(
        target, domainId, subopcode, param0, param1, param2, u8ReadLen, pData,
        cc);
}

EPECIStatus peci_Telemetry_GetCrashlogSample(uint8_t target, uint16_t index,
                                             uint16_t sampleId,
                                             uint8_t u8ReadLen, uint8_t* pData,
                                             uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_Telemetry_GetCrashlogSample(
        target, index, sampleId, u8ReadLen, pData, cc);
}

EPECIStatus peci_Telemetry_GetCrashlogSample_dom(
    uint8_t target, uint8_t domainId, uint16_t index, uint16_t sampleId,
    uint8_t u8ReadLen, uint8_t* pData, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_Telemetry_GetCrashlogSample_dom(
        target, domainId, index, sampleId, u8ReadLen, pData, cc);
}

EPECIStatus peci_Telemetry_ConfigWatcherRd(uint8_t target, uint16_t watcher,
                                           uint16_t offset, uint8_t u8ReadLen,
                                           uint8_t* pData, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_Telemetry_ConfigWatcherRd(
        target, watcher, offset, u8ReadLen, pData, cc);
}

EPECIStatus peci_Telemetry_ConfigWatcherRd_dom(uint8_t target, uint8_t domainId,
                                               uint16_t watcher,
                                               uint16_t offset,
                                               uint8_t u8ReadLen,
                                               uint8_t* pData, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_Telemetry_ConfigWatcherRd_dom(
        target, domainId, watcher, offset, u8ReadLen, pData, cc);
}

EPECIStatus peci_Telemetry_ConfigWatcherWr(uint8_t target, uint16_t watcher,
                                           uint16_t offset, uint8_t u8DataLen,
                                           uint8_t* pData, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_Telemetry_ConfigWatcherWr(
        target, watcher, offset, u8DataLen, pData, cc);
}

EPECIStatus peci_Telemetry_ConfigWatcherWr_dom(uint8_t target, uint8_t domainId,
                                               uint16_t watcher,
                                               uint16_t offset,
                                               uint8_t u8DataLen,
                                               uint8_t* pData, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_Telemetry_ConfigWatcherWr_dom(
        target, domainId, watcher, offset, u8DataLen, pData, cc);
}
