#include "gpio.h"
#include "timing.h"
#include "onfi_interface.h"
#include "hardware_locations.h"
#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <random>
#include <chrono>

struct BenchmarkResult {
    std::string name;
    double mean;
    double median;
    double stddev;
};

// Function to calculate the mean of a vector of timings.
double calculate_mean(const std::vector<uint64_t>& timings) {
    if (timings.empty()) {
        return 0.0;
    }
    double sum = std::accumulate(timings.begin(), timings.end(), 0.0);
    return sum / timings.size();
}

// Function to calculate the median of a vector of timings.
double calculate_median(std::vector<uint64_t> timings) {
    if (timings.empty()) {
        return 0.0;
    }
    size_t n = timings.size() / 2;
    std::nth_element(timings.begin(), timings.begin() + n, timings.end());
    return timings[n];
}

// Function to calculate the standard deviation of a vector of timings.
double calculate_stddev(const std::vector<uint64_t>& timings) {
    if (timings.size() < 2) {
        return 0.0;
    }
    double mean = calculate_mean(timings);
    double sq_sum = std::inner_product(timings.begin(), timings.end(), timings.begin(), 0.0);
    return std::sqrt(sq_sum / timings.size() - mean * mean);
}

// --- Benchmarking Functions ---

BenchmarkResult benchmark_gpio_init() {
    std::cout << "Benchmarking gpio_init..." << std::endl;
    std::vector<uint64_t> timings;
    for (int i = 0; i < 100; ++i) {
        uint64_t start_time = get_timestamp_ns();
        gpio_init();
        uint64_t end_time = get_timestamp_ns();
        timings.push_back(end_time - start_time);
    }
    return {"gpio_init", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_gpio_set_direction() {
    std::cout << "Benchmarking gpio_set_direction..." << std::endl;
    std::vector<uint64_t> timings;
    for (int i = 0; i < 100; ++i) {
        uint64_t start_time = get_timestamp_ns();
        gpio_set_direction(GPIO_DQ0, true);
        uint64_t end_time = get_timestamp_ns();
        timings.push_back(end_time - start_time);
    }
    return {"gpio_set_direction", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_gpio_set_high() {
    std::cout << "Benchmarking gpio_set_high..." << std::endl;
    std::vector<uint64_t> timings;
    for (int i = 0; i < 100; ++i) {
        uint64_t start_time = get_timestamp_ns();
        gpio_set_high(GPIO_DQ0);
        uint64_t end_time = get_timestamp_ns();
        timings.push_back(end_time - start_time);
    }
    return {"gpio_set_high", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_gpio_set_low() {
    std::cout << "Benchmarking gpio_set_low..." << std::endl;
    std::vector<uint64_t> timings;
    for (int i = 0; i < 100; ++i) {
        uint64_t start_time = get_timestamp_ns();
        gpio_set_low(GPIO_DQ0);
        uint64_t end_time = get_timestamp_ns();
        timings.push_back(end_time - start_time);
    }
    return {"gpio_set_low", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_onfi_init(onfi_interface& onfi) {
    std::cout << "Benchmarking onfi_init..." << std::endl;
    std::vector<uint64_t> timings;
    for (int i = 0; i < 100; ++i) {
        uint64_t start_time = get_timestamp_ns();
        onfi.initialize_onfi();
        uint64_t end_time = get_timestamp_ns();
        timings.push_back(end_time - start_time);
    }
    return {"onfi_init", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_onfi_identify(onfi_interface& onfi) {
    std::cout << "Benchmarking onfi_identify..." << std::endl;
    std::vector<uint64_t> timings;
    for (int i = 0; i < 100; ++i) {
        uint64_t start_time = get_timestamp_ns();
        onfi.read_parameters();
        uint64_t end_time = get_timestamp_ns();
        timings.push_back(end_time - start_time);
    }
    return {"onfi_identify", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_onfi_read_page(onfi_interface& onfi, std::default_random_engine& generator, std::uniform_int_distribution<uint16_t>& block_distribution, std::uniform_int_distribution<uint16_t>& page_distribution) {
    std::cout << "Benchmarking onfi_read_page..." << std::endl;
    std::vector<uint64_t> timings;
    // Create a buffer to store the read data
    std::vector<uint8_t> read_data(onfi.num_bytes_in_page + onfi.num_spare_bytes_in_page);
    for (int i = 0; i < 100; ++i) {
        uint16_t random_block = block_distribution(generator);
        uint16_t random_page = page_distribution(generator);
        uint64_t start_time = get_timestamp_ns();
        onfi.read_page(random_block, random_page);
        onfi.get_data(read_data.data(), read_data.size()); // Get data from cache to host
        uint64_t end_time = get_timestamp_ns();
        timings.push_back(end_time - start_time);
    }
    return {"onfi_read_page", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_onfi_write_page(onfi_interface& onfi, std::default_random_engine& generator, std::uniform_int_distribution<uint16_t>& block_distribution, std::uniform_int_distribution<uint16_t>& page_distribution) {
    std::cout << "Benchmarking onfi_write_page..." << std::endl;
    std::vector<uint64_t> timings;
    uint8_t data[onfi.num_bytes_in_page];
    for (int i = 0; i < 100; ++i) {
        uint16_t random_block = block_distribution(generator);
        uint16_t random_page = page_distribution(generator);
        uint64_t start_time = get_timestamp_ns();
        onfi.program_page(random_block, random_page, data);
        uint64_t end_time = get_timestamp_ns();
        timings.push_back(end_time - start_time);
    }
    return {"onfi_write_page", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

BenchmarkResult benchmark_onfi_erase_block(onfi_interface& onfi, std::default_random_engine& generator, std::uniform_int_distribution<uint16_t>& block_distribution) {
    std::cout << "Benchmarking onfi_erase_block..." << std::endl;
    std::vector<uint64_t> timings;
    for (int i = 0; i < 100; ++i) {
        uint16_t random_block = block_distribution(generator);
        uint64_t start_time = get_timestamp_ns();
        onfi.erase_block(random_block);
        uint64_t end_time = get_timestamp_ns();
        timings.push_back(end_time - start_time);
    }
    return {"onfi_erase_block", calculate_mean(timings), calculate_median(timings), calculate_stddev(timings)};
}

void print_summary(const std::vector<BenchmarkResult>& results) {
    std::cout << "--- Function Profiler Summary ---" << std::endl;
    std::cout << std::left << std::setw(25) << "Function" << std::setw(15) << "Mean (us)" << std::setw(15) << "Median (us)" << std::setw(15) << "Stddev (us)" << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    std::cout << std::fixed << std::setprecision(3);
    for (const auto& result : results) {
        std::cout << std::left << std::setw(25) << result.name << std::setw(15) << result.mean / 1000.0 << std::setw(15) << result.median / 1000.0 << std::setw(15) << result.stddev / 1000.0 << std::endl;
    }
}

void print_chip_info(const onfi_interface& onfi) {
    std::cout << "\n--- ONFI Chip Information ---" << std::endl;
    std::cout << std::left << std::setw(30) << "Parameter" << "Value" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    std::cout << std::left << std::setw(30) << "Bytes per Page:" << onfi.num_bytes_in_page << std::endl;
    std::cout << std::left << std::setw(30) << "Spare Bytes per Page:" << onfi.num_spare_bytes_in_page << std::endl;
    std::cout << std::left << std::setw(30) << "Pages per Block:" << onfi.num_pages_in_block << std::endl;
    std::cout << std::left << std::setw(30) << "Number of Blocks:" << onfi.num_blocks << std::endl;
    std::cout << std::left << std::setw(30) << "Column Address Cycles:" << (int)onfi.num_column_cycles << std::endl;
    std::cout << std::left << std::setw(30) << "Row Address Cycles:" << (int)onfi.num_row_cycles << std::endl;
    std::cout << std::left << std::setw(30) << "Manufacturer ID:" << onfi.manufacturer_id << std::endl;
    std::cout << std::left << std::setw(30) << "Device Model:" << onfi.device_model << std::endl;
    std::cout << std::left << std::setw(30) << "ONFI Version:" << onfi.onfi_version << std::endl;
    std::cout << std::left << std::setw(30) << "Unique ID:" ;
    for (int i = 0; i < 32; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)onfi.unique_id[i];
    }
    std::cout << std::dec << std::endl;
    std::cout << std::string(50, '-') << std::endl;
}

int main() {
    onfi_interface onfi;
    std::vector<BenchmarkResult> results;

    std::cout << "--- Function Profiler ---" << std::endl;
    std::cout << "This tool measures the execution time of various functions." << std::endl;
    std::cout << std::endl;

    // Initialize random number generator
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);

    results.push_back(benchmark_gpio_init());
    results.push_back(benchmark_gpio_set_direction());
    results.push_back(benchmark_gpio_set_high());
    results.push_back(benchmark_gpio_set_low());
    results.push_back(benchmark_onfi_init(onfi));
    results.push_back(benchmark_onfi_identify(onfi));

    // Get chip parameters after identification
    uint16_t num_blocks = onfi.num_blocks;
    uint16_t num_pages_in_block = onfi.num_pages_in_block;

    // Define distributions for random block and page numbers
    std::uniform_int_distribution<uint16_t> block_distribution(0, num_blocks - 1);
    std::uniform_int_distribution<uint16_t> page_distribution(0, num_pages_in_block - 1);

    results.push_back(benchmark_onfi_read_page(onfi, generator, block_distribution, page_distribution));
    results.push_back(benchmark_onfi_write_page(onfi, generator, block_distribution, page_distribution));
    results.push_back(benchmark_onfi_erase_block(onfi, generator, block_distribution));

    std::cout << std::endl;
    print_summary(results);
    print_chip_info(onfi);

    return 0;
}
