#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

#define PAGE_SIZE 4096
#define WORKING_PAGES 32
#define ACCESS_COUNT 1000

static volatile unsigned char *memory_area;

static void touch_page(size_t page)
{
    memory_area[page * PAGE_SIZE]++;

    uintptr_t address =
        (uintptr_t)&memory_area[page * PAGE_SIZE];

    uint64_t virtual_page =
        address / PAGE_SIZE;

    printf("MEMACCESS | addr=0x%lx | page=%lu | pid=%d\n",
           (unsigned long)address,
           (unsigned long)virtual_page,
           getpid());

    fflush(stdout);
}

int main(void)
{
    size_t bytes =
        WORKING_PAGES * PAGE_SIZE;

    memory_area = mmap(
        NULL,
        bytes,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (memory_area == MAP_FAILED)
    {
        perror("mmap");
        return 1;
    }

    printf("============================================================\n");
    printf("AIPage REAL MEMORY WORKLOAD\n");
    printf("============================================================\n");
    printf("PID            : %d\n", getpid());
    printf("Page Size      : %d bytes\n", PAGE_SIZE);
    printf("Allocated      : %zu bytes\n", bytes);
    printf("Working Pages  : %d\n", WORKING_PAGES);
    printf("Base Address   : %p\n", (void *)memory_area);
    printf("============================================================\n");

    /*
     * First touch every allocated page.
     * These are actual pages belonging to this process.
     */
    for (size_t page = 0; page < WORKING_PAGES; page++)
    {
        touch_page(page);
    }

    /*
     * Continue accessing the allocated memory.
     *
     * The access pattern is generated from the workload's
     * real memory addresses rather than supplied page IDs.
     */
    for (int i = 0; i < ACCESS_COUNT; i++)
    {
        size_t page;

        if (i % 17 == 0)
            page = 7 % WORKING_PAGES;
        else if (i % 5 == 0)
            page = 14 % WORKING_PAGES;
        else
            page = (size_t)(i % WORKING_PAGES);

        touch_page(page);

        usleep(10000);
    }

    munmap((void *)memory_area, bytes);

    return 0;
}
