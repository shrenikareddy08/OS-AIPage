#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

typedef struct
{
    unsigned long minor_faults;
    unsigned long major_faults;
    unsigned long vsize;
    long rss_pages;

    char name[256];
    char state[64];

    long vm_rss;
    long vm_data;
    long vm_stk;
    long vm_exe;
    long vm_lib;
    int threads;

} ProcessInfo;


/*
 * Read basic memory information from /proc/<pid>/status
 */
int read_memory_info(int pid, ProcessInfo *info)
{
    char path[256];
    char line[512];

    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *file = fopen(path, "r");

    if (file == NULL)
    {
        perror("Unable to open /proc status");
        return -1;
    }

    while (fgets(line, sizeof(line), file))
    {
        if (strncmp(line, "Name:", 5) == 0)
        {
            sscanf(line, "Name:\t%255[^\n]", info->name);
        }
        else if (strncmp(line, "State:", 6) == 0)
        {
            sscanf(line, "State:\t%63[^\n]", info->state);
        }
        else if (strncmp(line, "VmRSS:", 6) == 0)
        {
            sscanf(line, "VmRSS:%ld", &info->vm_rss);
        }
        else if (strncmp(line, "VmData:", 7) == 0)
        {
            sscanf(line, "VmData:%ld", &info->vm_data);
        }
        else if (strncmp(line, "VmStk:", 6) == 0)
        {
            sscanf(line, "VmStk:%ld", &info->vm_stk);
        }
        else if (strncmp(line, "VmExe:", 6) == 0)
        {
            sscanf(line, "VmExe:%ld", &info->vm_exe);
        }
        else if (strncmp(line, "VmLib:", 6) == 0)
        {
            sscanf(line, "VmLib:%ld", &info->vm_lib);
        }
        else if (strncmp(line, "Threads:", 8) == 0)
        {
            sscanf(line, "Threads:\t%d", &info->threads);
        }
    }

    fclose(file);

    return 0;
}


/*
 * Read page-fault and virtual-memory statistics
 * from /proc/<pid>/stat
 */
int read_page_statistics(int pid, ProcessInfo *info)
{
    char path[256];
    char buffer[4096];

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *file = fopen(path, "r");

    if (file == NULL)
    {
        perror("Unable to open /proc stat");
        return -1;
    }

    if (fgets(buffer, sizeof(buffer), file) == NULL)
    {
        fclose(file);
        return -1;
    }

    fclose(file);

    /*
     * The process name can contain spaces and parentheses.
     * Therefore locate the final ')' before parsing fields.
     */
    char *close_paren = strrchr(buffer, ')');

    if (close_paren == NULL)
    {
        return -1;
    }

    /*
     * After ')' comes:
     *
     * field 3 = state
     *
     * We begin parsing from field 4.
     */
    char *ptr = close_paren + 2;

    /*
     * Skip field 3 (state)
     */
    ptr = strchr(ptr, ' ');

    if (ptr == NULL)
        return -1;

    ptr++;

    int field = 4;

    while (*ptr != '\0' && field <= 24)
    {
        while (*ptr == ' ')
            ptr++;

        if (*ptr == '\0')
            break;

        char *end = strchr(ptr, ' ');

        char temp[64];

        if (end != NULL)
        {
            size_t length = end - ptr;

            if (length >= sizeof(temp))
                length = sizeof(temp) - 1;

            memcpy(temp, ptr, length);
            temp[length] = '\0';
        }
        else
        {
            snprintf(temp, sizeof(temp), "%s", ptr);
        }

        unsigned long value = strtoul(temp, NULL, 10);

        if (field == 10)
        {
            info->minor_faults = value;
        }
        else if (field == 12)
        {
            info->major_faults = value;
        }
        else if (field == 23)
        {
            info->vsize = value;
        }
        else if (field == 24)
        {
            info->rss_pages = (long)value;
        }

        if (end == NULL)
            break;

        ptr = end + 1;
        field++;
    }

    return 0;
}


/*
 * Display one monitoring sample
 */
void display_sample(int pid, ProcessInfo *info,
                    unsigned long previous_minor,
                    unsigned long previous_major)
{
    long page_size = sysconf(_SC_PAGESIZE);

    unsigned long minor_delta = 0;
    unsigned long major_delta = 0;

    if (info->minor_faults >= previous_minor)
        minor_delta = info->minor_faults - previous_minor;

    if (info->major_faults >= previous_major)
        major_delta = info->major_faults - previous_major;

    printf("\033[2J\033[H");

    printf("========================================\n");
    printf("           AIPage Monitor\n");
    printf("========================================\n");

    printf("PID: %d\n", pid);
    printf("Name:   %s\n", info->name);
    printf("State:  %s\n", info->state);

    printf("\nMemory\n");
    printf("----------------------------------------\n");

    printf("VmSize          : %lu KB\n",
           info->vsize / 1024);

    printf("VmRSS           : %ld KB\n",
           info->vm_rss);

    printf("VmData          : %ld KB\n",
           info->vm_data);

    printf("VmStk           : %ld KB\n",
           info->vm_stk);

    printf("VmExe           : %ld KB\n",
           info->vm_exe);

    printf("VmLib           : %ld KB\n",
           info->vm_lib);

    printf("Threads         : %d\n",
           info->threads);

    printf("\nPage Statistics\n");
    printf("----------------------------------------\n");

    printf("Minor Faults    : %lu\n",
           info->minor_faults);

    printf("Major Faults    : %lu\n",
           info->major_faults);

    printf("Minor Faults/s  : %lu\n",
           minor_delta);

    printf("Major Faults/s  : %lu\n",
           major_delta);

    printf("RSS Pages       : %ld\n",
           info->rss_pages);

    printf("Page Size       : %ld bytes\n",
           page_size);

    printf("Calculated RSS  : %ld KB\n",
           (info->rss_pages * page_size) / 1024);

    printf("\nMonitoring... Press Ctrl+C to stop.\n");

    printf("========================================\n");
}


int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <PID>\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);

    if (pid <= 0)
    {
        printf("Invalid PID\n");
        return 1;
    }

    ProcessInfo info;

    memset(&info, 0, sizeof(info));

    /*
     * First sample
     */
    if (read_memory_info(pid, &info) != 0 ||
        read_page_statistics(pid, &info) != 0)
    {
        return 1;
    }

    unsigned long previous_minor = info.minor_faults;
    unsigned long previous_major = info.major_faults;

    while (1)
    {
        sleep(1);

        memset(&info, 0, sizeof(info));

        if (read_memory_info(pid, &info) != 0 ||
            read_page_statistics(pid, &info) != 0)
        {
            printf("\nProcess %d is no longer available.\n", pid);
            break;
        }

        display_sample(pid,
                       &info,
                       previous_minor,
                       previous_major);

        previous_minor = info.minor_faults;
        previous_major = info.major_faults;
    }

    return 0;
}
