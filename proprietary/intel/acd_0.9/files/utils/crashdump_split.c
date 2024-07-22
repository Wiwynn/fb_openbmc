#include <cjson/cJSON.h>
#include <dirent.h>
#include <fnmatch.h>
#include <getopt.h>
#include <libsafec/safe_mem_lib.h>
#include <libsafec/safe_str_lib.h>
#include <limits.h>
#include <math.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const unsigned int _max_cpu_count = 4096;
#define MAX_FILES_TO_MERGE 1024
#define REG_FOUND_MERGE_PATTERN 4
#define DEFAULT_NUMFILES 4
#define DEFAULT_NUMCPUS 8
#define MAX_BASENAME (PATH_MAX - 32)
#define MAX_CPU_IDX_KEY 16

int cd_snprintf_s(char* str, size_t len, const char* format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(str, len, format, args);
    va_end(args);
    if (ret < 0)
    {
        printf("cd_snprintf_s: Error writing formatted data to string %d\n",
               ret);
    }
    return ret;
}

void print_help()
{
    printf(
        "Name: crashdump-split\n\n"
        "Description: Split (or Merge) crashdump files\n\n"
        "Usage Options:\n"
        "  crashdump-split [--help] [--merge basename] [--numfiles numfiles] "
        "[--numcpus expectnumcpus] [filename]\n\n"
        "  --help                    Shows this information, also printed "
        "when no parameters are used.\n"
        "  --merge basename          Run in merge mode using basename. "
        "Merge files with basename*.json\n"
        "  --numfiles numfiles       Split mode only: split the file into "
        "numfiles with expectnumcpus/numfiles CPUs per file (Default: 4 "
        "files).\n"
        "  --numcpus expectnumcpus   Split mode only: Expect that there are "
        "expectnumcpus in the input file. (Default: 8 cpus)\n"
        "  --remove                  Remove input files after processing.\n"
        "  filename                  Split mode: split this json filename. "
        "Merge mode: Ignored\n\n"
        "Examples:\n"
        "  Split mode:   crashdump-split --numfiles 4 --numcpus 8 "
        "crashdump_8cpus_041724-1120.json\n"
        "  Split a 8 CPU crashdump file into 4 files, each file "
        "contains 2 CPUs.\n"
        "  Since the numfiles: 4 and numcpus: 8 are default value in split "
        "mode, so you can also use as:\n"
        "  \"crashdump-split crashdump_8cpus_041724-1120.json\"\n\n"
        "  Merge mode:   crashdump-split --merge "
        "crashdump_8cpus_041724-1120\n\n");
}

typedef struct
{
    int help;
    int merge;
    int remove;
    int numfiles;
    int numcpus;
    char* basename;
    char* filename;
} Options;

bool startsWith(const char* str, const char* prefix)
{
    int str_len = strnlen_s(str, PATH_MAX);
    int prefix_len = strnlen_s(prefix, PATH_MAX);
    if (str_len < prefix_len)
    {
        return false;
    }
    return strncmp(str, prefix, prefix_len) == 0;
}

bool endsWith(const char* str, const char* suffix)
{
    int str_len = strnlen_s(str, PATH_MAX);
    int suffix_len = strnlen_s(suffix, PATH_MAX);
    if (str_len < suffix_len)
    {
        return 0;
    }
    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

cJSON* get_json_node_exit_when_failed(cJSON* parent_node, const char* node_name)
{
    cJSON* node = cJSON_GetObjectItemCaseSensitive(parent_node, node_name);
    if (!node)
    {
        fprintf(stderr, "%s not found\n", node_name);
        cJSON_Delete(parent_node);
        exit(EXIT_FAILURE);
    }
    return node;
}

cJSON* parse_json_from_file(const char* filename)
{
    cJSON* result = NULL;
    FILE* file = fopen(filename, "r");
    char* data = NULL;
    if (!file)
    {
        printf("Error: Unable to open file: %s to parse json.\n", filename);
        goto Exit0;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    data = (char*)malloc(length + 1);
    if (!data)
    {
        perror("Failed to allocate memory");
        goto Exit0;
    }
    data[length] = '\0';
    size_t read_count = fread(data, 1, length, file);
    if (read_count != length)
    {
        perror("Failed to read file");
        goto Exit0;
    }
    result = cJSON_Parse(data);
    if (result == NULL)
    {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Pase json error before: %s\n", error_ptr);
        }
        goto Exit0;
    }

Exit0:
    if (file)
    {
        fclose(file);
    }
    if (data)
    {
        free(data);
    }
    return result;
}

Options parse_options(int argc, char** argv)
{
    Options opts = {0, 0, 0, DEFAULT_NUMFILES, DEFAULT_NUMCPUS, NULL, NULL};
    int opt;
    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"merge", required_argument, NULL, 'm'},
        {"remove", no_argument, NULL, 'r'},
        {"numfiles", required_argument, NULL, 'n'},
        {"numcpus", required_argument, NULL, 'c'},
        {NULL, 0, NULL, 0}};

    while ((opt = getopt_long(argc, argv, "hm:rn:c:", long_options, NULL)) !=
           -1)
    {
        switch (opt)
        {
            case 'h':
                opts.help = 1;
                break;
            case 'm':
                opts.merge = 1;
                opts.basename = optarg;
                break;
            case 'r':
                opts.remove = 1;
                break;
            case 'n':
                opts.numfiles = atoi(optarg);
                break;
            case 'c':
                opts.numcpus = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Unexpected option: %c\n", opt);
                print_help();
                exit(EXIT_FAILURE);
        }
    }

    if (argv[optind] != NULL)
    {
        opts.filename = argv[optind];
        if (!endsWith(opts.filename, ".json"))
        {
            fprintf(stderr, "Invalid filename: %s\n", opts.filename);
            print_help();
            exit(EXIT_FAILURE);
        }
    }
    else if (!opts.merge)
    {
        fprintf(stderr, "Filename is required for split mode.\n");
        print_help();
        exit(EXIT_FAILURE);
    }

    return opts;
}

bool is_file_exists(const char* filename)
{
    struct stat buffer;
    return stat(filename, &buffer) == 0;
}

void compare_ppin_exit_for_fail(cJSON* common_meta, cJSON* partial_meta,
                                const char* cpu_idx)
{
    cJSON* common_cpu_idx_ppin =
        cJSON_GetObjectItemCaseSensitive(common_meta, cpu_idx);
    cJSON* partial_cpu_idx_ppin =
        cJSON_GetObjectItemCaseSensitive(partial_meta, cpu_idx);
    if (!common_cpu_idx_ppin || !partial_cpu_idx_ppin)
    {
        printf("Error: %s not found in METADATA, failed to compare ppin\n",
               cpu_idx);
        exit(EXIT_FAILURE);
    }
    cJSON* common_ppin =
        cJSON_GetObjectItemCaseSensitive(common_cpu_idx_ppin, "ppin");
    cJSON* partial_ppin =
        cJSON_GetObjectItemCaseSensitive(partial_cpu_idx_ppin, "ppin");
    if (!common_ppin || !partial_ppin)
    {
        printf("Error: ppin not found in %s, failed to compare ppin\n",
               cpu_idx);
        exit(EXIT_FAILURE);
    }

    if (strncmp(common_ppin->valuestring, partial_ppin->valuestring,
                strnlen(common_ppin->valuestring, 1024)) != 0)
    {
        printf("Error: ppin not match in %s, failed to compare ppin\n",
               cpu_idx);
        exit(EXIT_FAILURE);
    }
    printf("ppin match in %s\n", cpu_idx);
}

void merge_cpu_idx_in_processors(cJSON* merge_processors,
                                 cJSON* partial_cpu_idx)
{
    cJSON* merge_cpu_idx = cJSON_GetObjectItemCaseSensitive(
        merge_processors, partial_cpu_idx->string);
    if (merge_cpu_idx)
    {
        printf("Error: Duplicate CPU index: %s found in merge_processors\n",
               partial_cpu_idx->string);
        exit(EXIT_FAILURE);
    }
    cJSON_AddItemToObject(merge_processors, partial_cpu_idx->string,
                          cJSON_Duplicate(partial_cpu_idx, 1));
}

void write_json_to_file_exit_for_fail(cJSON* root, const char* filename)
{
    char* json_str = cJSON_Print(root);
    if (json_str == NULL)
    {
        fprintf(stderr, "Failed to serialize cJSON object.\n");
        exit(EXIT_FAILURE);
    }
    FILE* file = fopen(filename, "w");
    if (file == NULL)
    {
        fprintf(stderr, "Failed to open file %s for writing.\n", filename);
        free(json_str);
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s", json_str);
    fclose(file);
    free(json_str);
}

void merge_processors_by_cpu_id(cJSON* partial_crash_data,
                                cJSON* merge_metadata, cJSON* merge_processors)
{
    cJSON* partial_processors =
        get_json_node_exit_when_failed(partial_crash_data, "PROCESSORS");
    cJSON* partial_cpu_idx = NULL;
    if (cJSON_GetObjectItemCaseSensitive(partial_processors, "_version") !=
            NULL &&
        cJSON_GetObjectItemCaseSensitive(merge_processors, "_version") == NULL)
    {
        cJSON* version = cJSON_DetachItemFromObjectCaseSensitive(
            partial_processors, "_version");
        cJSON_AddItemToObject(merge_processors, "_version", version);
    }
    cJSON_ArrayForEach(partial_cpu_idx, partial_processors)
    {
        const char* cpu_idx = partial_cpu_idx->string;
        if (startsWith(cpu_idx, "cpu") == false)
        {
            continue;
        }
        compare_ppin_exit_for_fail(merge_metadata, merge_metadata, cpu_idx);
        cJSON* partial_cpu_idx =
            cJSON_GetObjectItemCaseSensitive(partial_processors, cpu_idx);
        merge_cpu_idx_in_processors(merge_processors, partial_cpu_idx);
    }
}

void merge_journal_if_exists(cJSON* journal, cJSON* merge_root)
{
    if (journal)
    {
        cJSON_AddItemToObject(merge_root, "journal", journal);
    }
}

void get_partial_filename(char* filename, const char* basename, int i,
                          int total)
{
    cd_snprintf_s(filename, PATH_MAX, "%s_%dof%d.json", basename, i, total);
}

void check_merge(const char* basename, int y)
{
    printf("Merging %d files with basename: %s, checking...\n", y, basename);
    cJSON* merge_root = cJSON_CreateObject();
    cJSON* merge_crash_data = cJSON_CreateObject();
    cJSON_AddItemToObject(merge_root, "crash_data", merge_crash_data);
    cJSON *merge_metadata = NULL, *merge_processors = NULL, *journal = NULL;

    for (int i = 1; i <= y; ++i)
    {
        char filename[PATH_MAX] = {0};
        get_partial_filename(filename, basename, i, y);
        if (is_file_exists(filename) == false)
        {
            printf("Warning: File not found, continue to merge with out this "
                   "file: %s\n",
                   filename);
            continue;
        }
        printf("Merging file: %s\n", filename);
        cJSON* partial_root = parse_json_from_file(filename);
        if (!partial_root)
        {
            printf("Failed to parse file: %s\n", filename);
            exit(EXIT_FAILURE);
        }
        if (i == 1)
        {
            journal = cJSON_DetachItemFromObjectCaseSensitive(partial_root,
                                                              "journal");
            if (journal)
            {
                printf("Journal found in the 1st file, adding to the "
                       "merged file\n");
            }
        }
        cJSON* partial_crash_data =
            get_json_node_exit_when_failed(partial_root, "crash_data");
        if (merge_metadata == NULL)
        {
            merge_metadata = cJSON_DetachItemFromObjectCaseSensitive(
                partial_crash_data, "METADATA");
            cJSON_AddItemToObject(merge_crash_data, "METADATA", merge_metadata);
            merge_processors = cJSON_CreateObject();
            cJSON_AddItemToObject(merge_crash_data, "PROCESSORS",
                                  merge_processors);
        }
        merge_processors_by_cpu_id(partial_crash_data, merge_metadata,
                                   merge_processors);
        cJSON_Delete(partial_root);
    }
    merge_journal_if_exists(journal, merge_root);
    char file_to_write[PATH_MAX] = {0};
    cd_snprintf_s(file_to_write, sizeof(file_to_write), "%s_merged.json",
                  basename);
    write_json_to_file_exit_for_fail(merge_root, file_to_write);
    printf("Merged to: %s\n", file_to_write);
    cJSON_Delete(merge_root);
}

void remove_merge_input(int total, const char* basename)
{
    printf("--remove enabled, starting removal of input files\n");
    for (int i = 0; i < total; i++)
    {
        char file_to_remove[PATH_MAX] = {0};
        cd_snprintf_s(file_to_remove, sizeof(file_to_remove), "%s_%dof%d.json",
                      basename, i + 1, total);
        if (remove(file_to_remove) != 0)
        {
            perror("Failed to remove input file");
        }
        else
        {
            printf("Input file: %s removed.\n", file_to_remove);
        }
    }
}

void remove_merge_if_opt_enabled(const Options* opts, const char* basename,
                                 int total)
{
    if (opts->remove)
    {
        remove_merge_input(total, basename);
    }
    else
    {
        printf("--remove not enabled, input files are not removed\n");
    }
}

bool check_if_merge_file(const char* filename, const char* basename, int* x,
                         int* y)
{
    regex_t regex;
    regmatch_t matches[REG_FOUND_MERGE_PATTERN];

    char pattern[PATH_MAX];
    cd_snprintf_s(pattern, sizeof(pattern), "^%s_([0-9]+)of([0-9]+)\\.json$",
                  basename);

    int reti = regcomp(&regex, pattern, REG_EXTENDED);
    if (reti)
    {
        char msgbuf[PATH_MAX];
        regerror(reti, &regex, msgbuf, sizeof(msgbuf));
        fprintf(stderr, "Could not compile regex: %s\n", msgbuf);
        return false;
    }

    reti = regexec(&regex, filename, 4, matches, 0);
    if (reti == 0)
    {
        char x_str[MAX_CPU_IDX_KEY];
        int x_len = matches[1].rm_eo - matches[1].rm_so;
        strncpy_s(x_str, sizeof(x_str), filename + matches[1].rm_so, x_len);
        x_str[x_len] = '\0';

        char* endptr;
        errno = 0;
        long x_val = strtol(x_str, &endptr, 10);
        if (errno != 0 || *endptr != '\0' || x_val < INT_MIN || x_val > INT_MAX)
        {
            regfree(&regex);
            return false;
        }
        *x = (int)x_val;

        char y_str[MAX_CPU_IDX_KEY];
        int y_len = matches[2].rm_eo - matches[2].rm_so;
        strncpy_s(y_str, sizeof(y_str), filename + matches[2].rm_so, y_len);
        y_str[y_len] = '\0';

        errno = 0;
        long y_val = strtol(y_str, &endptr, 10);
        if (errno != 0 || *endptr != '\0' || y_val < INT_MIN || y_val > INT_MAX)
        {
            regfree(&regex);
            return false;
        }
        *y = (int)y_val;

        regfree(&regex);
        return true;
    }

    regfree(&regex);
    return false;
}

void scan_pattern_merge(const Options* opts)
{
    const char* basename = opts->basename;
    DIR* dir;
    struct dirent* ent;

    if ((dir = opendir(".")) == NULL)
    {
        printf("Error: Unable to open directory.\n");
        exit(EXIT_FAILURE);
    }
    int x = -1, y = -1, last_y = -1, total = 0;
    while ((ent = readdir(dir)) != NULL)
    {
        if (check_if_merge_file(ent->d_name, basename, &x, &y) == false)
        {
            continue;
        }
        if (x <= 0 || x > MAX_FILES_TO_MERGE || y <= 0 ||
            y > MAX_FILES_TO_MERGE || x > y)
        {
            printf("Error: Invalid X, Y value in file: %s\n", ent->d_name);
            closedir(dir);
            exit(EXIT_FAILURE);
        }
        if (last_y == -1)
        {
            last_y = y;
        }
        else if (last_y != y)
        {
            printf("Error: The Y value in %s_XofY is not consistent: "
                   "discrepancy in Y values: %d (expected) vs %d (found) in "
                   "filename: %s. "
                   "Previous Y value was %d.\n",
                   basename, last_y, y, ent->d_name, last_y);
            closedir(dir);
            exit(EXIT_FAILURE);
        }
        ++total;
    }
    closedir(dir);
    const int min_files_to_merge = 2;
    if (total < min_files_to_merge)
    {
        printf("Error: Not enough files found with basename: %s to be merged\n",
               basename);
        exit(EXIT_FAILURE);
    }

    check_merge(basename, y);
    remove_merge_if_opt_enabled(opts, basename, total);
}

bool check_num_cpus_files_reasonable(cJSON* metadata, int numcpus)
{
    char cpu_idx[MAX_CPU_IDX_KEY] = {0};
    cd_snprintf_s(cpu_idx, sizeof(cpu_idx), "cpu%d", numcpus - 1);
    int cpu_count;
    for (cpu_count = 0; cpu_count < _max_cpu_count; cpu_count++)
    {
        cd_snprintf_s(cpu_idx, sizeof(cpu_idx), "cpu%d", cpu_count);
        cJSON* cpunums = cJSON_GetObjectItemCaseSensitive(metadata, cpu_idx);
        if (!cpunums)
        {
            break;
        }
    }
    if (cpu_count == numcpus)
    {
        return true;
    }
    else
    {
        printf("Max cpu count in metadata is: %d, expected cpu number is: %d\n",
               cpu_count, numcpus);
        return false;
    }
}

void split_opts_check_exit_for_fail(const Options* opts)
{
    if (opts->numfiles < 0)
    {
        fprintf(stderr, "Invalid number of files: %d\n", opts->numfiles);
        print_help();
        exit(EXIT_FAILURE);
    }
    if (opts->numcpus < 0 || opts->numcpus > _max_cpu_count)
    {
        fprintf(stderr, "Invalid number of CPUs: %d\n", opts->numcpus);
        print_help();
        exit(EXIT_FAILURE);
    }
    if (opts->numcpus < opts->numfiles)
    {
        fprintf(stderr, "Number of CPUs must be greater than or equal to the "
                        "number of files.\n");
        exit(EXIT_FAILURE);
    }
    if (opts->numcpus % opts->numfiles)
    {
        fprintf(stderr,
                "Number of CPUs must be divisible by the number of files.\n");
        exit(EXIT_FAILURE);
    }
}

void remove_split_input(const char* filename)
{
    printf("--remove enabled, starting removal of input file\n");
    if (remove(filename) != 0)
    {
        perror("Failed to remove input file");
    }
    else
    {
        printf("Input file: %s removed.\n", filename);
    }
}

void remove_split_if_opt_enabled(const Options* opts, const char* filename)
{
    if (opts->remove)
    {
        remove_split_input(filename);
    }
    else
    {
        printf("--remove not enabled, input file is not removed\n");
    }
}

void get_split_filename(const char* filename, char* split_filename, int i,
                        int numfiles)
{
    char basename[MAX_BASENAME] = {0};
    memset_s(basename, sizeof(basename), 0, sizeof(basename));
    size_t filename_len = strnlen_s(filename, PATH_MAX);
    size_t ext_len = strnlen_s(".json", 6);
    if (filename_len - ext_len > MAX_BASENAME)
    {
        fprintf(stderr, "Filename too long\n");
        exit(EXIT_FAILURE);
    }
    strncpy_s(basename, sizeof(basename), filename, filename_len - ext_len);
    basename[filename_len - ext_len] = '\0';
    cd_snprintf_s(split_filename, PATH_MAX, "%s_%dof%d.json", basename, i,
                  numfiles);
}

void split_cpu_in_processor(cJSON* split_root, cJSON* split_crash_data,
                            cJSON* metadata, int split_cpu_idx_start,
                            int numcpus, int numfiles, cJSON* processors)
{
    cJSON_AddItemToObject(split_root, "crash_data", split_crash_data);

    cJSON_AddItemToObject(split_crash_data, "METADATA",
                          cJSON_Duplicate(metadata, 1));

    cJSON* split_processors = cJSON_CreateObject();
    cJSON_AddItemToObject(split_crash_data, "PROCESSORS", split_processors);
    cJSON* version = cJSON_GetObjectItemCaseSensitive(processors, "_version");
    if (version)
    {
        cJSON_AddItemToObject(split_processors, "_version",
                              cJSON_Duplicate(version, 1));
    }
    for (int idx_in_file = split_cpu_idx_start;
         idx_in_file < split_cpu_idx_start + numcpus / numfiles; idx_in_file++)
    {
        char cpu_idx[MAX_CPU_IDX_KEY] = {0};
        cd_snprintf_s(cpu_idx, sizeof(cpu_idx), "cpu%d", idx_in_file);
        cJSON* idx_in_metadata =
            cJSON_GetObjectItemCaseSensitive(metadata, cpu_idx);
        if (!idx_in_metadata)
        {
            printf("%s is not in metadata, checking your input or "
                   "parameters\n",
                   cpu_idx);
            exit(EXIT_FAILURE);
        }

        cJSON* idx_in_processors =
            cJSON_DetachItemFromObjectCaseSensitive(processors, cpu_idx);
        if (!idx_in_processors)
        {
            printf("%s is not in metadata, checking your input or "
                   "parameters\n",
                   cpu_idx);
            exit(EXIT_FAILURE);
        }
        cJSON_AddItemToObject(split_processors, cpu_idx, idx_in_processors);
    }
}

void split_by_cpu_idx(int numfiles, cJSON* metadata, int numcpus,
                      cJSON* processors, cJSON* journal, const char* filename)
{
    int split_cpu_idx_start = 0;
    for (int i = 1; i <= numfiles; i++)
    {
        cJSON* split_root = cJSON_CreateObject();
        cJSON* split_crash_data = cJSON_CreateObject();
        split_cpu_in_processor(split_root, split_crash_data, metadata,
                               split_cpu_idx_start, numcpus, numfiles,
                               processors);
        if (i == 1 && journal)
        {
            printf("Journal found, adding to the 1st split file\n");
            cJSON_AddItemToObject(split_root, "journal", journal);
        }
        split_cpu_idx_start += numcpus / numfiles;
        char split_filename[PATH_MAX] = {0};
        get_split_filename(filename, split_filename, i, numfiles);
        write_json_to_file_exit_for_fail(split_root, split_filename);
        printf("Split to: %s\n", split_filename);
        cJSON_Delete(split_root);
    }
}

void split_crashdump(const Options* opts)
{
    split_opts_check_exit_for_fail(opts);

    const char* filename = opts->filename;
    int numfiles = opts->numfiles, numcpus = opts->numcpus;
    cJSON* json_root = parse_json_from_file(filename);
    if (!json_root)
    {
        printf("Failed to parse file: %s\n", filename);
        exit(EXIT_FAILURE);
    }
    cJSON* crash_data = get_json_node_exit_when_failed(json_root, "crash_data");
    cJSON* journal = cJSON_DetachItemFromObjectCaseSensitive(
        json_root, "journal"); // the "journal"'s absence is acceptable.
    cJSON* metadata = get_json_node_exit_when_failed(crash_data, "METADATA");
    if (!check_num_cpus_files_reasonable(metadata, numcpus))
    {
        printf("Error: The number of CPUs specified in the metadata does not "
               "match the expected number of CPUs.\n"
               "       Make sure the metadata accurately reflects the system "
               "configuration, or consider using custom parameters.\n"
               "       Use the '-h' option to display help information on "
               "setting the expected number of CPUs and other parameters.\n");

        cJSON_Delete(json_root);
        exit(EXIT_FAILURE);
    }
    cJSON* processors =
        get_json_node_exit_when_failed(crash_data, "PROCESSORS");
    split_by_cpu_idx(numfiles, metadata, numcpus, processors, journal,
                     filename);
    cJSON_Delete(json_root);
    remove_split_if_opt_enabled(opts, filename);
}

int main(int argc, char** argv)
{
    const Options opts = parse_options(argc, argv);

    if (opts.merge)
    {
        printf("Merge mode activated. Basename: %s\n", opts.basename);
        scan_pattern_merge(&opts);
    }
    else
    {
        printf("Split mode activated. Expect number of CPUs: %d, Number of "
               "files: %d, Filename: %s\n",
               opts.numcpus, opts.numfiles, opts.filename);
        if (!opts.filename)
        {
            fprintf(stderr,
                    "Error: Filename must be provided for split mode.\n");
            print_help();
            return EXIT_FAILURE;
        }
        split_crashdump(&opts);
    }
    return 0;
}
