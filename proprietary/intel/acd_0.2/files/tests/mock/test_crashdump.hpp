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

    TestCrashdump(Model model);

    TestCrashdump(Model model, int delay);

    TestCrashdump(Model model, int delay, int numberOfCpus);

    TestCrashdump(std::vector<CPUInfo>& cpuInfo_);

    ~TestCrashdump();

    void initializeModelMap();

    void removeInputFiles();

    void setupInputFiles();

    void copyInputFilesToDefaultLocation(std::filesystem::path sourceFile);

    std::map<Model, std::string> cpuModelMap;
    std::filesystem::path targetPath = "/tmp/crashdump/input";
    std::vector<CPUInfo> cpusInfo = {};
    cJSON* root = NULL;
    char* jsonStr = NULL;
    InputFileInfo inputFileInfo = {
        .unique = true, .filenames = {NULL}, .buffers = {NULL}};

    static std::unique_ptr<LibPeciMock> libPeciMock;
};
