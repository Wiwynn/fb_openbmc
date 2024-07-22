/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2019 Intel Corporation.
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

#ifndef UTILS_H
#define UTILS_H

#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>

#include "crashdump.h"

#define DEFAULT_INPUT_DIR "/usr/share/crashdump/input/"
#define OVERRIDE_INPUT_DIR "/tmp/crashdump/input/"
#define DEFAULT_INPUT_FILENAME_FMT "crashdump_input_%s.json"
#define DEFAULT_TELEMETRY_FILENAME_FMT "telemetry_input_%s.json"

#define PECI_CC_SKIP_CORE(cc)                                                  \
    (((cc) == PECI_DEV_CC_CATASTROPHIC_MCA_ERROR ||                            \
      (cc) == PECI_DEV_CC_NEED_RETRY || (cc) == PECI_DEV_CC_OUT_OF_RESOURCE || \
      (cc) == PECI_DEV_CC_UNAVAIL_RESOURCE ||                                  \
      (cc) == PECI_DEV_CC_INVALID_REQ || (cc) == PECI_DEV_CC_MCA_ERROR)        \
         ? true                                                                \
         : false)

#define PECI_CC_SKIP_SOCKET(cc)                                                \
    (((cc) == PECI_DEV_CC_PARITY_ERROR_ON_GPSB_OR_PMSB ||                      \
      (cc) == PECI_DEV_CC_PARITY_ERROR_ON_GPSB_OR_PMSB_IERR ||                 \
      (cc) == PECI_DEV_CC_PARITY_ERROR_ON_GPSB_OR_PMSB_MCA)                    \
         ? true                                                                \
         : false)

#define PECI_CC_UA(cc)                                                         \
    (((cc) != PECI_DEV_CC_SUCCESS && (cc) != PECI_DEV_CC_FATAL_MCA_DETECTED)   \
         ? true                                                                \
         : false)

#define FREE(ptr)                                                              \
    do                                                                         \
    {                                                                          \
        free(ptr);                                                             \
        ptr = NULL;                                                            \
    } while (0)

#define SET_BIT(val, pos) ((val) |= ((uint64_t)1 << ((uint64_t)pos)))
#define CLEAR_BIT(val, pos) ((val) &= ~((uint64_t)1 << ((uint64_t)pos)))
#define CHECK_BIT(val, pos) ((val) & ((uint64_t)1 << ((uint64_t)pos)))
#define CHECK_RANGE(val, min, max) ((val >= min) && (val <= max))
#define CPU_STR_LEN 7
#define NAME_STR_LEN 255
#define DEFAULT_VALUE -1
#define CD_SNPRINTF_S_INVALID 0
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX_SOCKETS_FOR_BAFI 2

void fillInputFile(CPUInfo* cpuInfo, PlatformInfo* platformInfo);
int logResetDetected(cJSON* metadata, PlatformState* platformState);
int fillCrashdumpVersion(char* cSectionName, cJSON* pJsonChild);

// crashdump_input_xxx.json #define(s) and c-helper functions
#define RECORD_ENABLE "_record_enable"
#define RECORD_ENABLE_SECTIONS "RecordEnable"
#define CD_ENABLE 0
#define UT_REG_NAME_LEN 32
#define UT_REG_DWORD 4
#define UT_REG_QWORD 8
#define SI_JSON_STRING_LEN 48
#define SI_JSON_SOCKET_NAME "cpu%d"
#define MD_NA "N/A"
#define RESET_DETECTED_DEFAULT "NONE"
#define INPUT_FILE_ERROR_STR "Error while reading input file"
#define SI_BMC_VER_LEN 64
#define SI_BIOS_ID_LEN 64
#define SUMMARY_ENABLE_IN_FLAG "SummaryEnable"
#define TRIAGE_ENABLE_IN_FLAG "TriageEnable"

typedef enum
{
    EXECUTION_ALL_REGISTERS,
    EXECUTION_TILL_ABORT,
    EXECUTION_ABORTED
} executionStatus;

typedef enum
{
    FLAG_ENABLE,
    FLAG_DISABLE,
    FLAG_NOT_PRESENT
} inputField;

typedef union
{
    uint64_t u64;
    uint32_t u32[2];
} SRegValue;

typedef struct
{
    SRegValue uValue;
    uint8_t cc;
    int ret;
} SRegRawData;

typedef struct
{
    char regName[UT_REG_NAME_LEN];
    uint8_t u8Seg;
    uint8_t u8Bus;
    uint8_t u8Dev;
    uint8_t u8Func;
    uint16_t u16Reg;
    uint8_t u8Size;
} SRegPci;

typedef struct
{
    char trigger[MAX_TRIGGER_STR_LEN];
    char modifier[MAX_TRIGGER_STR_LEN];
} TriggerString;

typedef enum
{
    TRIGGER_NODECODE,
    TRIGGER_IERR,
    TRIGGER_ERR2,
    TRIGGER_POST_RESET,
    TRIGGER_ONDEMAND_TELEMETRY,
    TRIGGER_UNKNOWN,
} STriggerType;

typedef enum
{
    MCA_ERR_SRC_LOG_SPR = 0,
    MCA_ERR_SRC_LOG_GNR,
} SRegPciIndex;

static const SRegPci sPciReg[] = {
    // Register, Seg, Bus, Dev, Func, Offset, Size
    // SPR mca_error_source_log register
    {"mca_err_src_log", 0, 31, 30, 2, 0xEC, 4},
    // GNR io0 global mca_error_source_log register
    {"mca_err_src_log", 0, 30, 5, 0, 0x4F0, 4},
};

typedef struct
{
    bool keepOriginal;
    char* outputFilePath;
    char* inputFilePath;
} compressOptions;

extern struct timespec crashdumpStart;

cJSON* getNewCrashDataSection(cJSON* root, char* section);
cJSON* getNewCrashDataSectionObjectOneLevel(cJSON* root, char* section,
                                            const char* firstLevel);
cJSON* getCommandFromSection(cJSON* root, const char* section,
                             const char* peciCommand);
bool readInputFileFlag(cJSON* pJsonChild, bool defaultValue, char* sectionName);
int cd_snprintf_s(char* str, size_t len, const char* format, ...);
cJSON* getCrashDataSectionCoreRegList(cJSON* root, char* version,
                                      uint32_t crashdumpVersionSize,
                                      CoreType coreType);
bool isCoreRegVersionMatch(cJSON* root, uint32_t version, CoreType coreType);
uint32_t getCrashDataSectionCoreSize(cJSON* root, char* version,
                                     uint32_t crashdumpVersionSize,
                                     CoreType coreType);
cJSON* getCrashDataSectionCoreSizeArray(cJSON* root, char* version,
                                        uint32_t crashdumpVersionSize,
                                        CoreType coreType);
void storeCrashDataSectionCoreSize(cJSON* root, char* version,
                                   uint32_t totalSize,
                                   uint32_t crashdumpVersionSize,
                                   CoreType coreType);
bool isValueInJsonArray(cJSON* jsonArray, uint32_t value);
uint64_t tsToNanosecond(struct timespec* ts);
struct timespec calculateTimeRemaining(uint32_t maxWaitTimeFromInputFileInSec);
uint32_t getDelayFromInputFile(CPUInfo* cpuInfo, char* sectionName);
int getPciRegister(CPUInfo* cpuInfo, SRegRawData* sRegData, uint8_t u8index,
                   uint8_t domainID);
bool getSkipFromNewInputFile(CPUInfo* cpuInfo, char* sectionName);
inputField getFlagValueFromInputFile(CPUInfo* cpuInfo, char* sectionName,
                                     char* flagName);
uint32_t getCollectionTimeFromInputFile(CPUInfo* cpuInfo);

executionStatus checkMaxTimeElapsed(uint32_t maxTime,
                                    struct timespec sectionStartTime);
double getTimeRemainingFromStart(uint32_t maxTime,
                                 struct timespec sectionStartTime);
uint8_t getNumberOfSections(CPUInfo* cpuInfo);
int cJSONToInt(cJSON* cjsonObj, int base);
bool getFlagFromSection(cJSON* inputFileBuf, int sectionIdx, char* flagName);
cJSON* getJsonFlagFromSection(cJSON* inputFileBuf, int sectionIdx,
                              char* flagName);
int clampInt(int val, int lBound, int uBound);
void getRangeValues(cJSON* section, const char* rangeName, int* low, int* high);
bool isCPUGNRD(CPUInfo* cpuInfo);
bool isSectionNotBigOrAtomCore(cJSON* recordType);
cJSON* getFirstBigOrAtomCoreFromSocket(CPUInfo* cpuInfo, cJSON* parent);

/* NVD Related functions */
cJSON* getNVDSection(cJSON* root, const char* const section,
                     bool* const enable);
cJSON* getNVDSectionRegList(cJSON* root, const char* const section,
                            bool* const enable);
cJSON* getPMEMSectionErrLogList(cJSON* root, const char* const section,
                                bool* const enable);
uint8_t getNumberOfCpus(CPUInfo* cpus);
acdStatus fillDimmMasks(CPUInfo* cpuInfo);
char* getSectionName(cJSON* inputFile, int sectionIndex);
cJSON* getSectionObj(cJSON* inputFile, int sectionIndex);

/* Crashlog Related functions */
cJSON* getCrashlogAgentsFromInputFile(cJSON* root);
uint8_t getMaxCollectionCoresFromInputFile(CPUInfo* cpuInfo);

/* SPRHBM Related functions */
bool isSprHbm(const CPUInfo* cpuInfo);

/* Unified flow helper functions */
bool isPostResetFlow(const char* triggerType);
bool isErrorFlow(const char* triggerType);
STriggerType getTriggerType(const char* triggerType);
bool isCrashDumpFail(int sectionFailCount, const char* triggerType);

bool isCrashlogFlow(const char* triggerType);
int splitTriggerString(const char* input, TriggerString* result);
void concatStrings(char* buffer, size_t bufferSize, const char* str1,
                   const char* str2);

/* File helper functions */
cJSON* readInputFile(const char* filename);
cJSON* selectAndReadInputFile(Model cpuModel, char** filename, bool isTelemetry,
                              char* defaultPath, char* overridePath);

void createOutputFile(const char* filename, const char* outputContents);
void createCrashdumpOutputFileWithTimeStamp(const char* outputContents);

/* BAFI*/
bool isBAFIEnabled(cJSON* inputFile, bool isTelemetry);
bool isTriageEnabled(cJSON* inputFile, bool isTelemetry);
uint8_t getMaxNumSocketsForBafi(cJSON* inputFile);

/* Compression helper functions */
acdStatus CompressFile(compressOptions options);

/* System Memory */
typedef struct SystemUsageInfo
{
    uint64_t memoryAvailable;
    bool isSuccess;
} SystemUsageInfo;

typedef struct AppUsageInfo
{
    uint64_t memoryPeak;
    uint64_t memoryUsage;
    bool isSuccess;
} AppUsageInfo;

SystemUsageInfo getCurrentAvailableMemory();
AppUsageInfo getCurrentAppMemoryUsage();

#endif // UTILS_H
