#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>

// Get the current timestamp in nanoseconds
uint64_t get_timestamp_ns();

// Busy-wait for a specified number of nanoseconds
void busy_wait_ns(uint64_t ns);

// Busy-wait for a specific number of cycles.
void busy_wait_cycles(uint32_t cycles);





#endif // TIMING_H
