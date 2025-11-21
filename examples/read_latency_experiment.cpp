#include "onfi_interface.hpp"
#include "onfi/timed_commands.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Range {
    unsigned start = 0;
    unsigned count = 0;
    bool specified = false;
};

struct ExperimentConfig {
    Range blocks{};
    Range pages{};
    std::string output_path;
};

struct MeasurementRow {
    unsigned block = 0;
    unsigned page = 0;
    double t_read_us = std::numeric_limits<double>::quiet_NaN();
    uint8_t status = 0;
    std::string error;
};

bool parse_unsigned(const std::string& token, unsigned& out) {
    if (token.empty()) {
        return false;
    }
    std::size_t idx = 0;
    try {
        const unsigned long value = std::stoul(token, &idx, 0);
        if (idx != token.size() || value > std::numeric_limits<unsigned>::max()) {
            return false;
        }
        out = static_cast<unsigned>(value);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parse_range(const std::string& arg, Range& range) {
    const std::size_t sep = arg.find_first_of(":,");
    const std::string first = arg.substr(0, sep);
    unsigned start = 0;
    if (!parse_unsigned(first, start)) {
        return false;
    }
    unsigned count = 1;
    if (sep != std::string::npos) {
        const std::string second = arg.substr(sep + 1);
        if (!parse_unsigned(second, count)) {
            return false;
        }
    }
    if (count == 0) {
        return false;
    }
    range.start = start;
    range.count = count;
    range.specified = true;
    return true;
}

void print_usage(const char* argv0) {
    std::cout << "Usage: sudo " << argv0
              << " [--blocks start[:count]] [--pages start[:count]]\n"
                 "                [--output file.csv]\n\n"
                 "Times ONFI page reads (tR) across the selected region; defaults to the full device.\n";
}

std::string describe_error(const onfi::timed::OperationTiming& timing) {
    if (!timing.busy_detected) {
        return "rb_not_asserted";
    }
    if (timing.timed_out) {
        return "rb_timeout";
    }
    if (!timing.ready) {
        return "status_not_ready";
    }
    if (!timing.pass) {
        return "status_fail";
    }
    return {};
}

bool parse_args(int argc, char** argv, ExperimentConfig& config, bool& show_help) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--blocks") {
            if (i + 1 >= argc || !parse_range(argv[++i], config.blocks)) {
                std::cerr << "Invalid --blocks argument\n";
                return false;
            }
        } else if (arg == "--pages") {
            if (i + 1 >= argc || !parse_range(argv[++i], config.pages)) {
                std::cerr << "Invalid --pages argument\n";
                return false;
            }
        } else if (arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "--output requires a path\n";
                return false;
            }
            config.output_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            show_help = true;
            return false;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }
    return true;
}

void ensure_range_fits(const Range& range, unsigned max_value, const char* label) {
    if (range.start >= max_value) {
        std::ostringstream oss;
        oss << label << " start " << range.start << " exceeds geometry (" << max_value << ")";
        throw std::out_of_range(oss.str());
    }
    if (range.start + range.count > max_value) {
        std::ostringstream oss;
        oss << label << " span (" << range.start << "+" << range.count << ") exceeds geometry (" << max_value << ")";
        throw std::out_of_range(oss.str());
    }
}

void write_results(std::ostream& out, const std::vector<MeasurementRow>& rows) {
    out << "block,page,t_read_us,status,error\n";
    out << std::fixed << std::setprecision(3);
    for (const auto& row : rows) {
        out << row.block << ',' << row.page << ',';
        if (!row.error.empty() || !std::isfinite(row.t_read_us)) {
            out << ',';
        } else {
            out << row.t_read_us << ',';
        }
        out << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(row.status) << std::nouppercase << std::dec << std::setfill(' ') << ','
            << row.error << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    ExperimentConfig config;
    bool show_help = false;
    if (!parse_args(argc, argv, config, show_help)) {
        print_usage(argv[0]);
        return show_help ? 0 : 1;
    }

    try {
        onfi_interface onfi;
        onfi.get_started();

        if (!config.blocks.specified) {
            config.blocks.start = 0;
            config.blocks.count = onfi.num_blocks;
        }
        if (!config.pages.specified) {
            config.pages.start = 0;
            config.pages.count = onfi.num_pages_in_block;
        }

        ensure_range_fits(config.blocks, onfi.num_blocks, "Block");
        ensure_range_fits(config.pages, onfi.num_pages_in_block, "Page");

        std::ostream* stream = &std::cout;
        std::ofstream output_file;
        if (!config.output_path.empty()) {
            output_file.open(config.output_path, std::ios::out | std::ios::trunc);
            if (!output_file) {
                std::cerr << "Failed to open " << config.output_path << " for writing\n";
                return 1;
            }
            stream = &output_file;
        }

        std::vector<MeasurementRow> rows;
        rows.reserve(static_cast<std::size_t>(config.blocks.count) * config.pages.count);
        std::size_t success = 0;

        for (unsigned b = 0; b < config.blocks.count; ++b) {
            const unsigned block = config.blocks.start + b;
            for (unsigned p = 0; p < config.pages.count; ++p) {
                const unsigned page = config.pages.start + p;

                MeasurementRow row{};
                row.block = block;
                row.page = page;
                try {
                    const auto timing = onfi::timed::read_page(onfi,
                                                               block,
                                                               page,
                                                               nullptr,
                                                               0,
                                                               false,
                                                               false,
                                                               false);
                    row.status = timing.status;
                    row.error = describe_error(timing);
                    if (row.error.empty()) {
                        row.t_read_us = static_cast<double>(timing.duration_ns) / 1000.0;
                        ++success;
                    }
                } catch (const std::exception& ex) {
                    row.error = ex.what();
                }
                rows.push_back(row);
            }
        }

        write_results(*stream, rows);
        std::cerr << "Measured " << rows.size() << " pages (" << success << " succeeded)\n";
    } catch (const std::exception& ex) {
        std::cerr << "Experiment failed: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
