#include "system_monitor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static void initialize_info(ProcessInfo *info, pid_t pid)
{
    memset(info, 0, sizeof(*info));

    info->pid = pid;
    info->page_size = sysconf(_SC_PAGESIZE);

    if (info->page_size <= 0)
        info->page_size = 4096;
}

static int read_status(pid_t pid, ProcessInfo *info)
{
    char path[256];
    char line[512];

    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *file = fopen(path, "r");

    if (file == NULL)
        return -1;

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
            sscanf(line, "VmRSS:%ld", &info->rss_kb);
        }
        else if (strncmp(line, "VmData:", 7) == 0)
        {
            sscanf(line, "VmData:%ld", &info->vm_data_kb);
        }
        else if (strncmp(line, "VmStk:", 7) == 0)
        {
            sscanf(line, "VmStk:%ld", &info->vm_stk_kb);
        }
        else if (strncmp(line, "VmExe:", 7) == 0)
        {
            sscanf(line, "VmExe:%ld", &info->vm_exe_kb);
        }
        else if (strncmp(line, "VmLib:", 7) == 0)
        {
            sscanf(line, "VmLib:%ld", &info->vm_lib_kb);
        }
        else if (strncmp(line, "Threads:", 8) == 0)
        {
            sscanf(line, "Threads:\t%d", &info->threads);
        }
    }

    fclose(file);

    return 0;
}

static int read_stat(pid_t pid, ProcessInfo *info)
{
    char path[256];
    char buffer[4096];

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *file = fopen(path, "r");

    if (file == NULL)
        return -1;

    if (fgets(buffer, sizeof(buffer), file) == NULL)
    {
        fclose(file);
        return -1;
    }

    fclose(file);

    /*
     * /proc/<pid>/stat:
     *
     * field 3  = state
     * field 10 = minor faults
     * field 12 = major faults
     * field 14 = user CPU time
     * field 15 = system CPU time
     * field 23 = virtual memory size
     * field 24 = resident set size
     */

    char *close_paren = strrchr(buffer, ')');

    if (close_paren == NULL)
        return -1;

    char *ptr = close_paren + 2;

    int field = 3;

    while (*ptr != '\0' && field <= 24)
    {
        while (*ptr == ' ')
            ptr++;

        if (*ptr == '\0')
            break;

        char *end = strchr(ptr, ' ');

        char value_string[128];

        if (end != NULL)
        {
            size_t length = (size_t)(end - ptr);

            if (length >= sizeof(value_string))
                length = sizeof(value_string) - 1;

            memcpy(value_string, ptr, length);
            value_string[length] = '\0';
        }
        else
        {
            snprintf(value_string,
                     sizeof(value_string),
                     "%.127s",
                     ptr);
        }

        if (field == 3)
        {
            snprintf(info->state,
                     sizeof(info->state),
                     "%.63s",
                     value_string);
        }
        else if (field == 10)
        {
            info->minor_faults =
                strtoul(value_string, NULL, 10);
        }
        else if (field == 12)
        {
            info->major_faults =
                strtoul(value_string, NULL, 10);
        }
        else if (field == 14)
        {
            info->process_user_time =
                strtoull(value_string, NULL, 10);
        }
        else if (field == 15)
        {
            info->process_system_time =
                strtoull(value_string, NULL, 10);
        }
        else if (field == 23)
        {
            info->vsize_bytes =
                strtoul(value_string, NULL, 10);
        }
        else if (field == 24)
        {
            info->rss_pages =
                strtol(value_string, NULL, 10);
        }

        if (end == NULL)
            break;

        ptr = end + 1;
        field++;
    }

    return 0;
}


static unsigned long long read_total_cpu_time(void)
{
    FILE *file = fopen("/proc/stat", "r");

    if (file == NULL)
        return 0;

    char line[512];

    if (fgets(line, sizeof(line), file) == NULL)
    {
        fclose(file);
        return 0;
    }

    fclose(file);

    unsigned long long user = 0;
    unsigned long long nice = 0;
    unsigned long long system = 0;
    unsigned long long idle = 0;
    unsigned long long iowait = 0;
    unsigned long long irq = 0;
    unsigned long long softirq = 0;
    unsigned long long steal = 0;

    sscanf(line,
           "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
           &user,
           &nice,
           &system,
           &idle,
           &iowait,
           &irq,
           &softirq,
           &steal);

    return user + nice + system + idle +
           iowait + irq + softirq + steal;
}


static void calculate_cpu_percent(ProcessInfo *info)
{
    static unsigned long long previous_process_time = 0;
    static unsigned long long previous_total_time = 0;
    static pid_t previous_pid = -1;

    unsigned long long process_time =
        info->process_user_time +
        info->process_system_time;

    unsigned long long total_time =
        read_total_cpu_time();

    if (previous_pid != info->pid ||
        previous_total_time == 0)
    {
        previous_process_time = process_time;
        previous_total_time = total_time;
        previous_pid = info->pid;

        info->cpu_percent = 0.0;
        return;
    }

    if (total_time <= previous_total_time ||
        process_time < previous_process_time)
    {
        info->cpu_percent = 0.0;

        previous_process_time = process_time;
        previous_total_time = total_time;

        return;
    }

    unsigned long long process_delta =
        process_time - previous_process_time;

    unsigned long long total_delta =
        total_time - previous_total_time;

    long cpu_count =
        sysconf(_SC_NPROCESSORS_ONLN);

    if (cpu_count < 1)
        cpu_count = 1;

    info->cpu_percent =
        ((double)process_delta /
         (double)total_delta) *
        100.0 *
        (double)cpu_count;

    if (info->cpu_percent < 0.0)
        info->cpu_percent = 0.0;

    if (info->cpu_percent > 100.0 * cpu_count)
        info->cpu_percent = 100.0 * cpu_count;

    previous_process_time = process_time;
    previous_total_time = total_time;
}


int monitor_read(pid_t pid, ProcessInfo *info)
{
    if (pid <= 0 || info == NULL)
        return -1;

    initialize_info(info, pid);

    if (read_status(pid, info) != 0)
        return -1;

    if (read_stat(pid, info) != 0)
        return -1;

    /*
     * RSS from /proc/<pid>/stat is measured in pages.
     * Convert it to KB.
     */
    info->rss_kb =
        (info->rss_pages * info->page_size) / 1024;

    calculate_cpu_percent(info);

    return 0;
}

void monitor_print_text(const ProcessInfo *info)
{
    printf("========================================\n");
    printf("              AIPage Monitor\n");
    printf("========================================\n");

    printf("PID             : %d\n", info->pid);
    printf("Name            : %s\n", info->name);
    printf("State           : %s\n", info->state);

    printf("\nMemory\n");
    printf("----------------------------------------\n");

    printf("Virtual Memory  : %lu KB\n",
           info->vsize_bytes / 1024);

    printf("RSS             : %ld KB\n",
           info->rss_kb);

    printf("Data            : %ld KB\n",
           info->vm_data_kb);

    printf("Stack           : %ld KB\n",
           info->vm_stk_kb);

    printf("Executable      : %ld KB\n",
           info->vm_exe_kb);

    printf("Libraries       : %ld KB\n",
           info->vm_lib_kb);

    printf("Threads         : %d\n",
           info->threads);

    printf("CPU Usage       : %.2f%%\n",
           info->cpu_percent);

    printf("\nPage Fault Statistics\n");
    printf("----------------------------------------\n");

    printf("Minor Faults    : %lu\n",
           info->minor_faults);

    printf("Major Faults    : %lu\n",
           info->major_faults);

    printf("Minor Faults/s  : %lu\n",
           info->minor_faults_per_sec);

    printf("Major Faults/s  : %lu\n",
           info->major_faults_per_sec);

    printf("RSS Pages       : %ld\n",
           info->rss_pages);

    printf("Page Size       : %ld bytes\n",
           info->page_size);

    printf("========================================\n");
}

void monitor_print_json(const ProcessInfo *info)
{
    printf(
        "{"
        "\"pid\":%d,"
        "\"name\":\"%s\","
        "\"state\":\"%s\","
        "\"minor_faults\":%lu,"
        "\"major_faults\":%lu,"
        "\"minor_faults_per_sec\":%lu,"
        "\"major_faults_per_sec\":%lu,"
        "\"vsize_bytes\":%lu,"
        "\"rss_pages\":%ld,"
        "\"rss_kb\":%ld,"
        "\"vm_data_kb\":%ld,"
        "\"vm_stk_kb\":%ld,"
        "\"vm_exe_kb\":%ld,"
        "\"vm_lib_kb\":%ld,"
        "\"threads\":%d,"
        "\"page_size\":%ld,"
        "\"cpu_percent\":%.2f"
        "}\n",
        info->pid,
        info->name,
        info->state,
        info->minor_faults,
        info->major_faults,
        info->minor_faults_per_sec,
        info->major_faults_per_sec,
        info->vsize_bytes,
        info->rss_pages,
        info->rss_kb,
        info->vm_data_kb,
        info->vm_stk_kb,
        info->vm_exe_kb,
        info->vm_lib_kb,
        info->threads,
        info->page_size,
        info->cpu_percent
    );
}
