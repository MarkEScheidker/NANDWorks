#include "nandworks/commands/onfi.hpp"
#include "nandworks/cli_parser.hpp"
#include "nandworks/command_context.hpp"
#include "nandworks/driver_context.hpp"
#include "gpio.hpp"
#include "onfi/controller.hpp"
#include "onfi/device.hpp"
#include "onfi/device_config.hpp"
#include "onfi/timed_commands.hpp"
#include "onfi_interface.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace nandworks::commands {
namespace {

void configure_device(onfi_interface& source, onfi::NandDevice& device) {
    const auto config = onfi::make_device_config(source);
    onfi::apply_device_config(config, device);
}

struct GeometrySummary {
    uint32_t page_bytes = 0;
    uint32_t spare_bytes = 0;
    uint32_t pages_per_block = 0;
    uint32_t blocks = 0;
};

GeometrySummary summarize_geometry(const onfi_interface& onfi) {
    GeometrySummary summary;
    summary.page_bytes = onfi.num_bytes_in_page;
    summary.spare_bytes = onfi.num_spare_bytes_in_page;
    summary.pages_per_block = onfi.num_pages_in_block;
    summary.blocks = onfi.num_blocks;
    return summary;
}

uint8_t parse_byte_token(const std::string& token) {
    std::size_t idx = 0;
    int value = 0;
    try {
        value = std::stoi(token, &idx, 0);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid byte token: '" + token + "'");
    }
    if (idx != token.size() || value < 0 || value > 0xFF) {
        throw std::invalid_argument("Invalid byte token: '" + token + "'");
    }
    return static_cast<uint8_t>(value);
}

uint16_t parse_u16_token(const std::string& token) {
    std::size_t idx = 0;
    long value = 0;
    try {
        value = std::stol(token, &idx, 0);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid index token: '" + token + "'");
    }
    if (idx != token.size() || value < 0 || value > 0xFFFF) {
        throw std::invalid_argument("Invalid index token: '" + token + "'");
    }
    return static_cast<uint16_t>(value);
}

std::vector<std::string> split_list(const std::string& spec, char delimiter = ',') {
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : spec) {
        if (ch == delimiter) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else if (!std::isspace(static_cast<unsigned char>(ch))) {
            current.push_back(ch);
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

std::vector<uint16_t> parse_page_list(const std::string& spec, uint32_t pages_per_block) {
    std::vector<uint16_t> indices;
    for (const auto& token : split_list(spec)) {
        auto dash = token.find('-');
        if (dash != std::string::npos) {
            uint16_t start = parse_u16_token(token.substr(0, dash));
            uint16_t end = parse_u16_token(token.substr(dash + 1));
            if (start > end) {
                throw std::invalid_argument("Invalid page range '" + token + "'");
            }
            for (uint16_t i = start; i <= end; ++i) {
                if (i >= pages_per_block) {
                    throw std::invalid_argument("Page index out of range: " + std::to_string(i));
                }
                indices.push_back(i);
            }
        } else {
            uint16_t value = parse_u16_token(token);
            if (value >= pages_per_block) {
                throw std::invalid_argument("Page index out of range: " + std::to_string(value));
            }
            indices.push_back(value);
        }
    }
    return indices;
}

std::array<uint8_t, 4> parse_feature_payload(const std::string& spec) {
    std::array<uint8_t, 4> data{0, 0, 0, 0};
    auto tokens = split_list(spec);
    if (!tokens.empty() && tokens.size() != 4) {
        throw std::invalid_argument("Feature data must contain exactly four bytes");
    }
    if (!tokens.empty()) {
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            data[i] = parse_byte_token(tokens[i]);
        }
    }
    return data;
}

void ensure_block_in_range(const onfi_interface& onfi, int64_t block) {
    if (block < 0 || block >= onfi.num_blocks) {
        throw std::invalid_argument("Block index out of range");
    }
}

void ensure_page_in_range(const onfi_interface& onfi, int64_t page) {
    if (page < 0 || page >= onfi.num_pages_in_block) {
        throw std::invalid_argument("Page index out of range");
    }
}

std::vector<uint8_t> read_parameter_page(onfi_interface& onfi, param_type type, bool bytewise) {
    while (gpio_read(GPIO_RB) == 0) {
    }

    uint8_t address = (type == param_type::JEDEC) ? 0x40 : 0x00;
    onfi.send_command(0xEC);
    onfi.send_addresses(&address);

    while (gpio_read(GPIO_RB) == 0) {
    }

    std::vector<uint8_t> buffer(256, 0xFF);
    if (!bytewise) {
        onfi.get_data(buffer.data(), static_cast<uint16_t>(buffer.size()));
    } else {
        uint8_t col[2] = {0, 0};
        for (std::size_t idx = 0; idx < buffer.size(); ++idx) {
            col[1] = static_cast<uint8_t>(idx / 256U);
            col[0] = static_cast<uint8_t>(idx % 256U);
            onfi.change_read_column(col);
            onfi.get_data(&buffer[idx], 1);
        }
    }
    return buffer;
}

void print_byte_table(std::ostream& out, const std::vector<uint8_t>& data) {
    out << std::hex << std::setfill('0');
    for (std::size_t offset = 0; offset < data.size(); offset += 16) {
        out << "0x" << std::setw(6) << offset << "  ";
        for (std::size_t i = 0; i < 16; ++i) {
            if (offset + i < data.size()) {
                out << std::setw(2) << static_cast<int>(data[offset + i]) << ' ';
            } else {
                out << "   ";
            }
        }
        out << " |";
        for (std::size_t i = 0; i < 16 && offset + i < data.size(); ++i) {
            const uint8_t byte = data[offset + i];
            out << (std::isprint(byte) ? static_cast<char>(byte) : '.');
        }
        out << "|\n";
    }
    out << std::dec << std::setfill(' ');
}

void print_timing_summary(std::ostream& out,
                          std::string_view label,
                          const onfi::timed::OperationTiming& timing) {
    out << label << " busy interval: ";
    if (!timing.busy_detected) {
        out << "not observed" << "\n";
        return;
    }

    const double micros = static_cast<double>(timing.duration_ns) / 1000.0;
    out << std::fixed << std::setprecision(3) << micros
        << " us (" << timing.duration_ns << " ns)\n";
    out.unsetf(std::ios::floatfield);
    out << "  status: 0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(timing.status) << std::dec << std::setfill(' ') << "\n";
    out << "  ready:  " << (timing.ready ? "yes" : "no") << "\n";
    out << "  pass:   " << (timing.pass ? "yes" : "no") << "\n";
    if (timing.timed_out) {
        out << "  note:  busy interval exceeded timeout threshold" << "\n";
    }
}

void report_timing_status(std::ostream& err,
                          std::string_view label,
                          const onfi::timed::OperationTiming& timing) {
    if (timing.succeeded()) return;

    err << label << ' ';
    if (!timing.busy_detected) {
        err << "never asserted busy";
    } else if (!timing.ready) {
        err << "did not report ready completion";
    } else if (!timing.pass) {
        err << "reported failure status";
    } else if (timing.timed_out) {
        err << "exceeded busy timeout";
    }
    err << " (status=0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(timing.status) << std::dec << std::setfill(' ') << ")\n";
}

bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return ofs.good();
}

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open input file: " + path);
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

std::string to_hex_string(const uint8_t* data, std::size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
        if (i + 1 < len) oss << ':';
    }
    return oss.str();
}

int probe_command(const CommandContext& context) {
    const bool use_jedec = context.arguments.has("jedec");
    const bool bytewise = context.arguments.has("bytewise");
    param_type type = use_jedec ? param_type::JEDEC : param_type::ONFI;

    auto& onfi = context.driver.require_onfi_started(type);
    if (context.arguments.has("refresh")) {
        onfi.read_parameters(type, bytewise, context.verbose);
    }

    GeometrySummary g = summarize_geometry(onfi);

    context.out << "Manufacturer: " << onfi.manufacturer_id << "\n";
    context.out << "Model: " << onfi.device_model << "\n";
    context.out << "ONFI Version: " << onfi.onfi_version << "\n";
    context.out << "Unique ID: " << onfi.unique_id << "\n";
    context.out << "Geometry:\n";
    context.out << "  Page bytes: " << g.page_bytes << "\n";
    context.out << "  Spare bytes: " << g.spare_bytes << "\n";
    context.out << "  Pages per block: " << g.pages_per_block << "\n";
    context.out << "  Blocks: " << g.blocks << "\n";
    context.out << "Interface: " << (onfi.interface_type == asynchronous ? "asynchronous" : "toggle") << "\n";
    return 0;
}

int read_id_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    if (context.arguments.has("refresh")) {
        onfi.read_id();
    }

    context.out << "Manufacturer: " << onfi.manufacturer_id << "\n";
    context.out << "Model: " << onfi.device_model << "\n";
    context.out << "Unique ID: " << onfi.unique_id << "\n";
    context.out << "Unique ID (hex): " << to_hex_string(reinterpret_cast<const uint8_t*>(onfi.unique_id), 32) << "\n";
    return 0;
}

int status_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const uint8_t status = onfi.get_status();

    context.out << "Status: 0x" << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(status) << std::dec << std::setfill(' ') << "\n";
    context.out << "  Ready: " << ((status & 0x40) ? "yes" : "no") << "\n";
    context.out << "  Pass: " << ((status & 0x01) ? "no" : "yes") << "\n";
    context.out << "  Write Protect: " << ((status & 0x80) ? "inactive" : "active") << "\n";
    if (context.arguments.has("raw")) {
        context.out << "  Raw bits: ";
        for (int bit = 7; bit >= 0; --bit) {
            context.out << ((status >> bit) & 0x1);
        }
        context.out << "\n";
    }
    return 0;
}

int test_leds_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    onfi.test_onfi_leds(context.verbose);
    context.out << "LED test completed." << "\n";
    return 0;
}

int read_parameters_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const bool use_jedec = context.arguments.has("jedec");
    const bool bytewise = context.arguments.has("bytewise");
    param_type type = use_jedec ? param_type::JEDEC : param_type::ONFI;

    onfi.read_parameters(type, bytewise, context.verbose);
    auto buffer = read_parameter_page(onfi, type, bytewise);

    if (context.arguments.has("raw")) {
        print_byte_table(context.out, buffer);
    }

    if (auto output = context.arguments.value("output")) {
        if (!write_file(*output, buffer)) {
            context.err << "Failed to write parameter data to '" << *output << "'\n";
            return 1;
        }
        context.out << "Wrote " << buffer.size() << " bytes to '" << *output << "'\n";
    }

    GeometrySummary g = summarize_geometry(onfi);
    context.out << "Geometry updated:\n";
    context.out << "  Page bytes: " << g.page_bytes << "\n";
    context.out << "  Spare bytes: " << g.spare_bytes << "\n";
    context.out << "  Pages per block: " << g.pages_per_block << "\n";
    context.out << "  Blocks: " << g.blocks << "\n";
    return 0;
}

int read_page_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t block = context.arguments.require_int("block");
    const int64_t page = context.arguments.require_int("page");
    if (block < 0 || block >= onfi.num_blocks) {
        throw std::invalid_argument("Block index out of range");
    }
    if (page < 0 || page >= onfi.num_pages_in_block) {
        throw std::invalid_argument("Page index out of range");
    }
    const bool include_spare = context.arguments.has("include-spare");
    const bool bytewise = context.arguments.has("bytewise");

    onfi::OnfiController controller(onfi);
    onfi::NandDevice device(controller);
    configure_device(onfi, device);

    std::vector<uint8_t> buffer;
    device.read_page(static_cast<unsigned int>(block), static_cast<unsigned int>(page), include_spare, bytewise, buffer);

    if (auto output = context.arguments.value("output")) {
        if (!write_file(*output, buffer)) {
            context.err << "Failed to write page to '" << *output << "'\n";
            return 1;
        }
        context.out << "Wrote " << buffer.size() << " bytes." << "\n";
    } else {
        print_byte_table(context.out, buffer);
    }
    return 0;
}

int program_page_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t block = context.arguments.require_int("block");
    const int64_t page = context.arguments.require_int("page");
    if (block < 0 || block >= onfi.num_blocks) {
        throw std::invalid_argument("Block index out of range");
    }
    if (page < 0 || page >= onfi.num_pages_in_block) {
        throw std::invalid_argument("Page index out of range");
    }
    auto input_path = context.arguments.value("input");
    if (!input_path) {
        throw std::invalid_argument("Missing required option '--input'");
    }
    const bool include_spare = context.arguments.has("include-spare");
    const bool verify = context.arguments.has("verify");
    const bool pad = context.arguments.has("pad");

    std::vector<uint8_t> payload = read_file(*input_path);
    const std::size_t expected = onfi.num_bytes_in_page + (include_spare ? onfi.num_spare_bytes_in_page : 0);
    if (payload.size() < expected) {
        if (!pad) {
            throw std::runtime_error("Input shorter than expected page length; use --pad to fill remaining bytes");
        }
        payload.resize(expected, 0xFF);
    } else if (payload.size() > expected) {
        throw std::runtime_error("Input larger than expected page length");
    }

    onfi::OnfiController controller(onfi);
    onfi::NandDevice device(controller);
    configure_device(onfi, device);

    device.program_page(static_cast<unsigned int>(block), static_cast<unsigned int>(page), payload.data(), include_spare);
    onfi.wait_ready_blocking();
    const uint8_t status = onfi.get_status();
    if (status & 0x01) {
        context.err << "Program operation failed (status=0x" << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(status) << std::dec << std::setfill(' ') << ")\n";
        return 1;
    }

    if (verify) {
        std::vector<uint8_t> verify_buf;
        device.read_page(static_cast<unsigned int>(block), static_cast<unsigned int>(page), include_spare,
                         /*bytewise*/false, verify_buf);
        if (verify_buf != payload) {
            context.err << "Verification failed: read-back data does not match input." << "\n";
            return 2;
        }
    }

    context.out << "Program successful." << "\n";
    return 0;
}

int erase_block_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t block = context.arguments.require_int("block");
    if (block < 0 || block >= onfi.num_blocks) {
        throw std::invalid_argument("Block index out of range");
    }

    onfi::OnfiController controller(onfi);
    onfi::NandDevice device(controller);
    configure_device(onfi, device);

    device.erase_block(static_cast<unsigned int>(block));
    onfi.wait_ready_blocking();
    const uint8_t status = onfi.get_status();
    if (status & 0x01) {
        context.err << "Erase failed (status=0x" << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(status) << std::dec << std::setfill(' ') << ")\n";
        return 1;
    }
    context.out << "Erase successful." << "\n";
    return 0;
}


} // namespace


int read_block_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t block = context.arguments.require_int("block");
    ensure_block_in_range(onfi, block);
    const bool include_spare = context.arguments.has("include-spare");
    const bool bytewise = context.arguments.has("bytewise");
    const auto pages_option = context.arguments.value("pages");
    const bool complete = !pages_option.has_value();
    std::vector<uint16_t> pages;
    if (pages_option) {
        pages = parse_page_list(*pages_option, onfi.num_pages_in_block);
        if (pages.empty()) {
            throw std::invalid_argument("--pages must specify at least one page index");
        }
    }

    onfi::OnfiController controller(onfi);
    onfi::NandDevice device(controller);
    configure_device(onfi, device);

    std::unique_ptr<onfi::DataSink> sink;
    if (auto output = context.arguments.value("output")) {
        sink = std::make_unique<onfi::FileDataSink>(output->c_str());
    } else {
        sink = std::make_unique<onfi::HexOstreamDataSink>(context.out);
    }

    device.read_block(static_cast<unsigned int>(block),
                      complete,
                      pages.empty() ? nullptr : pages.data(),
                      static_cast<uint16_t>(pages.size()),
                      include_spare,
                      bytewise,
                      *sink);
    sink->flush();
    return 0;
}

int program_block_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t block = context.arguments.require_int("block");
    ensure_block_in_range(onfi, block);
    const bool include_spare = context.arguments.has("include-spare");
    const bool randomize = context.arguments.has("random");
    const bool pad = context.arguments.has("pad");
    const bool verify = context.arguments.has("verify");

    const auto pages_option = context.arguments.value("pages");
    const bool complete = !pages_option.has_value();
    std::vector<uint16_t> pages;
    if (pages_option) {
        pages = parse_page_list(*pages_option, onfi.num_pages_in_block);
        if (pages.empty()) {
            throw std::invalid_argument("--pages must specify at least one page index");
        }
    }

    if (verify && randomize) {
        throw std::invalid_argument("Cannot combine --verify with --random");
    }

    std::vector<uint8_t> payload;
    const uint8_t* payload_ptr = nullptr;
    if (auto input = context.arguments.value("input")) {
        payload = read_file(*input);
        const std::size_t expected = onfi.num_bytes_in_page + (include_spare ? onfi.num_spare_bytes_in_page : 0);
        if (payload.size() < expected) {
            if (!pad) {
                throw std::runtime_error("Input shorter than expected page length; use --pad to fill remaining bytes");
            }
            payload.resize(expected, 0xFF);
        } else if (payload.size() > expected) {
            throw std::runtime_error("Input larger than expected page length");
        }
        payload_ptr = payload.data();
    }

    onfi::OnfiController controller(onfi);
    onfi::NandDevice device(controller);
    configure_device(onfi, device);

    device.program_block(static_cast<unsigned int>(block),
                         complete,
                         pages.empty() ? nullptr : pages.data(),
                         static_cast<uint16_t>(pages.size()),
                         payload_ptr,
                         include_spare,
                         randomize);
    onfi.wait_ready_blocking();
    const uint8_t status = onfi.get_status();
    if (status & 0x01) {
        context.err << "Program block failed (status=0x" << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(status) << std::dec << std::setfill(' ') << ")\n";
        return 1;
    }

    if (verify) {
        const uint8_t* expected_ptr = payload_ptr;
        const char* verification_label = payload_ptr ? "with provided pattern" : "with default pattern";
        const bool ok = device.verify_program_block(static_cast<unsigned int>(block),
                                                    complete,
                                                    pages.empty() ? nullptr : pages.data(),
                                                    static_cast<uint16_t>(pages.size()),
                                                    expected_ptr,
                                                    include_spare,
                                                    context.verbose,
                                                    /*max_allowed_errors*/0);
        if (!ok) {
            context.err << "Verification failed " << verification_label << "." << "\n";
            return 2;
        }
    }

    context.out << "Program block successful." << "\n";
    return 0;
}

int measure_erase_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t block = context.arguments.require_int("block");
    ensure_block_in_range(onfi, block);

    const auto timing = onfi::timed::erase_block(
        onfi,
        static_cast<unsigned int>(block),
        context.verbose);

    print_timing_summary(context.out, "Erase", timing);
    report_timing_status(context.err, "Erase", timing);
    return timing.succeeded() ? 0 : 1;
}

int measure_program_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t block = context.arguments.require_int("block");
    ensure_block_in_range(onfi, block);
    const int64_t page = context.arguments.require_int("page");
    ensure_page_in_range(onfi, page);

    const bool include_spare = context.arguments.has("include-spare");
    const bool pad = context.arguments.has("pad");
    const auto input_path = context.arguments.value("input");

    const std::size_t expected = static_cast<std::size_t>(onfi.num_bytes_in_page) +
                                 (include_spare ? onfi.num_spare_bytes_in_page : 0U);
    std::vector<uint8_t> payload(expected, 0xFF);
    if (input_path) {
        payload = read_file(*input_path);
        if (payload.size() < expected) {
            if (!pad) {
                throw std::runtime_error("Input shorter than expected page length; use --pad to fill remaining bytes");
            }
            payload.resize(expected, 0xFF);
        } else if (payload.size() > expected) {
            throw std::runtime_error("Input larger than expected page length");
        }
    }

    const auto timing = onfi::timed::program_page(
        onfi,
        static_cast<unsigned int>(block),
        static_cast<unsigned int>(page),
        payload.data(),
        static_cast<uint32_t>(payload.size()),
        include_spare,
        context.verbose);

    context.out << "Payload length: " << payload.size() << " bytes\n";
    print_timing_summary(context.out, "Program", timing);
    report_timing_status(context.err, "Program", timing);
    return timing.succeeded() ? 0 : 1;
}

int measure_read_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t block = context.arguments.require_int("block");
    ensure_block_in_range(onfi, block);
    const int64_t page = context.arguments.require_int("page");
    ensure_page_in_range(onfi, page);

    const bool include_spare = context.arguments.has("include-spare");
    const auto output_path = context.arguments.value("output");

    const std::size_t expected = static_cast<std::size_t>(onfi.num_bytes_in_page) +
                                 (include_spare ? onfi.num_spare_bytes_in_page : 0U);
    std::vector<uint8_t> buffer(expected, 0x00);

    const auto timing = onfi::timed::read_page(
        onfi,
        static_cast<unsigned int>(block),
        static_cast<unsigned int>(page),
        buffer.data(),
        static_cast<uint32_t>(buffer.size()),
        include_spare,
        context.verbose,
        true);

    context.out << "Captured " << buffer.size() << " bytes\n";
    print_timing_summary(context.out, "Read", timing);
    if (output_path) {
        if (!write_file(*output_path, buffer)) {
            throw std::runtime_error("Failed to write output file: " + *output_path);
        }
        context.out << "Wrote page data to '" << *output_path << "'\n";
    } else {
        print_byte_table(context.out, buffer);
    }
    report_timing_status(context.err, "Read", timing);
    return timing.succeeded() ? 0 : 1;
}

int verify_page_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t block = context.arguments.require_int("block");
    const int64_t page = context.arguments.require_int("page");
    ensure_block_in_range(onfi, block);
    ensure_page_in_range(onfi, page);
    const bool include_spare = context.arguments.has("include-spare");

    std::vector<uint8_t> expected;
    const uint8_t* expected_ptr = nullptr;
    if (auto input = context.arguments.value("input")) {
        expected = read_file(*input);
        const std::size_t expected_len = onfi.num_bytes_in_page + (include_spare ? onfi.num_spare_bytes_in_page : 0);
        if (expected.size() != expected_len) {
            throw std::runtime_error("Expected data length must match page length including spare selection");
        }
        expected_ptr = expected.data();
    }

    onfi::OnfiController controller(onfi);
    onfi::NandDevice device(controller);
    configure_device(onfi, device);

    uint32_t byte_errors = 0;
    uint32_t bit_errors = 0;
    const bool ok = device.verify_program_page(static_cast<unsigned int>(block),
                                               static_cast<unsigned int>(page),
                                               expected_ptr,
                                               include_spare,
                                               context.verbose,
                                               0,
                                               &byte_errors,
                                               &bit_errors);
    context.out << "Byte errors: " << byte_errors << ", bit errors: " << bit_errors << "\n";
    context.out << (ok ? "Verification passed." : "Verification failed.") << "\n";
    return ok ? 0 : 1;
}

int verify_block_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t block = context.arguments.require_int("block");
    ensure_block_in_range(onfi, block);
    const bool include_spare = context.arguments.has("include-spare");
    const auto pages_option = context.arguments.value("pages");
    const bool complete = !pages_option.has_value();
    std::vector<uint16_t> pages;
    if (pages_option) {
        pages = parse_page_list(*pages_option, onfi.num_pages_in_block);
        if (pages.empty()) {
            throw std::invalid_argument("--pages must specify at least one page index");
        }
    }

    std::vector<uint8_t> expected;
    const uint8_t* expected_ptr = nullptr;
    if (auto input = context.arguments.value("input")) {
        expected = read_file(*input);
        const std::size_t expected_len = onfi.num_bytes_in_page + (include_spare ? onfi.num_spare_bytes_in_page : 0);
        if (expected.size() != expected_len) {
            throw std::runtime_error("Expected data length must match page length including spare selection");
        }
        expected_ptr = expected.data();
    }

    onfi::OnfiController controller(onfi);
    onfi::NandDevice device(controller);
    configure_device(onfi, device);

    const bool ok = device.verify_program_block(static_cast<unsigned int>(block),
                                                complete,
                                                pages.empty() ? nullptr : pages.data(),
                                                static_cast<uint16_t>(pages.size()),
                                                expected_ptr,
                                                include_spare,
                                                context.verbose,
                                                0);
    context.out << (ok ? "Verification passed." : "Verification failed.") << "\n";
    return ok ? 0 : 1;
}

int erase_chip_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    int64_t start = context.arguments.value_as_int("start", 0);
    int64_t count = context.arguments.value_as_int("count", onfi.num_blocks - start);
    if (start < 0 || start >= onfi.num_blocks) {
        throw std::invalid_argument("Start block out of range");
    }
    if (count <= 0) {
        throw std::invalid_argument("Count must be positive");
    }
    if (start + count > onfi.num_blocks) {
        count = onfi.num_blocks - start;
    }

    onfi::OnfiController controller(onfi);
    onfi::NandDevice device(controller);
    configure_device(onfi, device);

    for (int64_t offset = 0; offset < count; ++offset) {
        const unsigned int block = static_cast<unsigned int>(start + offset);
        context.out << "Erasing block " << block << "..." << std::flush;
        device.erase_block(block);
        onfi.wait_ready_blocking();
        const uint8_t status = onfi.get_status();
        if (status & 0x01) {
            context.out << " failed (status=0x" << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<int>(status) << std::dec << std::setfill(' ') << ")" << std::endl;
            return 1;
        }
        context.out << " done" << std::endl;
    }
    return 0;
}

int scan_bad_blocks_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    if (context.arguments.has("block")) {
        const int64_t block = context.arguments.require_int("block");
        ensure_block_in_range(onfi, block);
        const bool bad = onfi.is_bad_block(static_cast<unsigned int>(block));
        context.out << "Block " << block << (bad ? " is bad" : " is good") << "\n";
        return bad ? 1 : 0;
    }

    context.out << "Scanning for bad blocks..." << std::endl;
    bool any_bad = false;
    for (uint32_t block = 0; block < onfi.num_blocks; ++block) {
        if (onfi.is_bad_block(block)) {
            any_bad = true;
            context.out << "  Bad block: " << block << "\n";
        }
    }
    if (!any_bad) {
        context.out << "No bad blocks detected." << std::endl;
    }
    return any_bad ? 1 : 0;
}

int set_feature_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t address = context.arguments.require_int("address");
    if (address < 0 || address > 0xFF) {
        throw std::invalid_argument("Feature address out of range");
    }
    auto data_arg = context.arguments.value("data");
    if (!data_arg) {
        throw std::invalid_argument("Missing required option '--data'");
    }
    auto payload = parse_feature_payload(*data_arg);
    onfi.set_features(static_cast<uint8_t>(address), payload.data(), onfi::FeatureCommand::Set);
    context.out << "Set feature 0x" << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(address) << std::dec << std::setfill(' ') << "\n";
    return 0;
}

int get_feature_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t address = context.arguments.require_int("address");
    if (address < 0 || address > 0xFF) {
        throw std::invalid_argument("Feature address out of range");
    }
    std::array<uint8_t, 4> payload{0, 0, 0, 0};
    onfi.get_features(static_cast<uint8_t>(address), payload.data(), onfi::FeatureCommand::Get);
    context.out << "Feature 0x" << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(address) << std::dec << std::setfill(' ') << ": ";
    for (std::size_t i = 0; i < payload.size(); ++i) {
        if (i) context.out << ' ';
        context.out << "0x" << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(payload[i]) << std::dec << std::setfill(' ');
    }
    context.out << "\n";
    return 0;
}

int reset_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    onfi.reset_device(context.verbose);
    context.out << "Reset issued." << "\n";
    return 0;
}

int device_init_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    onfi.device_initialization(context.verbose);
    context.out << "Device initialization sequence complete." << "\n";
    return 0;
}

int wait_ready_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    context.out << "Waiting for ready..." << std::flush;
    onfi.wait_ready_blocking();
    context.out << " done." << "\n";
    return 0;
}

int raw_command_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t value = context.arguments.require_int("value");
    if (value < 0 || value > 0xFF) {
        throw std::invalid_argument("Command byte out of range");
    }
    onfi.send_command(static_cast<uint8_t>(value));
    context.out << "Sent command 0x" << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(value) << std::dec << std::setfill(' ') << "\n";
    return 0;
}

int raw_address_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    auto bytes_arg = context.arguments.value("bytes");
    if (!bytes_arg) {
        throw std::invalid_argument("Missing required option '--bytes'");
    }
    auto tokens = split_list(*bytes_arg);
    if (tokens.empty()) {
        throw std::invalid_argument("--bytes requires at least one value");
    }
    std::vector<uint8_t> bytes;
    bytes.reserve(tokens.size());
    for (const auto& token : tokens) {
        bytes.push_back(parse_byte_token(token));
    }
    onfi.send_addresses(bytes.data(), static_cast<uint8_t>(bytes.size()));
    context.out << "Sent " << bytes.size() << " address bytes." << "\n";
    return 0;
}

int raw_send_data_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    auto bytes_arg = context.arguments.value("bytes");
    if (!bytes_arg) {
        throw std::invalid_argument("Missing required option '--bytes'");
    }
    auto tokens = split_list(*bytes_arg);
    if (tokens.empty()) {
        throw std::invalid_argument("--bytes requires at least one value");
    }
    std::vector<uint8_t> bytes;
    bytes.reserve(tokens.size());
    for (const auto& token : tokens) {
        bytes.push_back(parse_byte_token(token));
    }
    onfi.send_data(bytes.data(), static_cast<uint16_t>(bytes.size()));
    context.out << "Sent " << bytes.size() << " data bytes." << "\n";
    return 0;
}

int raw_read_data_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t count = context.arguments.require_int("count");
    if (count <= 0 || count > 4096) {
        throw std::invalid_argument("--count must be between 1 and 4096");
    }
    std::vector<uint8_t> buffer(static_cast<std::size_t>(count));
    onfi.get_data(buffer.data(), static_cast<uint16_t>(buffer.size()));
    print_byte_table(context.out, buffer);
    return 0;
}

int raw_change_column_command(const CommandContext& context) {
    auto& onfi = context.driver.require_onfi_started();
    const int64_t column = context.arguments.require_int("column");
    if (column < 0 || column > 0xFFFF) {
        throw std::invalid_argument("Column value out of range");
    }
    uint8_t col_bytes[2] = {
        static_cast<uint8_t>(column & 0xFF),
        static_cast<uint8_t>((column >> 8) & 0xFF)
    };
    onfi.change_read_column(col_bytes);
    context.out << "Adjusted read column to 0x" << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<int>(column) << std::dec << std::setfill(' ') << "\n";
    return 0;
}



void register_onfi_commands(CommandRegistry& registry) {
    registry.register_command({
        .name = "probe",
        .aliases = {"onfi-info", "info"},
        .summary = "Initialize the ONFI stack and report geometry.",
        .description = "Performs ONFI bring-up, optionally refreshes parameter parsing, and prints device details.",
        .usage = "nandworks probe [--jedec] [--bytewise] [--refresh]",
        .options = {
            OptionSpec{"jedec", '\0', false, false, false, "", "Use JEDEC parameter page instead of ONFI."},
            OptionSpec{"bytewise", '\0', false, false, false, "", "Acquire parameter page bytewise (slower)."},
            OptionSpec{"refresh", '\0', false, false, false, "", "Force re-reading the parameter page before reporting."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = probe_command,
    });

    registry.register_command({
        .name = "read-id",
        .aliases = {"id"},
        .summary = "Read and display the NAND unique identifier.",
        .description = "Fetches the ONFI unique ID sequence along with manufacturer and model strings.",
        .usage = "nandworks read-id [--refresh]",
        .options = {
            OptionSpec{"refresh", '\0', false, false, false, "", "Issue additional READ-ID transactions before reporting."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = read_id_command,
    });

    registry.register_command({
        .name = "status",
        .aliases = {},
        .summary = "Query and decode the ONFI status register.",
        .description = "Issues the 0x70 status command and prints readiness, pass/fail, and write-protect bits.",
        .usage = "nandworks status [--raw]",
        .options = {
            OptionSpec{"raw", '\0', false, false, false, "", "Print raw bit pattern in addition to decoded fields."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = status_command,
    });

    registry.register_command({
        .name = "test-leds",
        .aliases = {"leds"},
        .summary = "Pulse the ONFI indicator LEDs to validate GPIO connectivity.",
        .description = "Runs the built-in LED test from the HAL to confirm GPIO wiring.",
        .usage = "nandworks test-leds",
        .options = {},
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = test_leds_command,
    });

    registry.register_command({
        .name = "parameters",
        .aliases = {"param"},
        .summary = "Read the ONFI or JEDEC parameter page and update cached geometry.",
        .description = "Retrieves the 256-byte parameter page, updates cached geometry, and optionally dumps it.",
        .usage = "nandworks parameters [--jedec] [--bytewise] [--raw] [--output <path>]",
        .options = {
            OptionSpec{"jedec", '\0', false, false, false, "", "Use the JEDEC parameter page."},
            OptionSpec{"bytewise", '\0', false, false, false, "", "Read the parameter page byte-by-byte."},
            OptionSpec{"raw", '\0', false, false, false, "", "Dump the raw 256-byte page to stdout."},
            OptionSpec{"output", 'o', true, false, false, "file", "Write the raw parameter page to a file."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = read_parameters_command,
    });

    registry.register_command({
        .name = "read-page",
        .aliases = {"read"},
        .summary = "Read a NAND page into memory and display or persist it.",
        .description = "Uses the ONFI READ command sequence to capture a page, optionally including spare bytes.",
        .usage = "nandworks read-page --block <index> --page <index> [--include-spare] [--bytewise] [--output <path>]",
        .options = {
            OptionSpec{"block", 'b', true, true, false, "index", "Block index (0-based)."},
            OptionSpec{"page", 'p', true, true, false, "index", "Page index within the block (0-based)."},
            OptionSpec{"include-spare", 's', false, false, false, "", "Include spare (OOB) bytes in the dump."},
            OptionSpec{"bytewise", '\0', false, false, false, "", "Perform bytewise column switching for the transfer."},
            OptionSpec{"output", 'o', true, false, false, "file", "Write the raw page data to the specified file."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = read_page_command,
    });

    registry.register_command({
        .name = "program-page",
        .aliases = {"program"},
        .summary = "Program a NAND page from a binary file.",
        .description = "Writes the provided data buffer to the target page and optionally verifies the result.",
        .usage = "nandworks program-page --block <index> --page <index> --input <path> [--include-spare] [--pad] [--verify]",
        .options = {
            OptionSpec{"block", 'b', true, true, false, "index", "Block index (0-based)."},
            OptionSpec{"page", 'p', true, true, false, "index", "Page index within the block (0-based)."},
            OptionSpec{"input", 'i', true, true, false, "file", "Path to the binary payload to program."},
            OptionSpec{"include-spare", 's', false, false, false, "", "Include spare (OOB) bytes when programming."},
            OptionSpec{"pad", '\0', false, false, false, "", "Pad shorter inputs with 0xFF up to the required length."},
            OptionSpec{"verify", 'v', false, false, false, "", "Read the page back and compare with the input buffer."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .requires_session = true,
        .requires_root = true,
        .handler = program_page_command,
    });


    registry.register_command({
        .name = "read-block",
        .aliases = {"readb"},
        .summary = "Read an entire block or selected pages and display or persist it.",
        .description = "Uses the ONFI READ sequence to dump one or more pages, optionally including spare bytes.",
        .usage = "nandworks read-block --block <index> [--pages <list>] [--include-spare] [--bytewise] [--output <path>]",
        .options = {
            OptionSpec{"block", 'b', true, true, false, "index", "Block index (0-based)."},
            OptionSpec{"pages", 'p', true, false, false, "list", "Comma or dash separated page list (default all)."},
            OptionSpec{"include-spare", 's', false, false, false, "", "Include spare (OOB) bytes in the dump."},
            OptionSpec{"bytewise", '\0', false, false, false, "", "Perform bytewise column switching for the transfer."},
            OptionSpec{"output", 'o', true, false, false, "file", "Write the dump to a binary file."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = read_block_command,
    });

    registry.register_command({
        .name = "program-block",
        .aliases = {"programb"},
        .summary = "Program a block using a fixed payload, random data, or supplied pages.",
        .description = "Invokes the ONFI program flow across a block or subset of pages with optional verification.",
        .usage = "nandworks program-block --block <index> [--pages <list>] [--input <path>] [--include-spare] [--pad] [--verify] [--random]",
        .options = {
            OptionSpec{"block", 'b', true, true, false, "index", "Block index (0-based)."},
            OptionSpec{"pages", 'p', true, false, false, "list", "Comma or dash separated page list."},
            OptionSpec{"input", 'i', true, false, false, "file", "Binary payload to program into each page."},
            OptionSpec{"include-spare", 's', false, false, false, "", "Include spare bytes when programming."},
            OptionSpec{"pad", '\0', false, false, false, "", "Pad shorter payloads with 0xFF."},
            OptionSpec{"verify", 'v', false, false, false, "", "Verify contents after programming."},
            OptionSpec{"random", 'r', false, false, false, "", "Program pages with randomized data."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .requires_session = true,
        .requires_root = true,
        .handler = program_block_command,
    });

    registry.register_command({
        .name = "verify-page",
        .aliases = {"vp"},
        .summary = "Verify a programmed page against expected data.",
        .description = "Compares the contents of a page with optional reference data and reports byte/bit errors.",
        .usage = "nandworks verify-page --block <index> --page <index> [--include-spare] [--input <path>]",
        .options = {
            OptionSpec{"block", 'b', true, true, false, "index", "Block index (0-based)."},
            OptionSpec{"page", 'p', true, true, false, "index", "Page index (0-based)."},
            OptionSpec{"include-spare", 's', false, false, false, "", "Include spare bytes when comparing."},
            OptionSpec{"input", 'i', true, false, false, "file", "Reference data to compare against."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = verify_page_command,
    });

    registry.register_command({
        .name = "verify-block",
        .aliases = {"vb"},
        .summary = "Verify an entire block or subset of pages.",
        .description = "Runs the verify flow across a block, optionally with reference data for each page.",
        .usage = "nandworks verify-block --block <index> [--pages <list>] [--include-spare] [--input <path>]",
        .options = {
            OptionSpec{"block", 'b', true, true, false, "index", "Block index (0-based)."},
            OptionSpec{"pages", 'p', true, false, false, "list", "Comma or dash separated page list."},
            OptionSpec{"include-spare", 's', false, false, false, "", "Include spare bytes when comparing."},
            OptionSpec{"input", 'i', true, false, false, "file", "Reference data to compare against."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = verify_block_command,
    });

    registry.register_command({
        .name = "erase-chip",
        .aliases = {"erase-all"},
        .summary = "Erase a contiguous range of blocks (default entire device).",
        .description = "Iterates block erase across the device, stopping on first failure.",
        .usage = "nandworks erase-chip [--start <index>] [--count <n>]",
        .options = {
            OptionSpec{"start", '\0', true, false, false, "index", "Starting block index (default 0)."},
            OptionSpec{"count", '\0', true, false, false, "count", "Number of blocks to erase (default to end)."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .requires_session = true,
        .requires_root = true,
        .handler = erase_chip_command,
    });

    registry.register_command({
        .name = "scan-bad-blocks",
        .aliases = {"bad-blocks"},
        .summary = "Identify blocks marked as bad.",
        .description = "Checks either a single block or the entire device for factory bad-block markers.",
        .usage = "nandworks scan-bad-blocks [--block <index>]",
        .options = {
            OptionSpec{"block", 'b', true, false, false, "index", "Optional single block to inspect."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = scan_bad_blocks_command,
    });

    registry.register_command({
        .name = "set-feature",
        .aliases = {"feature-set"},
        .summary = "Issue an ONFI SET FEATURES command.",
        .description = "Writes four bytes to a feature address.",
        .usage = "nandworks set-feature --address <addr> --data <b0,b1,b2,b3>",
        .options = {
            OptionSpec{"address", 'a', true, true, false, "value", "Feature address."},
            OptionSpec{"data", 'd', true, true, false, "bytes", "Four comma-separated byte values."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .requires_session = true,
        .requires_root = true,
        .handler = set_feature_command,
    });

    registry.register_command({
        .name = "get-feature",
        .aliases = {"feature-get"},
        .summary = "Read an ONFI feature value.",
        .description = "Reads four bytes from the specified feature address.",
        .usage = "nandworks get-feature --address <addr>",
        .options = {
            OptionSpec{"address", 'a', true, true, false, "value", "Feature address."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = get_feature_command,
    });

    registry.register_command({
        .name = "reset-device",
        .aliases = {"reset"},
        .summary = "Issue an ONFI reset command.",
        .description = "Sends the 0xFF reset command and waits for ready.",
        .usage = "nandworks reset-device",
        .options = {},
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = reset_command,
    });

    registry.register_command({
        .name = "device-init",
        .aliases = {"init"},
        .summary = "Run the device initialization sequence.",
        .description = "Executes the power-on initialization flow and reset.",
        .usage = "nandworks device-init",
        .options = {},
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = device_init_command,
    });

    registry.register_command({
        .name = "wait-ready",
        .aliases = {"wait"},
        .summary = "Block until R/B# indicates ready.",
        .description = "Invokes the HAL wait helper to pause until the NAND is ready.",
        .usage = "nandworks wait-ready",
        .options = {},
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = wait_ready_command,
    });

    registry.register_command({
        .name = "raw-command",
        .aliases = {"cmd"},
        .summary = "Send a raw ONFI command byte.",
        .description = "Issues a single command cycle with the provided opcode.",
        .usage = "nandworks raw-command --value <byte>",
        .options = {
            OptionSpec{"value", 'v', true, true, false, "byte", "Command byte value."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .requires_session = true,
        .requires_root = true,
        .handler = raw_command_command,
    });

    registry.register_command({
        .name = "raw-address",
        .aliases = {"addr"},
        .summary = "Send one or more address bytes.",
        .description = "Dispatches an address cycle with the provided bytes.",
        .usage = "nandworks raw-address --bytes <b0,b1,...>",
        .options = {
            OptionSpec{"bytes", 'b', true, true, false, "list", "Comma separated address bytes."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .requires_session = true,
        .requires_root = true,
        .handler = raw_address_command,
    });

    registry.register_command({
        .name = "raw-send-data",
        .aliases = {"data"},
        .summary = "Drive data bytes onto the bus.",
        .description = "Uses the HAL to send the supplied bytes to the device.",
        .usage = "nandworks raw-send-data --bytes <b0,b1,...>",
        .options = {
            OptionSpec{"bytes", 'b', true, true, false, "list", "Comma separated data bytes."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .requires_session = true,
        .requires_root = true,
        .handler = raw_send_data_command,
    });

    registry.register_command({
        .name = "raw-read-data",
        .aliases = {"data-read"},
        .summary = "Read bytes from the data bus.",
        .description = "Captures data from the NAND into a hex dump.",
        .usage = "nandworks raw-read-data --count <n>",
        .options = {
            OptionSpec{"count", 'c', true, true, false, "n", "Number of bytes to read."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = raw_read_data_command,
    });

    registry.register_command({
        .name = "raw-change-column",
        .aliases = {"column"},
        .summary = "Adjust the current read column.",
        .description = "Issues the CHANGE READ COLUMN sequence to move the column pointer.",
        .usage = "nandworks raw-change-column --column <value>",
        .options = {
            OptionSpec{"column", 'c', true, true, false, "value", "Column address (0-65535)."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .requires_session = true,
        .requires_root = true,
        .handler = raw_change_column_command,
    });

    registry.register_command({
        .name = "measure-erase",
        .aliases = {"timed-erase"},
        .summary = "Erase a block and report busy time.",
        .description = "Issues a block erase and reports the busy interval measured from R/B#.",
        .usage = "nandworks measure-erase --block <index> --force",
        .options = {
            OptionSpec{"block", 'b', true, true, false, "index", "Block index (0-based)."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .requires_session = true,
        .requires_root = true,
        .handler = measure_erase_command,
    });

    registry.register_command({
        .name = "measure-program",
        .aliases = {"timed-program"},
        .summary = "Program a page and report busy time.",
        .description = "Programs a page and reports the busy interval observed via R/B#.",
        .usage = "nandworks measure-program --block <index> --page <index> --force",
        .options = {
            OptionSpec{"block", 'b', true, true, false, "index", "Block index (0-based)."},
            OptionSpec{"page", 'p', true, true, false, "index", "Page index within the block."},
            OptionSpec{"include-spare", '\0', false, false, false, "", "Include spare bytes in the transfer."},
            OptionSpec{"input", 'i', false, true, false, "file", "Binary payload to program (defaults to 0xFF fill)."},
            OptionSpec{"pad", '\0', false, false, false, "", "Pad shorter input with 0xFF."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .requires_session = true,
        .requires_root = true,
        .handler = measure_program_command,
    });

    registry.register_command({
        .name = "measure-read",
        .aliases = {"timed-read"},
        .summary = "Read a page and report busy time.",
        .description = "Reads a page, streams it to stdout unless --output is provided, and reports the busy interval.",
        .usage = "nandworks measure-read --block <index> --page <index> [--output <path>]",
        .options = {
            OptionSpec{"block", 'b', true, true, false, "index", "Block index (0-based)."},
            OptionSpec{"page", 'p', true, true, false, "index", "Page index within the block."},
            OptionSpec{"include-spare", '\0', false, false, false, "", "Include spare bytes in the transfer."},
            OptionSpec{"output", 'o', true, false, false, "file", "Write captured data to a file."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::Safe,
        .requires_session = true,
        .requires_root = true,
        .handler = measure_read_command,
    });

    registry.register_command({
        .name = "erase-block",
        .aliases = {"erase"},
        .summary = "Erase a NAND block.",
        .description = "Issues the ONFI block erase command for the selected block and waits for completion.",
        .usage = "nandworks erase-block --block <index>",
        .options = {
            OptionSpec{"block", 'b', true, true, false, "index", "Block index (0-based)."}
        },
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .requires_session = true,
        .requires_root = true,
        .handler = erase_block_command,
    });

auto set_flags = [&](std::string_view name, bool root, bool session) {
    if (const auto* cmd = registry.find(name)) {
        auto* mutable_cmd = const_cast<Command*>(cmd);
        mutable_cmd->requires_root = root;
        mutable_cmd->requires_session = session;
    }
};

set_flags("probe", true, true);
set_flags("read-id", true, true);
set_flags("status", true, true);
set_flags("test-leds", true, true);
set_flags("parameters", true, true);
set_flags("read-page", true, true);
set_flags("program-page", true, true);
set_flags("read-block", true, true);
set_flags("program-block", true, true);
set_flags("verify-page", true, true);
set_flags("verify-block", true, true);
set_flags("erase-chip", true, true);
set_flags("scan-bad-blocks", true, true);
set_flags("set-feature", true, true);
set_flags("get-feature", true, true);
set_flags("reset-device", true, true);
set_flags("device-init", true, true);
set_flags("wait-ready", true, true);
set_flags("raw-command", true, true);
set_flags("raw-address", true, true);
set_flags("raw-send-data", true, true);
set_flags("raw-read-data", true, true);
set_flags("raw-change-column", true, true);
set_flags("measure-erase", true, true);
set_flags("measure-program", true, true);
set_flags("measure-read", true, true);
set_flags("erase-block", true, true);


}

} // namespace nandworks::commands
