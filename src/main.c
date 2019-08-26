#define _GNU_SOURCE

#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <hdr_histogram.h>
#include <sched.h>

const long long PG_SIZE = 4096L;
const long long MAP_SIZE = 8L * 1024 * 1024 * 1024;
const int BATCH_SIZE = 100;
const long long REPORT_BATCH_INTVL = 100000;

#define REPORT_EVERY REPORT_BATCH_INTVL * BATCH_SIZE * PG_SIZE

struct hdr_histogram* histo;

typedef struct thread_args {
    void *addr1;
    void *addr2;
    int target_cpu;
};

long long now() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

void* create_mapping(const char *path) {
    int fd = open(path, O_RDWR | O_CREAT, (mode_t) 0644);
    lseek(fd, MAP_SIZE - 1, SEEK_SET);
    write(fd, "", 1);
    char *addr = mmap(NULL, MAP_SIZE, PROT_WRITE, MAP_SHARED, fd, 0L);

    // pre-fault every page
    for (long idx = 0; idx < MAP_SIZE - 1; idx += PG_SIZE) *(addr + idx) = 0;

    return addr;
}

void read_loop(char* addr) {
    long idx = 0;

    while (idx + (BATCH_SIZE * PG_SIZE) < MAP_SIZE) {
        long long int start = now();

        for (int i = 0; i < BATCH_SIZE; i++) {
            *(addr + idx) = 0xAA;
            idx += PG_SIZE;
        }

        long long int finish = now();
        hdr_record_value(histo, (finish - start) / BATCH_SIZE);

        if (idx % REPORT_EVERY == 0) {
            printf("mean: %.2f, min: %ld, 50th: %ld, 90th: %ld, 99th: %ld, max: %ld\n",
                    hdr_mean(histo), hdr_min(histo), hdr_value_at_percentile(histo, 50), hdr_value_at_percentile(histo, 90), hdr_value_at_percentile(histo, 99), hdr_max(histo));
            hdr_reset(histo);
        }
    }
}

void read_in_background(struct thread_args *args) {
    long long cpu = 1L << args->target_cpu;
    sched_setaffinity(0, sizeof(cpu), (const cpu_set_t *) &cpu);
    printf("Running on cpu %d\n", sched_getcpu());

    read_loop(args->addr1);
    printf("Finished first read loop, starting second\n");

    while(1) read_loop(args->addr2);
}

int main() {
    long long cpu = 1L << 22;
    sched_setaffinity(0, sizeof(cpu), (const cpu_set_t *) &cpu);

    hdr_init(1, INT64_C(3600000000), 3, &histo);

    char* addr1 = create_mapping("/dev/shm/file01");
    char* addr2 = create_mapping("/dev/shm/file02");

    pthread_t reader_thread;
    struct thread_args args = {.addr1 = addr1, .addr2 = addr2, .target_cpu = 23};
    pthread_create(&reader_thread, NULL, (void *(*)(void *)) read_in_background, &args);

    sleep(3);
    printf("Unmapping...\n");
    munmap(addr1, MAP_SIZE);
    printf("Unmapped...\n");
    sleep(3);

    printf("Done\n");
    fflush(stdout);
}


