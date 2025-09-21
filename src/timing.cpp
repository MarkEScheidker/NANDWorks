#include "timing.hpp"
#include <time.h>

// Get a timestamp in nanoseconds.
uint64_t get_timestamp_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

// Busy-wait for a specific number of nanoseconds.
void busy_wait_ns(uint64_t ns) {
    uint64_t start = get_timestamp_ns();
    while (get_timestamp_ns() - start < ns);
}

// Busy-wait for a specific number of cycles.
void busy_wait_cycles(uint32_t cycles) {
    for (uint32_t i = 0; i < cycles; ++i) {
        asm volatile("nop");
    }
}