#include "onfi_interface.hpp"
#include "onfi/controller.hpp"
#include "onfi/device.hpp"
#include "onfi/device_utils.hpp"
#include "onfi/types.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>
#include <limits>

namespace {

unsigned int pick_good_block(onfi_interface &onfi_instance) {
    for (int tries = 0; tries < 16; ++tries) {
        unsigned int candidate = static_cast<unsigned int>(rand() % onfi_instance.num_blocks);
        if (!onfi_instance.is_bad_block(candidate)) return candidate;
    }
    for (unsigned int block = 0; block < onfi_instance.num_blocks; ++block) {
        if (!onfi_instance.is_bad_block(block)) return block;
    }
    return 0;
}

struct MarginObservation {
    uint32_t loop_count;
    size_t mismatched_bytes;
    size_t mismatched_bits;
    double match_ratio;
};

uint8_t count_set_bits(uint8_t value) {
    value = static_cast<uint8_t>(value - ((value >> 1) & 0x55));
    value = static_cast<uint8_t>((value & 0x33) + ((value >> 2) & 0x33));
    return static_cast<uint8_t>((value + (value >> 4)) & 0x0F);
}

MarginObservation summarize_margin(uint32_t loop_count,
                                   const std::vector<uint8_t> &expected,
                                   const std::vector<uint8_t> &actual,
                                   size_t main_bytes) {
    MarginObservation observation{loop_count, 0, 0, 1.0};
    const size_t compare_bytes = std::min(main_bytes, std::min(expected.size(), actual.size()));
    size_t mismatched_bits = 0;
    size_t mismatched_bytes = 0;
    for (size_t i = 0; i < compare_bytes; ++i) {
        const uint8_t diff = static_cast<uint8_t>(expected[i] ^ actual[i]);
        if (diff != 0) {
            ++mismatched_bytes;
            mismatched_bits += count_set_bits(diff);
        }
    }
    observation.mismatched_bytes = mismatched_bytes;
    observation.mismatched_bits = mismatched_bits;
    const size_t total_bits = compare_bytes * 8;
    observation.match_ratio = total_bits ? (1.0 - (static_cast<double>(mismatched_bits) / total_bits)) : 1.0;
    return observation;
}

void report_observations(const char *title, const std::vector<MarginObservation> &observations) {
    if (observations.empty()) return;

    std::cout << "\n" << title << "\n";
    std::cout << "      wait_us  mismatched_bytes  mismatched_bits  match_pct" << std::endl;
    const std::streamsize previous_precision = std::cout.precision();
    for (const auto &obs : observations) {
        std::cout << std::setw(12) << obs.loop_count
                  << std::setw(18) << obs.mismatched_bytes
                  << std::setw(18) << obs.mismatched_bits
                  << std::setw(12) << std::fixed << std::setprecision(2) << (obs.match_ratio * 100.0) << "%"
                  << std::endl;
    }
    std::cout.precision(previous_precision);
    std::cout.unsetf(std::ios::floatfield);
}

uint32_t find_first_threshold(const std::vector<MarginObservation> &observations, bool want_clean) {
    for (const auto &obs : observations) {
        if (want_clean) {
            if (obs.mismatched_bits == 0) return obs.loop_count;
        } else if (obs.mismatched_bits > 0) {
            return obs.loop_count;
        }
    }
    return 0;
}

void print_threshold(const char *label, uint32_t value) {
    std::cout << "  " << label << " : ";
    if (value) {
        std::cout << value << std::endl;
    } else {
        std::cout << "not observed" << std::endl;
    }
}

std::vector<uint32_t> parse_loop_list(const char *arg) {
    std::vector<uint32_t> loops;
    std::stringstream ss(arg);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (token.empty()) continue;
        uint32_t value = static_cast<uint32_t>(std::stoul(token));
        if (value == 0) {
            throw std::invalid_argument("Loop count must be greater than zero");
        }
        loops.push_back(value);
    }
    if (loops.empty()) {
        throw std::invalid_argument("No loop counts parsed from input");
    }
    std::sort(loops.begin(), loops.end());
    loops.erase(std::unique(loops.begin(), loops.end()), loops.end());
    return loops;
}

void usage(const char *prog) {
    std::cout << "Usage: " << prog << " [-v|--verbose] [--seed N] [--loops L1,L2,...] [--block B] [--page P]" << std::endl;
}

} // namespace

int main(int argc, char **argv) {
    bool verbose = false;
    unsigned int seed = static_cast<unsigned int>(time(nullptr));
    std::vector<uint32_t> loop_sweep{10, 20, 30, 40, 50, 75, 100, 125, 150, 175, 200, 250, 300, 350, 400, 450, 500};

    const unsigned int invalid_index = std::numeric_limits<unsigned int>::max();
    unsigned int block_override = invalid_index;
    unsigned int page_override = invalid_index;

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<unsigned int>(strtoul(argv[++i], nullptr, 10));
        } else if (arg == "--loops" && i + 1 < argc) {
            try {
                loop_sweep = parse_loop_list(argv[++i]);
            } catch (const std::exception &ex) {
                std::cerr << "Error parsing --loops: " << ex.what() << std::endl;
                usage(argv[0]);
                return 1;
            }
        } else if (arg == "--block" && i + 1 < argc) {
            block_override = static_cast<unsigned int>(strtoul(argv[++i], nullptr, 10));
        } else if (arg == "--page" && i + 1 < argc) {
            page_override = static_cast<unsigned int>(strtoul(argv[++i], nullptr, 10));
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (verbose) {
        std::cout << "Seeding RNG with: " << seed << std::endl;
        std::cout << "Loop sweep: ";
        for (size_t idx = 0; idx < loop_sweep.size(); ++idx) {
            if (idx) std::cout << ",";
            std::cout << loop_sweep[idx];
        }
        std::cout << std::endl;
    }

    srand(seed);

    onfi_interface onfi_instance;
    std::cout << "\n--- Partial Operation Margin Sweep ---" << std::endl;
    std::cout << "Initializing ONFI interface..." << std::endl;
    onfi_instance.get_started();
    std::cout << "Initialization complete." << std::endl;

    unsigned int block = (block_override != invalid_index) ? block_override : pick_good_block(onfi_instance);
    if (block >= onfi_instance.num_blocks) {
        std::cerr << "Requested block out of range" << std::endl;
        return 1;
    }
    if (onfi_instance.is_bad_block(block)) {
        std::cerr << "Requested block is marked bad; choose another" << std::endl;
        return 1;
    }

    unsigned int page = (page_override != invalid_index) ? page_override : 0; // default to first page
    if (page >= onfi_instance.num_pages_in_block) {
        std::cerr << "Requested page out of range" << std::endl;
        return 1;
    }
    if (verbose) {
        std::cout << "Using block " << block << ", page " << page << std::endl;
    }

    const size_t main_bytes = onfi_instance.num_bytes_in_page;
    const size_t spare_bytes = onfi_instance.num_spare_bytes_in_page;
    const size_t total_bytes = main_bytes + spare_bytes;

    std::vector<uint8_t> program_payload(total_bytes, 0x00);
    for (size_t i = 0; i < main_bytes; ++i) {
        program_payload[i] = static_cast<uint8_t>((i * 17 + 31) & 0xFF);
    }
    std::fill(program_payload.begin() + main_bytes, program_payload.end(), 0xFF);
    std::vector<uint8_t> expected_main(program_payload.begin(), program_payload.begin() + main_bytes);
    std::vector<uint8_t> expected_erased(main_bytes, 0xFF);

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    onfi::populate_device(onfi_instance, dev);

    std::vector<MarginObservation> program_results;
    std::vector<MarginObservation> erase_results;
    std::vector<uint8_t> read_buffer;

    for (uint32_t loop_count : loop_sweep) {
        if (verbose) {
            std::cout << "\nApplying loop_count=" << loop_count << std::endl;
        }

        onfi_instance.erase_block(block, verbose);
        onfi_instance.partial_program_page(block, page, loop_count, program_payload.data(), false, verbose);

        read_buffer.clear();
        dev.read_page(block, page, /*including_spare*/true, /*bytewise*/false, read_buffer);
        program_results.push_back(summarize_margin(loop_count, expected_main, read_buffer, main_bytes));

        onfi_instance.erase_block(block, false);

        for (uint32_t p = 0; p < onfi_instance.num_pages_in_block; ++p) {
            onfi_instance.program_page(block, p, program_payload.data(), false, verbose);
        }
        onfi_instance.partial_erase_block(block, page, loop_count, verbose);

        size_t total_mismatched_bytes = 0;
        size_t total_mismatched_bits = 0;
        for (uint32_t p = 0; p < onfi_instance.num_pages_in_block; ++p) {
            read_buffer.clear();
            dev.read_page(block, p, /*including_spare*/true, /*bytewise*/false, read_buffer);
            const MarginObservation obs = summarize_margin(loop_count, expected_erased, read_buffer, main_bytes);
            total_mismatched_bytes += obs.mismatched_bytes;
            total_mismatched_bits += obs.mismatched_bits;
        }
        const size_t total_bits = static_cast<size_t>(onfi_instance.num_pages_in_block) * main_bytes * 8ULL;
        const double match_ratio = total_bits ? (1.0 - (static_cast<double>(total_mismatched_bits) / total_bits)) : 1.0;
        erase_results.push_back(MarginObservation{loop_count, total_mismatched_bytes, total_mismatched_bits, match_ratio});

        onfi_instance.erase_block(block, false);
    }

    report_observations("Partial program margin sweep", program_results);
    report_observations("Partial erase margin sweep", erase_results);

    const uint32_t program_first_effect = find_first_threshold(program_results, false);
    const uint32_t program_first_clean = find_first_threshold(program_results, true);
    const uint32_t erase_first_effect = find_first_threshold(erase_results, false);
    const uint32_t erase_first_clean = find_first_threshold(erase_results, true);

    std::cout << "\nPartial program thresholds:" << std::endl;
    print_threshold("First deviation wait_us", program_first_effect);
    print_threshold("First clean wait_us", program_first_clean);

    std::cout << "\nPartial erase thresholds:" << std::endl;
    print_threshold("First deviation wait_us", erase_first_effect);
    print_threshold("First clean wait_us", erase_first_clean);

    std::cout << "\nSweep complete." << std::endl;
    return 0;
}
