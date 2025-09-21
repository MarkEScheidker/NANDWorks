#include "gpio.hpp"
#include "timing.hpp"
#include "onfi_interface.hpp"
#include "hardware_locations.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct BenchmarkResult {
    std::string name;
    double mean;
    double median;
    double stddev;
    std::size_t samples;
};

struct ProfilerConfig {
    std::size_t iterations = 100;
    bool include_gpio = true;
    bool include_onfi = true;
    bool include_destructive = false;
    bool compare_bytewise_parameters = false;
    bool cleanup_after_destructive = true;
    std::optional<uint16_t> block_override;
    std::optional<uint16_t> page_override;
};

// ---------------------------------------------------------------------------
// Utility helpers
// ---------------------------------------------------------------------------

double calculate_mean(const std::vector<uint64_t>& timings) {
    if (timings.empty()) return 0.0;
    const double sum = std::accumulate(timings.begin(), timings.end(), 0.0);
    return sum / static_cast<double>(timings.size());
}

double calculate_median(std::vector<uint64_t> timings) {
    if (timings.empty()) return 0.0;
    const std::size_t mid = timings.size() / 2;
    std::nth_element(timings.begin(), timings.begin() + static_cast<std::vector<uint64_t>::difference_type>(mid), timings.end());
    return static_cast<double>(timings[mid]);
}

double calculate_stddev(const std::vector<uint64_t>& timings) {
    if (timings.size() < 2) return 0.0;
    const double mean = calculate_mean(timings);
    const double sq_sum = std::inner_product(timings.begin(), timings.end(), timings.begin(), 0.0);
    return std::sqrt(sq_sum / static_cast<double>(timings.size()) - mean * mean);
}

void print_usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [options]\n"
              << "\nOptions:\n"
              << "  --iterations N            Number of samples per benchmark (default: 100)\n"
              << "  --skip-gpio               Skip GPIO micro-benchmarks\n"
              << "  --skip-onfi               Skip ONFI benchmarking entirely\n"
              << "  --include-destructive     Measure program/erase operations (writes NAND)\n"
              << "  --no-cleanup              Leave programmed data in place after destructive tests\n"
              << "  --block N                 Target block for destructive ONFI benchmarks\n"
              << "  --page N                  Starting page within the target block\n"
              << "  --compare-bytewise        Also profile bytewise ONFI parameter reads\n"
              << "  --help                    Show this message\n";
}

ProfilerConfig parse_arguments(int argc, char** argv) {
    ProfilerConfig config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--iterations") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --iterations");
            config.iterations = static_cast<std::size_t>(std::stoul(argv[++i]));
            if (config.iterations == 0) throw std::runtime_error("Iterations must be greater than zero");
        } else if (arg == "--skip-gpio") {
            config.include_gpio = false;
        } else if (arg == "--skip-onfi") {
            config.include_onfi = false;
        } else if (arg == "--include-destructive") {
            config.include_destructive = true;
        } else if (arg == "--no-cleanup") {
            config.cleanup_after_destructive = false;
        } else if (arg == "--block") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --block");
            config.block_override = static_cast<uint16_t>(std::stoul(argv[++i]));
        } else if (arg == "--page") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --page");
            config.page_override = static_cast<uint16_t>(std::stoul(argv[++i]));
        } else if (arg == "--compare-bytewise") {
            config.compare_bytewise_parameters = true;
        } else if (arg == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::ostringstream oss;
            oss << "Unknown option: " << arg;
            throw std::runtime_error(oss.str());
        }
    }
    return config;
}

template <typename Callable>
BenchmarkResult run_benchmark(const std::string& name, std::size_t iterations, Callable&& callable) {
    std::vector<uint64_t> timings;
    timings.reserve(iterations);
    for (std::size_t i = 0; i < iterations; ++i) {
        const uint64_t start = get_timestamp_ns();
        callable(i);
        const uint64_t end = get_timestamp_ns();
        timings.push_back(end - start);
    }
    return {name, calculate_mean(timings), calculate_median(timings), calculate_stddev(timings), timings.size()};
}

void print_summary(const std::vector<BenchmarkResult>& results) {
    if (results.empty()) {
        std::cout << "No benchmarks executed." << std::endl;
        return;
    }

    std::cout << "\n--- Function Profiler Summary ---" << std::endl;
    std::cout << std::left << std::setw(28) << "Function"
              << std::setw(12) << "Samples"
              << std::setw(15) << "Mean (us)"
              << std::setw(15) << "Median (us)"
              << std::setw(15) << "Stddev (us)" << std::endl;
    std::cout << std::string(85, '-') << std::endl;

    std::cout << std::fixed << std::setprecision(3);
    for (const auto& result : results) {
        std::cout << std::left << std::setw(28) << result.name
                  << std::setw(12) << result.samples
                  << std::setw(15) << result.mean / 1000.0
                  << std::setw(15) << result.median / 1000.0
                  << std::setw(15) << result.stddev / 1000.0
                  << std::endl;
    }
    std::cout << std::setfill(' ');
}

void print_chip_info(const onfi_interface& onfi) {
    std::cout << "\n--- ONFI Chip Information ---" << std::endl;
    std::cout << std::left << std::setw(32) << "Parameter" << "Value" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    std::cout << std::left << std::setw(32) << "Bytes per Page:" << onfi.num_bytes_in_page << std::endl;
    std::cout << std::left << std::setw(32) << "Spare Bytes per Page:" << onfi.num_spare_bytes_in_page << std::endl;
    std::cout << std::left << std::setw(32) << "Pages per Block:" << onfi.num_pages_in_block << std::endl;
    std::cout << std::left << std::setw(32) << "Number of Blocks:" << onfi.num_blocks << std::endl;
    std::cout << std::left << std::setw(32) << "Column Address Cycles:" << static_cast<int>(onfi.num_column_cycles) << std::endl;
    std::cout << std::left << std::setw(32) << "Row Address Cycles:" << static_cast<int>(onfi.num_row_cycles) << std::endl;
    std::cout << std::left << std::setw(32) << "Manufacturer ID:" << onfi.manufacturer_id << std::endl;
    std::cout << std::left << std::setw(32) << "Device Model:" << onfi.device_model << std::endl;
    std::cout << std::left << std::setw(32) << "ONFI Version:" << onfi.onfi_version << std::endl;

    std::cout << std::left << std::setw(32) << "Unique ID:";
    std::cout << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) {
        std::cout << std::setw(2) << static_cast<unsigned int>(static_cast<uint8_t>(onfi.unique_id[i])) << ' ';
    }
    std::cout << std::dec << std::setfill(' ') << std::endl;

    uint8_t timing_mode_feature[4] = {0};
    onfi.get_features(0x01, timing_mode_feature);
    std::cout << std::left << std::setw(32) << "Timing Mode Feature (0x01):";
    std::cout << std::hex << std::setfill('0');
    for (uint8_t value : timing_mode_feature) {
        std::cout << std::setw(2) << static_cast<unsigned int>(value) << ' ';
    }
    std::cout << std::dec << std::setfill(' ') << std::endl;
    std::cout << std::string(60, '-') << std::endl;
}

// ---------------------------------------------------------------------------
// GPIO benchmarks
// ---------------------------------------------------------------------------

BenchmarkResult benchmark_gpio_init(std::size_t iterations) {
    std::cout << "Benchmarking gpio_init..." << std::endl;
    return run_benchmark("gpio_init", iterations, [](std::size_t) {
        const uint64_t start = get_timestamp_ns();
        (void)start; // quiet unused warning if assertions disabled
        gpio_shutdown();
        GpioSession session(/*throw_on_failure=*/false);
        if (!session.ok()) {
            throw std::runtime_error("gpio_init failed during benchmark");
        }
        // Session destructor handles shutdown.
    });
}

BenchmarkResult benchmark_gpio_set_direction(std::size_t iterations, uint8_t pin) {
    std::cout << "Benchmarking gpio_set_direction..." << std::endl;
    return run_benchmark("gpio_set_direction", iterations, [pin](std::size_t i) {
        const bool output = (i % 2) == 0;
        gpio_set_direction(pin, output);
    });
}

BenchmarkResult benchmark_gpio_set_high(std::size_t iterations, uint8_t pin) {
    std::cout << "Benchmarking gpio_set_high..." << std::endl;
    return run_benchmark("gpio_set_high", iterations, [pin](std::size_t) {
        gpio_set_high(pin);
    });
}

BenchmarkResult benchmark_gpio_set_low(std::size_t iterations, uint8_t pin) {
    std::cout << "Benchmarking gpio_set_low..." << std::endl;
    return run_benchmark("gpio_set_low", iterations, [pin](std::size_t) {
        gpio_set_low(pin);
    });
}

// ---------------------------------------------------------------------------
// ONFI benchmarks (non-destructive)
// ---------------------------------------------------------------------------

BenchmarkResult benchmark_onfi_get_started(onfi_interface& onfi, std::size_t iterations) {
    std::cout << "Benchmarking onfi_get_started..." << std::endl;
    onfi.deinitialize_onfi();
    return run_benchmark("onfi_get_started", iterations, [&](std::size_t) {
        onfi.get_started();
        onfi.deinitialize_onfi();
    });
}

BenchmarkResult benchmark_onfi_read_parameters(onfi_interface& onfi,
                                               std::size_t iterations,
                                               param_type page_type,
                                               bool bytewise) {
    std::cout << "Benchmarking onfi_read_parameters (" << (bytewise ? "bytewise" : "bulk") << ")..." << std::endl;
    return run_benchmark(bytewise ? "onfi_read_parameters_byte" : "onfi_read_parameters_bulk",
                         iterations,
                         [&](std::size_t) {
                             onfi.read_parameters(page_type, bytewise, false);
                         });
}

BenchmarkResult benchmark_onfi_read_id(onfi_interface& onfi, std::size_t iterations) {
    std::cout << "Benchmarking onfi_read_id..." << std::endl;
    return run_benchmark("onfi_read_id", iterations, [&](std::size_t) {
        onfi.read_id();
    });
}

BenchmarkResult benchmark_onfi_get_features(onfi_interface& onfi,
                                            std::size_t iterations,
                                            uint8_t feature_address) {
    std::cout << "Benchmarking onfi_get_features (0x" << std::hex << static_cast<int>(feature_address)
              << std::dec << ")..." << std::endl;
    return run_benchmark("onfi_get_features", iterations, [&](std::size_t) {
        uint8_t data[4] = {0};
        onfi.get_features(feature_address, data);
    });
}

BenchmarkResult benchmark_onfi_set_features(onfi_interface& onfi,
                                            std::size_t iterations,
                                            uint8_t feature_address,
                                            const uint8_t (&payload)[4]) {
    std::cout << "Benchmarking onfi_set_features (0x" << std::hex << static_cast<int>(feature_address)
              << std::dec << ")..." << std::endl;
    return run_benchmark("onfi_set_features", iterations, [&](std::size_t) {
        onfi.set_features(feature_address, payload);
    });
}

BenchmarkResult benchmark_onfi_read_page(onfi_interface& onfi,
                                         std::size_t iterations,
                                         std::default_random_engine& rng,
                                         std::uniform_int_distribution<uint16_t>& block_dist,
                                         std::uniform_int_distribution<uint16_t>& page_dist) {
    std::cout << "Benchmarking onfi_read_page..." << std::endl;
    const std::size_t total_bytes = static_cast<std::size_t>(onfi.num_bytes_in_page) + onfi.num_spare_bytes_in_page;
    std::vector<uint8_t> buffer(total_bytes);
    return run_benchmark("onfi_read_page", iterations, [&](std::size_t) {
        const uint16_t block = block_dist(rng);
        const uint16_t page = page_dist(rng);
        onfi.read_page(block, page);
        onfi.get_data(buffer.data(), static_cast<uint16_t>(buffer.size()));
    });
}

BenchmarkResult benchmark_onfi_change_read_column(onfi_interface& onfi,
                                                  std::size_t iterations,
                                                  uint16_t block,
                                                  uint16_t page) {
    std::cout << "Benchmarking onfi_change_read_column..." << std::endl;
    // Prime cache with a page read so column changes are valid.
    onfi.read_page(block, page);
    std::vector<uint8_t> scratch(16);
    const uint16_t max_offset = static_cast<uint16_t>(std::max<int>(0, onfi.num_bytes_in_page - static_cast<int>(scratch.size())));
    return run_benchmark("onfi_change_read_column", iterations, [&](std::size_t iteration) {
        const uint16_t offset = static_cast<uint16_t>((iteration * 16) % (max_offset + 1));
        uint8_t col_address[2] = { static_cast<uint8_t>(offset & 0xFF), static_cast<uint8_t>((offset >> 8) & 0xFF) };
        onfi.change_read_column(col_address);
        onfi.get_data(scratch.data(), static_cast<uint16_t>(scratch.size()));
    });
}

BenchmarkResult benchmark_onfi_verify_page(onfi_interface& onfi,
                                           std::size_t iterations,
                                           uint16_t block,
                                           uint16_t page) {
    std::cout << "Benchmarking onfi_verify_program_page..." << std::endl;
    std::vector<uint8_t> expected(onfi.num_bytes_in_page);
    onfi.read_page(block, page);
    onfi.get_data(expected.data(), static_cast<uint16_t>(expected.size()));
    return run_benchmark("onfi_verify_program_page", iterations, [&](std::size_t) {
        onfi.verify_program_page(block, page, expected.data(), false);
    });
}

// ---------------------------------------------------------------------------
// ONFI benchmarks (destructive)
// ---------------------------------------------------------------------------

BenchmarkResult benchmark_onfi_program_page(onfi_interface& onfi,
                                            std::size_t iterations,
                                            uint16_t block,
                                            uint16_t start_page,
                                            uint16_t pages_per_block,
                                            const std::vector<uint8_t>& payload,
                                            std::vector<uint16_t>& touched_pages) {
    std::cout << "Benchmarking onfi_program_page..." << std::endl;
    if (pages_per_block == 0) {
        throw std::runtime_error("pages_per_block is zero; cannot benchmark program_page");
    }
    const std::size_t effective_iterations = std::min<std::size_t>(iterations, pages_per_block);
    touched_pages.clear();
    touched_pages.reserve(effective_iterations);

    onfi.erase_block(block);

    auto result = run_benchmark("onfi_program_page", effective_iterations, [&](std::size_t iteration) {
        const uint16_t page = static_cast<uint16_t>((start_page + iteration) % pages_per_block);
        touched_pages.push_back(page);
        onfi.program_page(block, page, const_cast<uint8_t*>(payload.data()));
    });

    return result;
}

BenchmarkResult benchmark_onfi_erase_block(onfi_interface& onfi,
                                           std::size_t iterations,
                                           uint16_t block) {
    std::cout << "Benchmarking onfi_erase_block..." << std::endl;
    return run_benchmark("onfi_erase_block", iterations, [&](std::size_t) {
        onfi.erase_block(block);
    });
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    try {
        const ProfilerConfig config = parse_arguments(argc, argv);
        std::vector<BenchmarkResult> results;
        std::vector<std::string> notes;

        std::cout << "--- Function Profiler ---" << std::endl;
        std::cout << "Iterations per benchmark: " << config.iterations << std::endl;
        if (!config.include_destructive) {
            notes.emplace_back("Destructive program/erase benchmarks skipped (enable with --include-destructive).");
        }

        if (config.include_gpio) {
            results.push_back(benchmark_gpio_init(config.iterations));
        }

        GpioSession gpio_guard;
        if (!gpio_guard.ok()) {
            throw std::runtime_error("gpio_init failed; cannot continue");
        }

        if (config.include_gpio) {
            results.push_back(benchmark_gpio_set_direction(config.iterations, GPIO_DQ0));
            results.push_back(benchmark_gpio_set_high(config.iterations, GPIO_DQ0));
            results.push_back(benchmark_gpio_set_low(config.iterations, GPIO_DQ0));
        }

        onfi_interface onfi;

        if (config.include_onfi) {
            results.push_back(benchmark_onfi_get_started(onfi, config.iterations));
            onfi.get_started();

            const bool toshiba_toggle = (onfi.flash_chip == toshiba_tlc_toggle);
            const param_type page_type = toshiba_toggle ? JEDEC : ONFI;

            results.push_back(benchmark_onfi_read_parameters(onfi, config.iterations, page_type, false));
            if (config.compare_bytewise_parameters) {
                notes.emplace_back("Bytewise parameter page reads are significantly slower; included per request.");
                results.push_back(benchmark_onfi_read_parameters(onfi, std::max<std::size_t>(10, config.iterations / 10), page_type, true));
            }

            results.push_back(benchmark_onfi_read_id(onfi, config.iterations));

            uint8_t feature_payload[4] = {0};
            onfi.get_features(0x01, feature_payload);
            results.push_back(benchmark_onfi_get_features(onfi, config.iterations, 0x01));
            results.push_back(benchmark_onfi_set_features(onfi, config.iterations, 0x01, feature_payload));

            const uint16_t safe_block = onfi.num_blocks > 0
                ? std::min<uint16_t>(config.block_override.value_or(onfi.num_blocks - 1), onfi.num_blocks - 1)
                : 0;
            const uint16_t safe_page = onfi.num_pages_in_block > 0
                ? std::min<uint16_t>(config.page_override.value_or(0), onfi.num_pages_in_block - 1)
                : 0;

            if (onfi.num_blocks == 0 || onfi.num_pages_in_block == 0) {
                throw std::runtime_error("ONFI geometry not initialised; aborting");
            }

            const unsigned seed = static_cast<unsigned>(
                std::chrono::system_clock::now().time_since_epoch().count());
            std::default_random_engine rng(seed);
            std::uniform_int_distribution<uint16_t> block_dist(0, onfi.num_blocks - 1);
            std::uniform_int_distribution<uint16_t> page_dist(0, onfi.num_pages_in_block - 1);

            results.push_back(benchmark_onfi_read_page(onfi, config.iterations, rng, block_dist, page_dist));
            results.push_back(benchmark_onfi_change_read_column(onfi, config.iterations, safe_block, safe_page));
            results.push_back(benchmark_onfi_verify_page(onfi, config.iterations, safe_block, safe_page));

            if (config.include_destructive) {
                std::vector<uint8_t> payload(static_cast<std::size_t>(onfi.num_bytes_in_page), 0xAA);
                std::vector<uint16_t> touched_pages;
                auto program_result = benchmark_onfi_program_page(onfi, config.iterations, safe_block, safe_page,
                                                                  onfi.num_pages_in_block, payload, touched_pages);
                results.push_back(program_result);

                if (!touched_pages.empty() && config.cleanup_after_destructive) {
                    onfi.erase_block(safe_block);
                }

                results.push_back(benchmark_onfi_erase_block(onfi, config.iterations, safe_block));

                if (!config.cleanup_after_destructive) {
                    notes.emplace_back("Cleanup disabled: block " + std::to_string(safe_block) +
                                       " may contain programmed pages.");
                }
            }
        }

        print_summary(results);

        if (config.include_onfi) {
            print_chip_info(onfi);
        }

        if (!notes.empty()) {
            std::cout << "\nNotes:" << std::endl;
            for (const auto& note : notes) {
                std::cout << "  - " << note << std::endl;
            }
        }

    } catch (const std::exception& ex) {
        std::cerr << "Profiler failed: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}

