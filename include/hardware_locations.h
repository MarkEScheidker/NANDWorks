#ifndef HARDWARE_LOCATIONS
#define HARDWARE_LOCATIONS

#include <fstream>
#include <pigpio.h>

#define PROFILE_TIME true

#define FORCE_INLINE __attribute__((always_inline))

// Timing macros using pigpio
#define START_TIME do { uint32_t start_tick = gpioTick();
#define END_TIME   uint32_t end_tick = gpioTick(); time_info_file << "  took " << (end_tick - start_tick) << " microseconds\n"; } while(0);
#define PRINT_TIME // Handled within END_TIME
#define GET_TIME_ELAPSED (end_tick - start_tick)

// Placeholder BCM GPIO pin numbers (adjust as per your wiring)
#define DQ0_PIN 2
#define DQ1_PIN 3
#define DQ2_PIN 4
#define DQ3_PIN 17
#define DQ4_PIN 27
#define DQ5_PIN 22
#define DQ6_PIN 10
#define DQ7_PIN 9

#define WP_PIN 11
#define CLE_PIN 5
#define ALE_PIN 6
#define RE_PIN 13
#define WE_PIN 19
#define CE_PIN 26
#define RB_PIN 21
#define DQS_PIN 20
#define DQSc_PIN 16

// Red LEDs (adjust as per your wiring)
#define RLED0_PIN 14
#define RLED1_PIN 15
#define RLED2_PIN 18
#define RLED3_PIN 23

// Timing delays in microseconds (adjust as needed based on NAND datasheet)
// These are rough estimates and may need fine-tuning
#define SAMPLE_TIME gpioDelay(1); // ~1us, adjust for tDS
#define HOLD_TIME gpioDelay(1); // ~1us, adjust for tDH
#define tWW gpioDelay(100); // 100ns -> 0.1us, using 1us for safety
#define tWB gpioDelay(1); // 200ns -> 0.2us, using 1us for safety
#define tRR gpioDelay(1); // 40ns -> 0.04us, using 1us for safety
#define tRHW gpioDelay(1); // 200ns -> 0.2us, using 1us for safety
#define tCCS gpioDelay(1); // 200ns -> 0.2us, using 1us for safety
#define tADL gpioDelay(1); // 300ns -> 0.3us, using 1us for safety
#define tWHR gpioDelay(1); // 120ns -> 0.12us, using 1us for safety

#endif