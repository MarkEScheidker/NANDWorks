#ifndef HARDWARE_LOCATIONS_H
#define HARDWARE_LOCATIONS_H

#include <fstream>
#include "timing.h"
#include "gpio.h"

// Enables/disables detailed timing profiles for operations.
#define PROFILE_TIME true

// Forces a function to be inlined.
#define FORCE_INLINE __attribute__((always_inline))

// Timing macros using nanosecond precision.
#define START_TIME do { uint64_t start_tick = get_timestamp_ns();
#define END_TIME   uint64_t end_tick = get_timestamp_ns(); time_info_file << "  took " << (end_tick - start_tick) / 1000 << " microseconds\n"; } while(0);
#define PRINT_TIME // Handled within END_TIME
#define GET_TIME_ELAPSED (end_tick - start_tick)

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Pin Definitions
//
// This section centralizes all GPIO pin assignments for the project.
// Each pin is defined with a clear name, its BCM number, and a comment explaining its purpose
// and connection on the NAND flash memory interface.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Data Quentin (DQ) Pins: Bidirectional data bus for transferring commands, addresses, and data.
#define GPIO_DQ0 21  // Data Bus Line 0
#define GPIO_DQ1 20  // Data Bus Line 1
#define GPIO_DQ2 16  // Data Bus Line 2
#define GPIO_DQ3 12  // Data Bus Line 3
#define GPIO_DQ4 25  // Data Bus Line 4
#define GPIO_DQ5 24  // Data Bus Line 5
#define GPIO_DQ6 23  // Data Bus Line 6
#define GPIO_DQ7 18  // Data Bus Line 7

// Control Signal Pins: Manage the flow of data and commands to the NAND flash.
#define GPIO_WP  26  // Write Protect: Prevents writing to the memory array. Active low.
#define GPIO_CLE 11  // Command Latch Enable: Signals that a command is on the data bus. Active high.
#define GPIO_ALE 13  // Address Latch Enable: Signals that an address is on the data bus. Active high.
#define GPIO_RE  27  // Read Enable: Strobes data out of the NAND flash. Active low.
#define GPIO_WE  19  // Write Enable: Strobes data into the NAND flash. Active low.
#define GPIO_CE  22  // Chip Enable: Activates the NAND flash chip. Active low.
#define GPIO_RB  17  // Ready/Busy: Indicates the status of the NAND flash. Low means busy.

// Data Strobe Pins: Used for synchronizing data transfer in synchronous mode.
#define GPIO_DQS  5  // Data Strobe: Toggles to signal valid data.
#define GPIO_DQSC 6  // Data Strobe Complement: The inverse of DQS.

// LED Indicator Pins: Provide visual feedback on the system's status.
#define GPIO_RLED0 14 // Red LED 0
#define GPIO_RLED1 15 // Red LED 1
#define GPIO_RLED2 7  // Red LED 2
#define GPIO_RLED3 8  // Red LED 3


#endif // HARDWARE_LOCATIONS_H
