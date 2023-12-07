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

#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libpeci_mock.hpp"
#pragma GCC diagnostic pop

extern "C" {
#include <peci.h>

#include "engine/crashdump.h"
}

#include "crashdump.hpp"

#include <filesystem>

class TestCrashdump
{
  public:
    TestCrashdump() = delete;

    explicit TestCrashdump(Model model);

    TestCrashdump(Model model, int delay);

    TestCrashdump(Model model, std::string section);

    TestCrashdump(Model model, int delay, int numberOfCpus);

    explicit TestCrashdump(CPUInfo* cpus);

    ~TestCrashdump();

    void initializeModelMap();

    void removeInputFiles();

    void setupInputFiles();

    void copyInputFilesToDefaultLocation(std::filesystem::path sourceFile);

    void setupInputFilesBySection(std::string section);

    void copyInputFilesToDefaultLocationBySection(
        std::filesystem::path sourceFile, std::string originalFileName);

    void disableAllSections(std::string section,
                            std::filesystem::path sourceFile);

    std::map<Model, std::string> cpuModelMap;
#ifdef COMPILE_UNIT_TESTS
#define TARGET_PATH "/tmp/crashdump" TMP_DIR_POSTFIX "/input"
    std::filesystem::path targetPath = TARGET_PATH;
#else
    std::filesystem::path targetPath = "/tmp/crashdump/input";
#endif
    CPUInfo cpus[MAX_CPUS];
    cJSON* root = NULL;
    char* jsonStr = NULL;
    InputFileInfo inputFileInfo = {
        .unique = true, .filenames = {NULL}, .buffers = {NULL}};

    static std::unique_ptr<LibPeciMock> libPeciMock;
};
