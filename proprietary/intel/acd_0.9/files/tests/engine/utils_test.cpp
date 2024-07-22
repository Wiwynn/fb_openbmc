/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2021 Intel Corporation.
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
#include "../mock/test_crashdump.hpp"

extern "C" {
#include <safe_mem_lib.h>

#include "engine/utils.h"
}

#include "tests/test_utils.hpp"

#include <fstream>
#include <streambuf>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#define DECOMPRESS_BUFFER_SIZE 2048
uint8_t* DecompressString(uint8_t* dataToDecompress, uint32_t decompressSize)
{
    int pipefd[2];
    int pipefd_out[2];
    pid_t pid;
    ssize_t result;

    // Create a pipe
    if (pipe(pipefd) == -1 || pipe(pipefd_out) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {                     /* Child process */
        close(pipefd[1]); /* Close unused write end */

        // Redirect stdin/stdout to pipefd_out
        dup2(pipefd[0], STDIN_FILENO);
        dup2(pipefd_out[1], STDOUT_FILENO);
        close(pipefd_out[0]);
        close(pipefd_out[1]);

        // Replace the process image with gzip
        char* args[] = {"gzip", "-dcf", NULL};
        execvp(args[0], args);

        // execvp will only return if an error occurred.
        printf("An error occurred while decompressing the file\n");
        exit(1);
    }
    else
    {                         /* Parent process */
        close(pipefd[0]);     /* Close unused read end */
        close(pipefd_out[1]); /* Close unused write end */

        // Write the compressed data to the pipe
        result = write(pipefd[1], dataToDecompress, decompressSize);
        if (result == -1)
        {
            perror("write");
            return NULL;
        }

        close(pipefd[1]); /* Reader will see EOF */

        // Wait for the child process to finish
        wait(NULL);

        // Read the decompressed data from pipefd_out
        char buffer[DECOMPRESS_BUFFER_SIZE];
        ssize_t len;
        size_t total_len = 0;
        char* decompressed_data = NULL;

        while ((len = read(pipefd_out[0], buffer, sizeof(buffer))) > 0)
        {
            decompressed_data =
                (char*)realloc(decompressed_data, total_len + len);
            memcpy_s(decompressed_data + total_len,
                     DECOMPRESS_BUFFER_SIZE - total_len, buffer, len);
            total_len += len;
        }

        if (len == -1)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        // Null-terminate the decompressed data
        decompressed_data = (char*)realloc(decompressed_data, total_len + 1);
        decompressed_data[total_len] = '\0';

        // Return the decompressed string
        return (uint8_t*)decompressed_data;
    }
}

TEST(UtilsTestFixture, snprintf_s_test)
{

    char jsonItemString[10];
    cd_snprintf_s(jsonItemString, 10, "%s", "testing");
    EXPECT_STREQ(jsonItemString, "testing");
}

TEST(UtilsTestFixture, readInputFileFlag_test)
{
    cJSON* root = NULL;
    bool status = readInputFileFlag(root, true, "ENABLE");
    EXPECT_TRUE(status);
    status = readInputFileFlag(root, false, "ENABLE");
    EXPECT_FALSE(status);
    static const char* rootstrtrue = R"({"ENABLE":true})";
    root = cJSON_Parse(rootstrtrue);
    status = readInputFileFlag(root, true, "ENABLE");
    EXPECT_TRUE(status);
    static const char* rootstrfalse = R"({"ENABLE":false})";
    root = cJSON_Parse(rootstrfalse);
    status = readInputFileFlag(root, true, "ENABLE");
    EXPECT_FALSE(status);
}

TEST(UtilsTestFixture, getNewCrashDataSection_test)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_mmio.json");
    cJSON* section = getNewCrashDataSection(inRoot, "Uncore_MMIO");
    EXPECT_TRUE(section != NULL);
}

TEST(UtilsTestFixture, getNewCrashDataSection_test_NULL)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_mmio.json");
    cJSON* section = getNewCrashDataSection(inRoot, "MMIO");
    EXPECT_TRUE(section == NULL);
}

TEST(UtilsTestFixture, getNewCrashDataSectionObjectOneLevel_test)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_mmio.json");
    cJSON* section =
        getNewCrashDataSectionObjectOneLevel(inRoot, "Uncore_MMIO", "Commands");
    EXPECT_TRUE(section != NULL);
}

TEST(UtilsTestFixture, getNewCrashDataSectionObjectOneLevel_test_NULL)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_mmio.json");
    cJSON* section = getNewCrashDataSectionObjectOneLevel(inRoot, "Uncore_MMIO",
                                                          "Commands2");
    EXPECT_TRUE(section == NULL);
}

TEST(UtilsTestFixture, isCoreRegVersionMatch_test)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_bigcore.json");
    bool status = isCoreRegVersionMatch(inRoot, 0x4029001, ACD_BIG_CORE);
    EXPECT_TRUE(status);
}

TEST(UtilsTestFixture, isCoreRegVersionMatch_test_NULL)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_bigcore.json");
    bool status = isCoreRegVersionMatch(inRoot, 0x4029004, ACD_BIG_CORE);
    EXPECT_FALSE(status);
}

TEST(UtilsTestFixture, getCrashDataSectionBigCoreRegList_test)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_bigcore.json");
    cJSON* output =
        getCrashDataSectionCoreRegList(inRoot, "0x4029001", 0, ACD_BIG_CORE);
    EXPECT_TRUE(output != NULL);
}

TEST(UtilsTestFixture, getCrashDataSectionBigCoreRegList_test_NULL)
{
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_bigcore.json");
    cJSON* output =
        getCrashDataSectionCoreRegList(inRoot, "0x53", 0, ACD_BIG_CORE);
    EXPECT_TRUE(output == NULL);
}

TEST(UtilsTestFixture, getCrashDataSectionBigCoreSize_test)
{
    uint32_t value = 0;
    cJSON* inRoot = readInputFile("../tests/UnitTestFiles/ut_bigcore.json");
    uint32_t output =
        getCrashDataSectionCoreSize(inRoot, "0x4029001", 0, ACD_BIG_CORE);
    EXPECT_EQ(output, value);
}

TEST(UtilsTestFixture, readInputFile_valid_full_flow)
{
    char* InputFile = "../crashdump_input_gnr.json";
    cJSON* inRoot = readInputFile(InputFile);
    EXPECT_TRUE(inRoot != NULL);
}

TEST(UtilsTestFixture, readInputFile_error_non_exitent_file)
{
    char* InputFile = "Testing.json"; // Causes fp == NULL
    cJSON* inRoot = readInputFile(InputFile);
    EXPECT_TRUE(inRoot == NULL);
}

TEST(UtilsTestFixture, readInputFile_error_parsing_cjson)
{
    char* InputFile = "Testing"; // Causes jsonBuf == NULL
    cJSON* inRoot = readInputFile(InputFile);
    EXPECT_TRUE(inRoot == NULL);
}

TEST(UtilsTestFixture, selectAndReadInputFile_cpuModel_notSupported)
{
    char* InputFile = "Testing.json";
    Model cpu = cd_icx;
    cJSON* inRoot = selectAndReadInputFile(cpu, &InputFile, false, NULL, NULL);
    EXPECT_TRUE(inRoot == NULL);
}

TEST(UtilsTestFixture, logResetDetected_negative_test)
{
    static PlatformState platformState = {false, DEFAULT_VALUE};
    cJSON* metadata = cJSON_CreateObject();
    logResetDetected(metadata, &platformState);
    EXPECT_STREQ(metadata->child->string, "_reset_detected");
    EXPECT_STREQ(metadata->child->valuestring, "NONE");
}

TEST(UtilsTestFixture, logResetDetected_postive_test)
{
    PlatformState platformState = {true, 1, "Metadata_Cpu_General"};
    cJSON* metadata = cJSON_CreateObject();
    CPUInfo cpuInfo;
    cJSON* inRoot = readInputFile("../crashdump_input_gnr.json");
    cpuInfo.inputFile.bufferPtr = inRoot;
    logResetDetected(metadata, &platformState);
    EXPECT_STREQ(metadata->child->string, "_reset_detected");
    EXPECT_STREQ(metadata->child->valuestring, "cpu1.Metadata_Cpu_General");
}

TEST(UtilsTestFixture, logResetDetected_test_flow)
{
    PlatformState platformState = {false, DEFAULT_VALUE};
    cJSON* metadata = cJSON_CreateObject();
    CPUInfo cpuInfo;
    cJSON* inRoot;
    int cpuNum = 1;
    int numOfSections = 40;
    int resetSectionIdx = 4; // Input file Sections[4] is Phy2LogCore

    // Mimic two resets and reset test flow in createCrashdump()
    for (int resetCount = 0; resetCount < 1; resetCount++)
    {
        inRoot = readInputFile("../crashdump_input_gnr.json");
        cpuInfo.inputFile.bufferPtr = inRoot;
        for (int i = 0; i < numOfSections; i++)
        {
            updateResetInfo(&platformState, (int)cpuNum,
                            getSectionName(cpuInfo.inputFile.bufferPtr, 0));
            if (i == resetSectionIdx)
            {
                setResetDetected(&platformState);
            }
            cJSON* sections = cJSON_GetObjectItemCaseSensitive(
                cpuInfo.inputFile.bufferPtr, "Sections");
            if (sections != NULL && cJSON_IsArray(sections))
            {
                cJSON_DeleteItemFromArray(sections, 0);
            }
        }
        logResetDetected(metadata, &platformState);
        EXPECT_STREQ(metadata->child->string, "_reset_detected");
        EXPECT_STREQ(metadata->child->valuestring, "cpu1.Phy2LogCore");

        clearResetDetected(&platformState);
        EXPECT_EQ(platformState.resetDetected, false);
        EXPECT_EQ(platformState.resetCpu, DEFAULT_VALUE);
        EXPECT_STREQ(platformState.resetSectionName, "N/A");
    }
}

TEST(UtilsTestFixture, fillCrashdumpVersion_test)
{
    cJSON* metaData = cJSON_CreateObject();
    int status = fillCrashdumpVersion("crashdump_ver", metaData);
    EXPECT_EQ(status, ACD_SUCCESS);
}

TEST(UtilsTestFixture, cJSONToInt_PASS_test)
{
    cJSON* object = cJSON_CreateObject();
    cJSON_AddStringToObject(object, "Register", "0x16");
    int value = cJSONToInt(object->child, 16);
    EXPECT_EQ(value, 22);
    value = cJSONToInt(object, 16);
    EXPECT_EQ(value, 0);
    cJSON* object2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(object, "Register", 2);
    value = cJSONToInt(object2->child, 16);
    EXPECT_EQ(value, 0);
    cJSON* object3 = NULL;
    value = cJSONToInt(object3, 16);
    EXPECT_EQ(value, 0);
}

TEST(UtilsTestFixture, splitTriggerString)
{
    TriggerString split = {{0}, {0}};
    int err;

    // Crashlog Flow
    err = splitTriggerString("IERR.Crashlog", &split);
    ASSERT_TRUE(err == 0);
    ASSERT_STREQ(split.trigger, "IERR");
    ASSERT_STREQ(split.modifier, "Crashlog");

    err = splitTriggerString("MCERR.Crashlog", &split);
    ASSERT_TRUE(err == 0);
    ASSERT_STREQ(split.trigger, "MCERR");
    ASSERT_STREQ(split.modifier, "Crashlog");

    err = splitTriggerString("ERR2.Crashlog", &split);
    ASSERT_TRUE(err == 0);
    ASSERT_STREQ(split.trigger, "ERR2");
    ASSERT_STREQ(split.modifier, "Crashlog");

    err = splitTriggerString("OnDemand.Crashlog", &split);
    ASSERT_TRUE(err == 0);
    ASSERT_STREQ(split.trigger, "OnDemand");
    ASSERT_STREQ(split.modifier, "Crashlog");

    // Normal Flow
    err = splitTriggerString("IERR", &split);
    ASSERT_TRUE(err == 0);
    ASSERT_STREQ(split.trigger, "IERR");
    ASSERT_STREQ(split.modifier, "\0");

    err = splitTriggerString("MCERR", &split);
    ASSERT_TRUE(err == 0);
    ASSERT_STREQ(split.trigger, "MCERR");
    ASSERT_STREQ(split.modifier, "\0");

    err = splitTriggerString("ERR2", &split);
    ASSERT_TRUE(err == 0);
    ASSERT_STREQ(split.trigger, "ERR2");
    ASSERT_STREQ(split.modifier, "\0");

    err = splitTriggerString("OnDemand", &split);
    ASSERT_TRUE(err == 0);
    ASSERT_STREQ(split.trigger, "OnDemand");
    ASSERT_STREQ(split.modifier, "\0");

    err = splitTriggerString("Telemetry", &split);
    ASSERT_TRUE(err == 0);
    ASSERT_STREQ(split.trigger, "Telemetry");
    ASSERT_STREQ(split.modifier, "\0");

    err = splitTriggerString(NULL, &split);
    ASSERT_TRUE(err != 0);
}

TEST(UtilsTestFixture, concatStrings)
{
    const char* modifier = "Crashlog";
    const char* partTwo = "Data";
    char resultBuffer[MAX_TRIGGER_STR_LEN];

    concatStrings(resultBuffer, sizeof(resultBuffer), modifier, partTwo);

    cJSON* root = cJSON_Parse(R"({"CrashlogData": true }")");
    cJSON* crashlogDataItem = cJSON_GetObjectItem(root, resultBuffer);
    ASSERT_TRUE(cJSON_IsTrue(crashlogDataItem));

    root = cJSON_Parse(R"({"CrashlogData": false }")");
    crashlogDataItem = cJSON_GetObjectItem(root, resultBuffer);
    ASSERT_FALSE(cJSON_IsTrue(crashlogDataItem));

    root = cJSON_Parse(R"("{}")");
    crashlogDataItem = cJSON_GetObjectItem(root, resultBuffer);
    ASSERT_FALSE(cJSON_IsTrue(crashlogDataItem));

    cJSON_Delete(root);
}

TEST(UtilsTestFixture, isCrashlogFlow)
{
    bool err;

    // valid crashlog flow cases
    err = isCrashlogFlow("IERR.Crashlog");
    ASSERT_TRUE(err == true);
    err = isCrashlogFlow("MCERR.Crashlog");
    ASSERT_TRUE(err == true);
    err = isCrashlogFlow("ERR2.Crashlog");
    ASSERT_TRUE(err == true);
    err = isCrashlogFlow("OnDemand.Crashlog");
    ASSERT_TRUE(err == true);

    // invalid crashlog flow cases
    err = isCrashlogFlow("IERR");
    ASSERT_TRUE(err == false);
    err = isCrashlogFlow("MCERR");
    ASSERT_TRUE(err == false);
    err = isCrashlogFlow("ERR2");
    ASSERT_TRUE(err == false);
    err = isCrashlogFlow("OnDemand");
    ASSERT_TRUE(err == false);
    err = isCrashlogFlow("Telemetry");
    ASSERT_TRUE(err == false);
}

TEST(UtilsTestFixture, isPostResetFlow)
{
    bool val;
    val = isPostResetFlow("IERR.PostReset");
    ASSERT_TRUE(val == true);
    val = isPostResetFlow("ERR2.PostReset");
    ASSERT_TRUE(val == true);
    val = isPostResetFlow("abc");
    ASSERT_TRUE(val == false);
    val = isPostResetFlow("IERR.PostReset.Extra");
    ASSERT_TRUE(val == true);
    val = isPostResetFlow("IERR.Post");
    ASSERT_TRUE(val == false);
    val = isPostResetFlow("ERR2.PostReset.Extra");
    ASSERT_TRUE(val == true);
    val = isPostResetFlow("ERR2.Post");
    ASSERT_TRUE(val == false);
    val = isPostResetFlow(NULL);
    ASSERT_TRUE(val == false);
}

TEST(UtilsTestFixture, isErrorFlow)
{
    bool val;
    // Ensure we catch PostReset before errors
    val = isErrorFlow("IERR.PostReset");
    ASSERT_TRUE(val == false);
    val = isErrorFlow("ERR2.PostReset");
    ASSERT_TRUE(val == false);

    val = isErrorFlow("abc");
    ASSERT_TRUE(val == false);
    val = isErrorFlow("IERR.PostReset.Extra");
    ASSERT_TRUE(val == false);
    val = isErrorFlow("IERR.Post");
    ASSERT_TRUE(val == true);
    val = isErrorFlow("ERR2.Post.Extra");
    ASSERT_TRUE(val == true);
    val = isErrorFlow("ERR2");
    ASSERT_TRUE(val == true);
    val = isErrorFlow(NULL);
    ASSERT_TRUE(val == false);
}

// TODO: fix this test
TEST(UtilsTestFixture, DISABLED_getTriggerType)
{
    STriggerType trig;
    // Ensure we catch PostReset before errors
    trig = getTriggerType("IERR.PostReset");
    ASSERT_TRUE(trig == TRIGGER_POST_RESET);
    trig = getTriggerType("ERR2.PostReset");
    ASSERT_TRUE(trig == TRIGGER_POST_RESET);

    trig = getTriggerType("abc");
    ASSERT_TRUE(trig == TRIGGER_ONDEMAND_TELEMETRY);
    trig = getTriggerType("IERR.PostReset.Extra");
    ASSERT_TRUE(trig == TRIGGER_POST_RESET);
    trig = getTriggerType("IERR.Post");
    ASSERT_TRUE(trig == TRIGGER_IERR);
    trig = getTriggerType("ERR2.Post.Extra");
    ASSERT_TRUE(trig == TRIGGER_ERR2);
    trig = getTriggerType("ERR2");
    ASSERT_TRUE(trig == TRIGGER_ERR2);
    trig = getTriggerType(NULL);
    ASSERT_TRUE(trig == TRIGGER_UNKNOWN);
}

TEST(UtilsTestFixture, GNR_getFlagFromSection_LoopOnCompute_test)
{
    cJSON* content = readInputFile("../crashdump_input_gnr.json");
    char* flagStr = "LoopOnCompute";
    if (content == NULL)
    {
        ASSERT_TRUE(false) << "crashdump_input_gnr.json not found";
    }

    bool val;
    val = getFlagFromSection(content, 1, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 2, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 3, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 4, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 5, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 6, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 7, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 8, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 9, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 10, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 11, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 12, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 13, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 14, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 15, flagStr);
    ASSERT_TRUE(val == true);
    val = getFlagFromSection(content, 16, flagStr);
    ASSERT_TRUE(val == true);

    // Delete LoopOnCompute from first section and val should return true
    cJSON* obj = getJsonObjectFromPath("Sections", content);
    cJSON* aSection = cJSON_GetArrayItem(obj, 0);
    cJSON_DeleteItemFromObject(aSection->child, flagStr);
    val = getFlagFromSection(content, 0, flagStr);
    ASSERT_TRUE(val == true);

    aSection = cJSON_GetArrayItem(obj, 2);
    cJSON* cjsonFlag = cJSON_Parse(R"({"LoopOnCompute": false})");
    cJSON_ReplaceItemInObject(aSection->child, flagStr, cjsonFlag);
    val = getFlagFromSection(content, 2, flagStr);
    ASSERT_TRUE(val == false);
}

TEST(UtilsTestFixture, getPciRegister_invalid_index_test)
{
    uint8_t invalid_index_list[] = {sizeof(sPciReg) / sizeof(sPciReg[0]),
                                    sizeof(sPciReg) / sizeof(sPciReg[0]) + 1};
    int ret = ACD_FAILURE;

    for (uint8_t i = 0;
         i < sizeof(invalid_index_list) / sizeof(invalid_index_list[0]); i++)
    {
        ret = getPciRegister(NULL, NULL, invalid_index_list[i], 0);
        ASSERT_TRUE(ret == ACD_FAILURE);
    }
}

TEST(UtilsTestFixture, compress_output_test)
{
    // Make a long 400 char string into a file
    std::string originalString(400, 'a');

    // Write originalString to a file
    char* originalFilePath = "/tmp/original.txt";
    std::ofstream originalFile(originalFilePath);
    if (originalFile.is_open())
    {
        originalFile << originalString;
        originalFile.close();
    }
    else
    {
        std::cout << "Unable to open file for writing";
        return;
    }

    // Compress
    compressOptions options;
    options.keepOriginal = false;
    options.inputFilePath = originalFilePath;
    CompressFile(options);

    // Read the compressed file into a vector of bytes
    std::ifstream fileIn("/tmp/original.txt.gz", std::ios::binary);
    std::vector<uint8_t> compressedData(
        (std::istreambuf_iterator<char>(fileIn)),
        std::istreambuf_iterator<char>());

    // Decompress
    uint8_t* decompressedData =
        DecompressString(compressedData.data(), compressedData.size());

    // Convert decompressed data to string for comparison
    std::string decompressedString(reinterpret_cast<char*>(decompressedData));

    // make sure we can read data and no errors.
    ASSERT_EQ(originalString, decompressedString);
}

TEST(UtilsTestFixture, getAvailableMemory)
{
    SystemUsageInfo mem = getCurrentAvailableMemory();
    EXPECT_NE(mem.memoryAvailable, 0);
    if (mem.memoryAvailable > 0)
    {
        CRASHDUMP_PRINT(INFO, stderr, "MemAvailable: %dKB\n", mem);
    }
}

TEST(UtilsTestFixture, getUsageMemory)
{
    AppUsageInfo mem = getCurrentAppMemoryUsage();
    EXPECT_NE(mem.memoryPeak, 0);
    EXPECT_NE(mem.memoryUsage, 0);

    if (mem.memoryPeak > 0)
    {
        CRASHDUMP_PRINT(INFO, stderr, "MemPeak: %dKB\n", mem.memoryPeak);
    }
    if (mem.memoryUsage > 0)
    {
        CRASHDUMP_PRINT(INFO, stderr, "MemSize: %dKB\n", mem.memoryUsage);
    }
}
