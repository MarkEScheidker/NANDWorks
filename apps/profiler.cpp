#include "gpio.hpp"
#include "timing.hpp"
#include "onfi_interface.hpp"
#include "hardware_locations.hpp"

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <exception>
#include <stdexcept>

struct BenchmarkResult {
    std::string name;
    double mean;
    double median;
    double stddev;
};

// Calculate the mean of timing samples.
double calculate_mean(const std::vector<uint64_t>& timings) {
    if (timings.empty()) return 0.0;
    double sum = std::accumulate(timings.begin(), timings.end(), 0.0);
    return sum / static_cast<double>(timings.size());
}

// Calculate the median of timing samples.
double calculate_median(std::vector<uint64_t> timings) {
    if (timings.empty()) return 0.0;
    const size_t mid = timings.size() / 2;
    std::nth_element(timings.begin(), timings.begin() + static_cast<std::vector<uint64_t>::difference_type>(mid), timings.end());
    return static_cast<double>(timings[mid]);
}

// Calculate the population standard deviation of timing samples.
double calculate_stddev(const std::vector<uint64_t>& timings) {
    if (timings.size() < 2) return 0.0;
    const double mean = calculate_mean(timings);
    const double sq_sum = std::inner_product(timings.begin(), timings.end(), timings.begin(), 0.0);
    return std::sqrt(sq_sum / static_cast<double>(timings.size()) - mean * mean);
}

// --- Benchmark helpers ----------------------------------------------------

BenchmarkResult benchmark_gpio_init() {
    std::cout << "Benchmarking gpio_init..." << std::endl;
    std::vector<uint64_t> timings;
    timings.reserve(100);

    // Ensure we start from a clean slate before benchmarking.
    gpio_shutdown();
    for (int i = 0; i < 100; ++i) {
        const uint64_t start = get_timestamp_ns();
        GpioSession session(/*throw_on_failure=*/false);
        if (!session.ok()) {
            timings.push_back(0);
        } else {
            const uint64_t end = get_timestamp_ns();
            timings.push_back(end - start);
        }
        // session goes out of scope here and calls gpio_shutdown()
    }

    return {"gpio_init", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_gpio_set_direction() {
    std::cout << "Benchmarking gpio_set_direction..." << std::endl;
    std::vector<uint64_t> timings;
    timings.reserve(100);
    for (int i = 0; i < 100; ++i) {
        const uint64_t start = get_timestamp_ns();
        gpio_set_direction(GPIO_DQ0, true);
        const uint64_t end = get_timestamp_ns();
        timings.push_back(end - start);
    }
    return {"gpio_set_direction", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_gpio_set_high() {
    std::cout << "Benchmarking gpio_set_high..." << std::endl;
    std::vector<uint64_t> timings;
    timings.reserve(100);
    for (int i = 0; i < 100; ++i) {
        const uint64_t start = get_timestamp_ns();
        gpio_set_high(GPIO_DQ0);
        const uint64_t end = get_timestamp_ns();
        timings.push_back(end - start);
    }
    return {"gpio_set_high", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_gpio_set_low() {
    std::cout << "Benchmarking gpio_set_low..." << std::endl;
    std::vector<uint64_t> timings;
    timings.reserve(100);
    for (int i = 0; i < 100; ++i) {
        const uint64_t start = get_timestamp_ns();
        gpio_set_low(GPIO_DQ0);
        const uint64_t end = get_timestamp_ns();
        timings.push_back(end - start);
    }
    return {"gpio_set_low", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_onfi_init(onfi_interface& onfi) {
    std::cout << "Benchmarking onfi_initialize..." << std::endl;
    std::vector<uint64_t> timings;
    timings.reserve(100);

    onfi.deinitialize_onfi();
    for (int i = 0; i < 100; ++i) {
        const uint64_t start = get_timestamp_ns();
        onfi.initialize_onfi();
        const uint64_t end = get_timestamp_ns();
        timings.push_back(end - start);
        onfi.deinitialize_onfi();
    }

    // Leave the interface initialised for subsequent operations.
    onfi.initialize_onfi();

    return {"onfi_initialize", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_onfi_identify(onfi_interface& onfi) {
    std::cout << "Benchmarking onfi_read_parameters..." << std::endl;
    std::vector<uint64_t> timings;
    timings.reserve(100);

    for (int i = 0; i < 100; ++i) {
        const uint64_t start = get_timestamp_ns();
        onfi.read_parameters(ONFI, true, false);
        const uint64_t end = get_timestamp_ns();
        timings.push_back(end - start);
    }

    return {"onfi_read_parameters", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_onfi_read_page(onfi_interface& onfi,
                                         std::default_random_engine& rng,
                                         std::uniform_int_distribution<uint16_t>& block_dist,
                                         std::uniform_int_distribution<uint16_t>& page_dist) {
    std::cout << "Benchmarking onfi_read_page..." << std::endl;
    std::vector<uint64_t> timings;
    timings.reserve(100);

    const size_t total_bytes = static_cast<size_t>(onfi.num_bytes_in_page) + static_cast<size_t>(onfi.num_spare_bytes_in_page);
    std::vector<uint8_t> buffer(total_bytes);

    for (int i = 0; i < 100; ++i) {
        const uint16_t block = block_dist(rng);
        const uint16_t page = page_dist(rng);
        const uint64_t start = get_timestamp_ns();
        onfi.read_page(block, page);
        onfi.get_data(buffer.data(), static_cast<uint16_t>(buffer.size()));
        const uint64_t end = get_timestamp_ns();
        timings.push_back(end - start);
    }

    return {"onfi_read_page", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_onfi_write_page(onfi_interface& onfi,
                                          std::default_random_engine& rng,
                                          std::uniform_int_distribution<uint16_t>& block_dist,
                                          std::uniform_int_distribution<uint16_t>& page_dist) {
    std::cout << "Benchmarking onfi_program_page..." << std::endl;
    std::vector<uint64_t> timings;
    timings.reserve(100);

    std::vector<uint8_t> data(static_cast<size_t>(onfi.num_bytes_in_page), 0xAA);

    for (int i = 0; i < 100; ++i) {
        const uint16_t block = block_dist(rng);
        const uint16_t page = page_dist(rng);
        const uint64_t start = get_timestamp_ns();
        onfi.program_page(block, page, data.data());
        const uint64_t end = get_timestamp_ns();
        timings.push_back(end - start);
    }

    return {"onfi_program_page", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_onfi_erase_block(onfi_interface& onfi,
                                           std::default_random_engine& rng,
                                           std::uniform_int_distribution<uint16_t>& block_dist) {
    std::cout << "Benchmarking onfi_erase_block..." << std::endl;
    std::vector<uint64_t> timings;
    timings.reserve(100);

    for (int i = 0; i < 100; ++i) {
        const uint16_t block = block_dist(rng);
        const uint64_t start = get_timestamp_ns();
        onfi.erase_block(block);
        const uint64_t end = get_timestamp_ns();
        timings.push_back(end - start);
    }

    return {"onfi_erase_block", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

// --- Reporting helpers ----------------------------------------------------

void print_summary(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n--- Function Profiler Summary ---" << std::endl;
    std::cout << std::left << std::setw(25) << "Function" << std::setw(15) << "Mean (us)"
              << std::setw(15) << "Median (us)" << std::setw(15) << "Stddev (us)" << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    std::cout << std::fixed << std::setprecision(3);
    for (const auto& result : results) {
        std::cout << std::left << std::setw(25) << result.name
                  << std::setw(15) << result.mean / 1000.0
                  << std::setw(15) << result.median / 1000.0
                  << std::setw(15) << result.stddev / 1000.0 << std::endl;
    }
    std::cout << std::setfill(' ');
}

void print_chip_info(const onfi_interface& onfi) {
    std::cout << "\n--- ONFI Chip Information ---" << std::endl;
    std::cout << std::left << std::setw(30) << "Parameter" << "Value" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    std::cout << std::left << std::setw(30) << "Bytes per Page:" << onfi.num_bytes_in_page << std::endl;
    std::cout << std::left << std::setw(30) << "Spare Bytes per Page:" << onfi.num_spare_bytes_in_page << std::endl;
    std::cout << std::left << std::setw(30) << "Pages per Block:" << onfi.num_pages_in_block << std::endl;
    std::cout << std::left << std::setw(30) << "Number of Blocks:" << onfi.num_blocks << std::endl;
    std::cout << std::left << std::setw(30) << "Column Address Cycles:" << static_cast<int>(onfi.num_column_cycles) << std::endl;
    std::cout << std::left << std::setw(30) << "Row Address Cycles:" << static_cast<int>(onfi.num_row_cycles) << std::endl;
    std::cout << std::left << std::setw(30) << "Manufacturer ID:" << onfi.manufacturer_id << std::endl;
    std::cout << std::left << std::setw(30) << "Device Model:" << onfi.device_model << std::endl;
    std::cout << std::left << std::setw(30) << "ONFI Version:" << onfi.onfi_version << std::endl;

    std::cout << std::left << std::setw(30) << "Unique ID:";
    std::cout << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) {
        std::cout << std::setw(2) << static_cast<int>(onfi.unique_id[i]);
    }
    std::cout << std::dec << std::setfill(' ') << std::endl;

    uint8_t timing_mode_feature[4] = {0};
    onfi.get_features(0x01, timing_mode_feature);
    std::cout << std::left << std::setw(30) << "Timing Mode Feature (0x01):";
    std::cout << std::hex << std::setfill('0');
    for (uint8_t value : timing_mode_feature) {
        std::cout << std::setw(2) << static_cast<int>(value) << ' ';
    }
    std::cout << std::dec << std::setfill(' ') << std::endl;
    std::cout << std::string(50, '-') << std::endl;
}

int main() {
    try {
        std::vector<BenchmarkResult> results;

        std::cout << "--- Function Profiler ---" << std::endl;
        std::cout << "This tool measures the execution time of various functions." << std::endl;
        std::cout << std::endl;

        results.push_back(benchmark_gpio_init());

        GpioSession gpio_guard;
        if (!gpio_guard.ok()) {
            throw std::runtime_error("gpio_init failed");
        }

        onfi_interface onfi;

        // Seed RNG for randomized ONFI access benchmarks.
        const unsigned seed = static_cast<unsigned>(
            std::chrono::system_clock::now().time_since_epoch().count());
        std::default_random_engine rng(seed);

        results.push_back(benchmark_gpio_set_direction());
        results.push_back(benchmark_gpio_set_high());
        results.push_back(benchmark_gpio_set_low());
        results.push_back(benchmark_onfi_init(onfi));
        results.push_back(benchmark_onfi_identify(onfi));

        // Fully initialize ONFI stack so geometry values are populated.
        onfi.get_started();

        const uint16_t num_blocks = onfi.num_blocks ? onfi.num_blocks : 1;
        const uint16_t pages_per_block = onfi.num_pages_in_block ? onfi.num_pages_in_block : 1;

        std::uniform_int_distribution<uint16_t> block_dist(0, num_blocks - 1);
        std::uniform_int_distribution<uint16_t> page_dist(0, pages_per_block - 1);

        results.push_back(benchmark_onfi_read_page(onfi, rng, block_dist, page_dist));
        results.push_back(benchmark_onfi_write_page(onfi, rng, block_dist, page_dist));
        results.push_back(benchmark_onfi_erase_block(onfi, rng, block_dist));

        print_summary(results);
        print_chip_info(onfi);
    } catch (const std::exception& ex) {
        std::cerr << "Profiler failed: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
