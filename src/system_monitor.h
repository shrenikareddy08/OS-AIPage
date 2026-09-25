#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <stddef.h>
#include <sys/types.h>

typedef struct
{
    pid_t pid;

    char name[256];
    char state[64];

    unsigned long minor_faults;
    unsigned long major_faults;

    unsigned long minor_faults_per_sec;
    unsigned long major_faults_per_sec;

    unsigned long vsize_bytes;
    long rss_pages;
    long rss_kb;

    long vm_data_kb;
    long vm_stk_kb;
    long vm_exe_kb;
    long vm_lib_kb;

    int threads;

    long page_size;

    unsigned long long process_user_time;
    unsigned long long process_system_time;

    double cpu_percent;

} ProcessInfo;

int monitor_read(pid_t pid, ProcessInfo *info);

void monitor_print_text(const ProcessInfo *info);

void monitor_print_json(const ProcessInfo *info);

#endif
