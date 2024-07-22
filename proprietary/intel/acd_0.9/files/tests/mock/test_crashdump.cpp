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

extern "C" {
#include "engine/crashdump.h"
}

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
    cpus[0].clientAddr = MIN_CLIENT_ADDR;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        cpus[i].cpuidRead.cpuidValid = true;
        cpus[i].coreMask = 0x0000db7e;
        cpus[i].chaCountRead.chaCountValid = true;
        cpus[i].chaCount = 0;
        cpus[i].coreMaskRead.coreMaskValid = true;
        cpus[i].model = model;
        cpus[i].dieMaskInfo.dieMask = 0x211;
        cpus[i].dieMaskInfo.dieMaskSupported = false;
        cpus[i].crashdumpDiscoveryMask = 0x1;
        cpus[i].crashedCoreMask = 0x0;
        cpus[i].coreType = ACD_BIG_CORE;
        switch (model)
        {
            case cd_gnr:
                cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(GNR_MODEL);
                cpus[i].dieMaskInfo.dieMaskSupported = true;
                break;
            case cd_gnrd:
                cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(GNRD_MODEL);
                cpus[i].dieMaskInfo.dieMaskSupported = true;
                break;
            case cd_srf:
                cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(SRF_MODEL);
                cpus[i].dieMaskInfo.dieMaskSupported = true;
                cpus[i].coreType = ACD_ATOM_CORE;
                break;
            default:
                break;
        }
    }

    root = cJSON_CreateObject();
    setupInputFiles();

    // Delegates for default behavior
    libPeciMock->DelegateWrEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigPciLocal_dom();
}

TestCrashdump::TestCrashdump(Model model, cJSON* flagsOverride) :
    TestCrashdump(model)
{
    setupInputFilesByFlags(flagsOverride);
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

    setupInputFilesBySection(section);

    // Delegates for default behavior
    libPeciMock->DelegateWrEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigPciLocal_dom();
}

TestCrashdump::TestCrashdump(Model model, std::string section, cJSON* flags)
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

    setupInputFilesByFlags(section, flags);

    // Delegates for default behavior
    libPeciMock->DelegateWrEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigPciLocal_dom();
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

    if (model == cd_srf)
    {
        cpus[0].coreType = ACD_ATOM_CORE;
    }
    else
    {
        cpus[0].coreType = ACD_BIG_CORE;
    }

    root = cJSON_CreateObject();
    setupInputFiles();

    // Delegates for default behavior
    libPeciMock->DelegateWrEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigPciLocal_dom();
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
        cpus[c].clientAddr = MIN_CLIENT_ADDR + c;
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

    // Delegates for default behavior
    libPeciMock->DelegateWrEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigPciLocal_dom();
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
        this->cpus[i].coreType = ACD_BIG_CORE;
    }

    libPeciMock = std::make_unique<NiceMock<LibPeciMock>>();
    root = cJSON_CreateObject();
    setupInputFiles();

    // Delegates for default behavior
    libPeciMock->DelegateWrEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigMmio_dom();
    libPeciMock->DelegateRdEndPointConfigPciLocal_dom();
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
                   {cd_emr, "emr"}, {cd_gnrd, "gnrd"}};
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
        strcpy_s(inputFileInfo.defaultPath, MAX_PATH_LEN,
                 OVERRIDE_INPUT_UT_DIR);
        strcpy_s(inputFileInfo.overridePath, MAX_PATH_LEN,
                 OVERRIDE_INPUT_UT_DIR);
        loadInputFiles(this->cpus, &inputFileInfo, false);
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
        strcpy_s(inputFileInfo.defaultPath, MAX_PATH_LEN,
                 OVERRIDE_INPUT_UT_DIR);
        strcpy_s(inputFileInfo.overridePath, MAX_PATH_LEN,
                 OVERRIDE_INPUT_UT_DIR);
        loadInputFiles(this->cpus, &inputFileInfo, false);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

void TestCrashdump::setupInputFilesByFlags(std::string section, cJSON* flags)
{
    try
    {
        std::string OriginalFileName =
            "crashdump_input_" + cpuModelMap[this->cpus[0].model] + ".json";
        std::filesystem::path originalFile =
            std::filesystem::current_path() / ".." / OriginalFileName;
        disableAllSections(section, originalFile);
        std::filesystem::path tmpFile =
            std::filesystem::current_path() / ".." / "Temp.json";

        std::ifstream ifs(tmpFile);
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            (std::istreambuf_iterator<char>()));
        ifs.close();

        cJSON* root = cJSON_Parse(content.c_str());
        if (!root)
        {
            std::cerr << "Error parsing JSON: " << cJSON_GetErrorPtr() << '\n';
            return;
        }

        cJSON* sectionsArray = cJSON_GetObjectItem(root, "Sections");
        if (sectionsArray)
        {
            for (int i = 0; i < cJSON_GetArraySize(sectionsArray); i++)
            {
                cJSON* sectionObject = cJSON_GetArrayItem(sectionsArray, i);
                cJSON* targetSection =
                    cJSON_GetObjectItem(sectionObject, section.c_str());

                if (targetSection)
                {
                    for (cJSON* currentFlag = flags->child; currentFlag;
                         currentFlag = currentFlag->next)
                    {
                        cJSON* replacementItem = NULL;
                        switch (currentFlag->type)
                        {
                            case cJSON_Number:
                                replacementItem = cJSON_CreateNumber(
                                    currentFlag->valuedouble);
                                break;
                            case cJSON_String:
                                replacementItem = cJSON_CreateString(
                                    currentFlag->valuestring);
                                break;
                            case cJSON_True:
                            case cJSON_False:
                                replacementItem = cJSON_CreateBool(
                                    currentFlag->type == cJSON_True);
                                break;
                            default:
                                std::cerr << "Unsupported flag type." << '\n';
                                break;
                        }

                        if (replacementItem)
                        {
                            bool status =
                                cJSON_ReplaceItemInObjectCaseSensitive(
                                    targetSection, currentFlag->string,
                                    replacementItem);
                            if (!status)
                            {
                                std::cerr << "Fail to override flag." << '\n';
                            }
                        }
                        else
                        {
                            std::cerr << "Failed to create a replacement item "
                                         "for the flag."
                                      << '\n';
                        }
                    }
                    break;
                }
            }
        }

        char* updatedContent = cJSON_Print(root);
        if (!updatedContent)
        {
            std::cerr << "Error printing updated JSON" << '\n';
            cJSON_Delete(root);
            return;
        }

        std::ofstream ofs(tmpFile);
        ofs << updatedContent;
        ofs.close();

        cJSON_free(updatedContent);
        cJSON_Delete(root);

        removeInputFiles();
        copyInputFilesToDefaultLocationBySection(tmpFile, OriginalFileName);
        strcpy_s(inputFileInfo.defaultPath, MAX_PATH_LEN,
                 OVERRIDE_INPUT_UT_DIR);
        strcpy_s(inputFileInfo.overridePath, MAX_PATH_LEN,
                 OVERRIDE_INPUT_UT_DIR);
        loadInputFiles(this->cpus, &inputFileInfo, false);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
/// Override a particular keyword in all sections of an input file
void TestCrashdump::setupInputFilesByFlags(cJSON* flags)
{
    try
    {
        std::string OriginalFileName =
            "crashdump_input_" + cpuModelMap[this->cpus[0].model] + ".json";
        std::filesystem::path originalFile =
            std::filesystem::current_path() / ".." / OriginalFileName;
        std::filesystem::path tmpFile =
            std::filesystem::current_path() / ".." / "Temp.json";

        std::ifstream ifs(originalFile);
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            (std::istreambuf_iterator<char>()));
        ifs.close();

        // debug
        std::ofstream ofss(tmpFile);
        ofss << content;
        ofss.close();

        cJSON* root = cJSON_Parse(content.c_str());
        if (!root)
        {
            std::cerr << "Error parsing JSON: " << cJSON_GetErrorPtr() << '\n';
            return;
        }

        cJSON* sectionsArray = cJSON_GetObjectItem(root, "Sections");
        if (sectionsArray)
        {
            int arraysize = cJSON_GetArraySize(sectionsArray);
            for (int i = 0; i < cJSON_GetArraySize(sectionsArray); i++)
            {
                cJSON* sectionObject = cJSON_GetArrayItem(sectionsArray, i);
                cJSON* targetSection = sectionObject->child;
                if (targetSection)
                {
                    for (cJSON* currentFlag = flags->child; currentFlag;
                         currentFlag = currentFlag->next)
                    {
                        cJSON* replacementItem = NULL;
                        switch (currentFlag->type)
                        {
                            case cJSON_Number:
                                replacementItem = cJSON_CreateNumber(
                                    currentFlag->valuedouble);
                                break;
                            case cJSON_String:
                                replacementItem = cJSON_CreateString(
                                    currentFlag->valuestring);
                                break;
                            case cJSON_True:
                            case cJSON_False:
                                replacementItem = cJSON_CreateBool(
                                    currentFlag->type == cJSON_True);
                                break;
                            default:
                                std::cerr << "Unsupported flag type." << '\n';
                                break;
                        }

                        if (replacementItem)
                        {
                            bool status =
                                cJSON_ReplaceItemInObjectCaseSensitive(
                                    targetSection, currentFlag->string,
                                    replacementItem);
                            if (!status)
                            {
                                std::cerr << "Fail to override flag." << '\n';
                            }
                        }
                        else
                        {
                            std::cerr << "Failed to create a replacement item "
                                         "for the flag."
                                      << '\n';
                        }
                    }
                }
            }
        }

        char* updatedContent = cJSON_Print(root);
        if (!updatedContent)
        {
            std::cerr << "Error printing updated JSON" << '\n';
            cJSON_Delete(root);
            return;
        }

        std::ofstream ofs(tmpFile);
        ofs << updatedContent;
        ofs.close();

        cJSON_free(updatedContent);
        cJSON_Delete(root);

        removeInputFiles();
        copyInputFilesToDefaultLocationBySection(tmpFile, OriginalFileName);
        strcpy_s(inputFileInfo.defaultPath, MAX_PATH_LEN,
                 OVERRIDE_INPUT_UT_DIR);
        strcpy_s(inputFileInfo.overridePath, MAX_PATH_LEN,
                 OVERRIDE_INPUT_UT_DIR);
        loadInputFiles(this->cpus, &inputFileInfo, false);
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

EPECIStatus peci_WrPkgConfig_dom(uint8_t target, uint8_t domainId,
                                 uint8_t u8Index, uint16_t u16Param,
                                 const void* pPkgData, uint8_t u8WriteLen,
                                 uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_WrPkgConfig_dom(
        target, domainId, u8Index, u16Param, pPkgData, u8WriteLen, cc);
}

EPECIStatus peci_WrPkgConfig(uint8_t target, uint8_t u8Index, uint16_t u16Param,
                             const void* pPkgData, uint8_t u8WriteLen,
                             uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_WrPkgConfig(
        target, u8Index, u16Param, pPkgData, u8WriteLen, cc);
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
        target, domainId, threadID, MSRAddress, u64MsrVal, cc);
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

EPECIStatus peci_WrEndPointConfigMmio_dom(uint8_t target, uint8_t domainId,
                                          uint8_t u8Seg, uint8_t u8Bus,
                                          uint8_t u8Device, uint8_t u8Fcn,
                                          uint8_t u8Bar, uint8_t u8AddrType,
                                          uint64_t u64Offset, uint8_t u8DataLen,
                                          uint64_t u64DataVal, uint8_t* cc)
{
    return TestCrashdump::libPeciMock->peci_WrEndPointConfigMmio_dom(
        target, domainId, u8Seg, u8Bus, u8Device, u8Fcn, u8Bar, u8AddrType,
        u64Offset, u8DataLen, u64DataVal, cc);
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
