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

#ifndef CRASHDUMP_H
#define CRASHDUMP_H

#include <cjson/cJSON.h>
#include <linux/peci-ioctl.h>
#include <peci.h>
#include <safe_mem_lib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "RemoteModule.h"
#include "consolelog.h"
#include "safe_str_lib.h"

#define CRASHDUMP_VER "BMC_BHS_0.9"
#define JOURNAL_PRINT(stream, ...) fprintf(stream, __VA_ARGS__)
#define CRASHDUMP_PRINT(level, stream, ...)                                    \
    consoleLog(level, stream, __VA_ARGS__)
#define VERBOSE_PRINT(stream,                                                  \
                      ...) // TODO enable levels on consoleLog to enable this
#define CRASHDUMP_VALUE_LEN 6
#define JSON_VAL_LEN 64
#define MAX_PLATFORM_NAME_LEN 64
#define MAX_TIMESTAMP_LEN 64
#define MAX_TRIGGER_STR_LEN 64
#define MAX_DEV_NAME_LEN 64
#define MAX_FILENAME_LEN 255
#define MAX_PATH_LEN 255
#define RESET_DETECTED_NAME "cpu%d.%s"
#define DIE_MAP_ERROR_VALUE 255

#define MAX_CORE_MASK 64
#define MAX_MODULE_MASK 64
#define MAX_CORE_PER_MODULE 4
#define MAX_MODIFIER_LEN 32

#define SPR_MODEL 0x000806F0
#define EMR_MODEL 0x000C06F0
#define GNR_MODEL 0xA06D0
#define GNRD_MODEL 0xA06E0
#define SRF_MODEL 0xA06F0
#define SPR_HBM_PLATFORM_ID 0x4
#define STEPPING_SPR 0

#ifdef USE_TWO_BY_ONE_S_CONFIGURATION
#define DEVICE_FORMAT "/dev/peci-%d"
#endif

/* GNR related #defines */
/*
    3 Computes and 2 IOs is the max we will see for GNR;
    Compute is bits [31:9]
    IO is bits [8:0]
*/
#define GNR_MAX_DIEMASK 0xE11
#define GNR_MAX_ACTIVE_DIES 3
// Max count of phy2log table indices is (max cores / (4 cores per command))
#define GNR_PHY2LOG_MAX_INDICES MAX_CORE_MASK / 4
/* GNR #define end */

/* SRF related #defines */
#define SRF_MAX_DIEMASK 0x611
/* SRF #define end */

#define RECORD_TYPE_OFFSET 24
#define RECORD_TYPE_CORECRASHLOG 0x04
#define RECORD_TYPE_ATOM_CORECRASHLOG 0x06
#define RECORD_TYPE_UNCORESTATUSLOG 0x08
#define RECORD_TYPE_TORDUMP 0x09
#define RECORD_TYPE_METADATA 0x0B
#define RECORD_TYPE_PMINFO 0x0C
#define RECORD_TYPE_ADDRESSMAP 0x0D
#define RECORD_TYPE_BMCAUTONOMOUS 0x23
#define RECORD_TYPE_MCALOG 0x3E
#ifdef OEMDATA_SECTION
#define RECORD_TYPE_OEM 0x99
#endif

#define PRODUCT_TYPE_OFFSET 12
#define PRODUCT_TYPE_BMCAUTONOMOUS 0x23
#define PRODUCT_TYPE_SPR 0x1C
#define PRODUCT_TYPE_EMR 0x79
#define PRODUCT_TYPE_GNR_SP 0x2F
#define PRODUCT_TYPE_GNR_D 0x84
#define PRODUCT_TYPE_SRF_SP 0x82

#define REVISION_OFFSET 0
#define REVISION_1 0x01

#define BIG_CORE_SECTION_NAME "BigCore"
#define ATOM_CORE_SECTION_NAME "AtomCore"

/* Post Reset Flow Strings */
#define POST_RESET_TRIGGER "PostReset"
#define IERR_TRIGGER "IERR"
#define ERR2_TRIGGER "ERR2"

/* Wake On PECI */
#define DONT_CARE 0

typedef enum
{
    EMERG,
    ALERT,
    CRIT,
    ERR,
    WARNING,
    NOTICE,
    INFO,
    DEBUG,
} severity;

typedef enum
{
    // General status
    ACD_SUCCESS,
    ACD_FAILURE,
    ACD_INVALID_OBJECT,
    ACD_ALLOCATE_FAILURE,
    ACD_SECTION_DISABLE,
    ACD_INPUT_FILE_ERROR,
    ACD_FAILURE_CMD_TABLE,
    ACD_FAILURE_UPDATE_PARAMS,
    ACD_INVALID_CMD,

    // createCrashdump() error related statuses
    ACD_PING_ERROR,

    // command parameters check status
    ACD_INVALID_CRASHDUMP_DISCOVERY_PARAMS,
    ACD_INVALID_CRASHDUMP_DISCOVERY_DOM_PARAMS,
    ACD_INVALID_CRASHDUMP_GETFRAME_PARAMS,
    ACD_INVALID_CRASHDUMP_GETFRAME_DOM_PARAMS,
    ACD_INVALID_GETCPUID_PARAMS,
    ACD_INVALID_PING_PARAMS,
    ACD_INVALID_RDENDPOINTCONFIGMMIO_PARAMS,
    ACD_INVALID_RDENDPOINTCONFIGMMIO_DOM_PARAMS,
    ACD_INVALID_RDPCICONFIGLOCAL_PARAMS,
    ACD_INVALID_RDPCICONFIGLOCAL_DOM_PARAMS,
    ACD_INVALID_RDENDPOINTCONFIGPCILOCAL_PARAMS,
    ACD_INVALID_RDENDPOINTCONFIGPCILOCAL_DOM_PARAMS,
    ACD_INVALID_WRENDPOINTCONFIGPCILOCAL_PARAMS,
    ACD_INVALID_WRENDPOINTCONFIGPCILOCAL_DOM_PARAMS,
    ACD_INVALID_RDIAMSR_PARAMS,
    ACD_INVALID_RDIAMSR_DOM_PARAMS,
    ACD_INVALID_RDPKGCONFIG_PARAMS,
    ACD_INVALID_RDPKGCONFIG_DOM_PARAMS,
    ACD_INVALID_RDPKGCONFIGCORE_PARAMS,
    ACD_INVALID_RDPKGCONFIGCORE_DOM_PARAMS,
    ACD_INVALID_RDPOSTENUMBUS_PARAMS,
    ACD_INVALID_RDCHACOUNT_PARAMS,
    ACD_INVALID_TELEMETRY_DISCOVERY_PARAMS,
    ACD_INVALID_TELEMETRY_DISCOVERY_DOM_PARAMS,
    ACD_INVALID_RD_CONCATENATE_PARAMS,
    ACD_INVALID_GLOBAL_VARS_PARAMS,
    ACD_INVALID_SAVE_STR_VARS_PARAMS,

    // Timeout related status
    ACD_INVALID_RDPHY2LOG_PARAMS,
    ACD_GLOBAL_TIMEOUT,
    ACD_SECTION_TIMEOUT,

    // DEVICE NAME
    ACD_DEVICE_NAME_ERROR,

    // Remote Modules
    ACD_MODULE_NOT_FOUND,
    ACD_MODULE_INVALID_API,
} acdStatus;

typedef enum
{
    STARTUP = 1,
    EVENT = 0,
    INVALID = 2,
    OVERWRITTEN = 3,
} cpuidState;

typedef enum
{
    ON_PC2 = 1,
    ON_PC0 = 2,
    OFF = 0,
    UNKNOWN = -1,
} pwState;

typedef enum
{
    MCA_UNCORE,
    UNCORE,
    TOR,
    PM_INFO,
    ADDRESS_MAP,
#ifdef OEMDATA_SECTION
    OEM,
#endif
    BIG_CORE,
    MCA_CORE,
    METADATA,
    CRASHLOG,
    NUMBER_OF_SECTIONS,
} Section;

typedef enum
{
    ACD_BIG_CORE,
    ACD_ATOM_CORE,
} CoreType;

typedef enum
{
    cd_icx,
    cd_icx2,
    cd_spr,
    cd_sprhbm,
    cd_icxd,
    cd_gnr,
    cd_gnrd,
    cd_emr,
    cd_srf,
    cd_numberOfModels,
} Model;

typedef struct
{
    char* filenamePtr;
    cJSON* bufferPtr;
} JSONInfo;

typedef struct
{
    uint8_t coreMaskCc;
    int coreMaskRet;
    bool coreMaskValid;
    cpuidState source;
} COREMaskRead;

typedef struct
{
    uint8_t chaCountCc;
    int chaCountRet;
    bool chaCountValid;
    cpuidState source;
} CHACountRead;

typedef struct
{
    uint8_t cpuidCc;
    int cpuidRet;
    CPUModel cpuModel;
    uint8_t stepping;
    bool cpuidValid;
    cpuidState source;
} CPUIDRead;

typedef union _DieMaskBits
{
    uint32_t dieMask;
    struct
    {
        uint32_t io : 9;
        uint32_t compute : 23;
    } GNR, SRF;

    struct
    {
        uint32_t io : 9;
        uint32_t compute : 19; // TBD
        uint32_t reserve : 4;  // TBD
    } GNRD;

} DieMaskBits;

typedef struct
{
    uint32_t range;
    uint32_t offset;
    uint32_t effectiveMask;
    uint16_t maxDies;
    pwState initialPeciWake[GNR_MAX_ACTIVE_DIES];
} DieInfo;

typedef struct
{
    uint32_t dieMask;
    bool maskValid;
    bool dieMaskSupported;
    DieInfo compute;
    DieInfo io;
} DieMaskInfo;

typedef struct
{
    uint32_t table[GNR_MAX_ACTIVE_DIES][GNR_PHY2LOG_MAX_INDICES];
    bool isValid;
} PHY2LOGRead;

typedef struct
{
    uint64_t coreMask;
    uint64_t chaMask;
    CHACountRead chaCountRead;
    COREMaskRead coreMaskRead;
    uint64_t crashedCoreMask;
    size_t chaCount;
} ComputeDieInfo;

typedef struct
{
    bool resetDetected;
    int resetCpu;
    char resetSectionName[JSON_VAL_LEN];
} PlatformState;

typedef struct
{
    char* bmc_fw_ver;
    char* bios_id;
    char* time_stamp;
    char* trigger_type;
    char* platform_name;
    char* crashdump_ver;
    char* inputfile_ver;
    char* input_file;
    bool loopOverOneCpu;
    bool loopOverOneDie;
    bool loopOverOneIO;
    bool noDomainLoop;
} PlatformInfo;

typedef struct
{
    int clientAddr;
    Model model;
    DieMaskInfo dieMaskInfo;
    ComputeDieInfo* computeDies;
    uint64_t coreMask;
    uint64_t crashedCoreMask;
    size_t chaCount;
    pwState initialPeciWake;
    JSONInfo inputFile;
    CPUIDRead cpuidRead;
    CHACountRead chaCountRead;
    COREMaskRead coreMaskRead;
    PHY2LOGRead phy2logReadCore;
    PHY2LOGRead phy2logReadCHA;
    uint16_t dimmMask;
    uint32_t platformID;
    PlatformState platformState;
    uint32_t ucodePatch;
    CoreType coreType;
    uint64_t crashdumpDiscoveryMask;
    char devName[MAX_DEV_NAME_LEN];
} CPUInfo;

typedef struct
{
    char* name;
    Section section;
    int record_type;
    int (*fptr)(CPUInfo* cpuInfo, cJSON* pJsonChild);
} CrashdumpSection;

typedef struct
{
    bool unique;
    char* filenames[cd_numberOfModels];
    cJSON* buffers[cd_numberOfModels];
    char defaultPath[MAX_PATH_LEN];
    char overridePath[MAX_PATH_LEN];
} InputFileInfo;

typedef struct
{
    Model cpuModel;
    int data;
} VersionInfo;

typedef struct
{
    struct timespec globalRunTime;
    struct timespec sectionRunTime;
    uint32_t maxGlobalTime;
    uint32_t maxSectionTime;
    bool maxSectionRunTimeReached;
    bool maxGlobalRunTimeReached;
    bool globalMaxPrint;
    bool sectionMaxPrint;
} RunTimeInfo;

extern const DieMaskBits DieMaskRange;
extern const CrashdumpSection sectionNames[NUMBER_OF_SECTIONS];
extern int revision_uncore;
extern bool commonMetaDataEnabled;

/* GNR Related functions */
uint8_t UpdateGnrPcuDeviceNum(DieMaskInfo* dieMaskInfo, uint8_t pos);
uint8_t mapDieNumToBit(uint32_t dieMask, uint32_t posToMap, uint32_t rangeMask);
acdStatus parseExplicitDomain(char* domainName, char* inputString,
                              uint32_t* valuePtr);
bool isDomainCompute(DieMaskInfo* dieMaskInfo, uint32_t domainID);

/* CPU Init and Discovery Related Functions */
acdStatus initCPUInfo(CPUInfo* cpus);
void initMaxCPUs(CPUInfo* cpus);
acdStatus getCPUData(CPUInfo* cpus, cpuidState cpuState);
void overwriteCPUInfo(CPUInfo* cpus);
int isCpusInfoEmpty(CPUInfo* cpus);
uint8_t getClientAddrs(CPUInfo* cpus);
void initComputeDieStructure(DieMaskInfo* dieMaskInfo,
                             ComputeDieInfo** computeDies);
void initDieReadStructure(DieMaskInfo* dieMaskInfo, uint32_t computeRange,
                          uint32_t ioRange);
bool getCHACounts(CPUInfo* cpus, cpuidState cpuState);
bool getCoreMasks(CPUInfo* cpus, cpuidState cpuState);
void getDieMasks(CPUInfo* cpus, cpuidState cpuState);
void getPlatformIDs(CPUInfo* cpus);
void getUcodePatchVers(CPUInfo* cpus);
uint32_t applyComputeMask(CPUInfo* cpu, uint32_t dieMask);
uint32_t applyIOMask(CPUInfo* cpu, uint32_t dieMask);

/* Memory Clean Up Related Functions */
void cleanupComputeDies(CPUInfo* cpus);

/* PECI Wake Related Functions */
int savePeciWake(CPUInfo* cpus);
int setPeciWake(CPUInfo* cpus, pwState desiredState);
void setPeciWakeInitial(CPUInfo* cpus);
int checkPeciWake(CPUInfo* cpus);
pwState savePeciWakeOfDomain(int clientAddr, uint32_t domainId);
void checkPeciWakeOfDomain(int clientAddr, uint32_t domainID);
void setPeciWakeOfDomain(int clientAddr, uint32_t domainId, int writeValue);

/* Input File Related Functions */
acdStatus loadInputFiles(CPUInfo* cpus, InputFileInfo* inputFileInfo,
                         int isTelemetry);
void logInputFileVersion(CPUInfo* cpuInfo, PlatformInfo* platformInfo);
void cleanupInputFiles(CPUInfo* cpus, InputFileInfo* inputFileInfo);

/* Time Related Functions */
void newTimestamp(char* logTime, size_t logTimeSize);

/* cJSON Related Functions */
char* printCJSON(cJSON* root);

/* Platform State Related Functions */
void setResetDetected(PlatformState* platformState);
void clearResetDetected(PlatformState* platformState);
void updateResetInfo(PlatformState* platformState, uint8_t cpu,
                     char* sectionName);

/* Filling crashdumpContents related functions */
void fillMetaDataCommon(PlatformInfo* platformInfo, CPUInfo* cpuInfo,
                        char* platformName, char* triggerType, char* timestamp,
                        char* biosVersion, char* bmcVersion);
acdStatus createCrashdump(CPUInfo* cpus, char** crashdumpContents,
                          char* triggerType, char* timestamp, bool isTelemetry,
                          int* sectionFailCount, InputFileInfo* inputFileInfo,
                          cJSON** root);
void logCrashdumpVersion(cJSON* parent, CPUInfo* cpuInfo, int recordType);

/* Section Memory Checks*/
#define MEM_CHECK_INPUT_KEY "MemRequiredMB"
#define MEM_CHECK_FINAL_EXPANSION_RATIO_KEY "FinalSaveExpansionRatio"
#define MEM_CHECK_FINAL_EXPANSION_RATIO_DEFAULT_VALUE 1.5
#define SKIPPED_SECTION_KEY_FMT "_skipped_section_%s"
#define SKIPPED_SECTION_VALUE "low memory"
bool skipSectionOnLowMemory(uint32_t sectionMaxMemMB, uint32_t numOfCpus,
                            float expansionRatio);
bool checkAvailableMemory(CPUInfo* cpus, cJSON** root, int32_t section);

/* Standalone related functions*/
typedef struct
{
    char triggerType[MAX_TRIGGER_STR_LEN];
    char timestamp[MAX_TIMESTAMP_LEN];
    char* outputContents;
    cJSON* root;
    InputFileInfo inputFileInfo;
    int sectionFailCount;
    bool isTelemetry;
} CrashdumpContext;

acdStatus initCPUs(CPUInfo* cpus, CrashdumpContext* context);
acdStatus createCrashdumpContents(CPUInfo* cpus, CrashdumpContext* context);
acdStatus setPECIAddressAndDeviceName(CPUInfo* cpu, int addr, char* devName);
acdStatus setInputFileLocation(CPUInfo* cpu, char* filename);
acdStatus deinitCPUsAndContext(CPUInfo* cpus, CrashdumpContext* context);

#endif // CRASHDUMP_H
