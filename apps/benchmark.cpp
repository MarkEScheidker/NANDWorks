#include "gpio.h"
#include "timing.h"
#include <iostream>
#include <bcm2835.h>

#define BENCHMARK_PIN RPI_V2_GPIO_P1_07 // GPIO 4
#define NUM_CYCLES 1000000
#define TARGET_HALF_PERIOD_NS 50 // Target 10 MHz

int main() {
    if (!gpio_init()) {
        return 1;
    }

    gpio_set_direction(BENCHMARK_PIN, true);

    std::cout << "--- GPIO Benchmark (Variable Delay) ---" << std::endl;
    std::cout << "This benchmark measures the actual GPIO toggle frequency for various software delay settings." << std::endl;
    std::cout << "'Half-Period Cycles' refers to the number of CPU cycles used in a busy-wait loop for each half of the toggle period." << std::endl;
    std::cout << "A value of 0 for 'Half-Period Cycles' represents the maximum achievable frequency with minimal software overhead." << std::endl;
    std::cout << "Target Frequency: " << (1e9 / (2 * TARGET_HALF_PERIOD_NS)) / 1e6 << " MHz" << std::endl;

    for (int half_period_cycles = 0; half_period_cycles <= 100; half_period_cycles += 5) {
        uint64_t start_time = get_timestamp_ns();

        for (int i = 0; i < NUM_CYCLES; ++i) {
            gpio_set_high(BENCHMARK_PIN);
            busy_wait_cycles(half_period_cycles);
            gpio_set_low(BENCHMARK_PIN);
            busy_wait_cycles(half_period_cycles);
        }

        uint64_t end_time = get_timestamp_ns();

        uint64_t elapsed_ns = end_time - start_time;
        double elapsed_s = elapsed_ns / 1e9;
        double actual_freq = NUM_CYCLES / elapsed_s;

        std::cout << "Half-Period Cycles: " << half_period_cycles << ", Actual Frequency: " << actual_freq / 1e6 << " MHz" << std::endl;
    }

    return 0;
}
