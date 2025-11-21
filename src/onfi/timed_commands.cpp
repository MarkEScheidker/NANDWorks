#include "onfi/timed_commands.hpp"

#include "gpio.hpp"
#include "timing.hpp"

#include <limits>
#include <stdexcept>

namespace onfi::timed {
namespace {
constexpr uint64_t kGuardReadyTimeoutNs = 5'000'000ULL;       // 5 ms
constexpr uint64_t kBusyAssertTimeoutNs = 5'000'000ULL;       // 5 ms
constexpr uint64_t kDefaultBusyTimeoutNs = 1'000'000'000ULL;  // 1 s

struct BusyWindow {
    uint64_t duration_ns = 0;
    bool busy_detected = false;
    bool timed_out = false;
};

void wait_for_ready_high(uint64_t timeout_ns) {
    if (gpio_read(GPIO_RB) != 0) {
        return;
    }
    const uint64_t start = get_timestamp_ns();
    while (gpio_read(GPIO_RB) == 0) {
        if ((get_timestamp_ns() - start) > timeout_ns) {
            throw std::runtime_error("Timed out waiting for device to become ready before issuing command");
        }
    }
}

BusyWindow measure_busy_cycle(uint64_t assert_timeout_ns, uint64_t busy_timeout_ns) {
    BusyWindow window{};

    const uint64_t assert_start = get_timestamp_ns();
    while (gpio_read(GPIO_RB) != 0) {
        if ((get_timestamp_ns() - assert_start) > assert_timeout_ns) {
            window.timed_out = true;
            return window;
        }
    }

    window.busy_detected = true;
    const uint64_t busy_start = get_timestamp_ns();
    while (gpio_read(GPIO_RB) == 0) {
        if ((get_timestamp_ns() - busy_start) > busy_timeout_ns) {
            window.duration_ns = get_timestamp_ns() - busy_start;
            window.timed_out = true;
            return window;
        }
    }

    window.duration_ns = get_timestamp_ns() - busy_start;
    return window;
}

OperationTiming make_timing(const BusyWindow &window, uint8_t status) {
    OperationTiming timing{};
    timing.duration_ns = window.duration_ns;
    timing.status = status;
    timing.ready = (status & 0x40) != 0;
    timing.pass = (status & 0x01) == 0;
    timing.busy_detected = window.busy_detected;
    timing.timed_out = window.timed_out;
    return timing;
}

void ensure_payload_length(const onfi_interface &onfi, uint32_t length, bool include_spare) {
    const uint32_t expected = static_cast<uint32_t>(onfi.num_bytes_in_page) +
                              (include_spare ? static_cast<uint32_t>(onfi.num_spare_bytes_in_page) : 0U);
    if (length != expected) {
        throw std::invalid_argument("Payload length mismatch for timed command");
    }
}

} // namespace

OperationTiming erase_block(onfi_interface &onfi,
                            unsigned int block,
                            bool verbose) {
    if (block >= onfi.num_blocks) {
        throw std::out_of_range("Block index out of range");
    }

    uint8_t address[8] = {0};
    onfi.convert_pagenumber_to_columnrow_address(block, 0, address, verbose);
    uint8_t *row_address = address + onfi.num_column_cycles;

    onfi.enable_erase();
    gpio_set_direction(GPIO_RB, false);
    wait_for_ready_high(kGuardReadyTimeoutNs);

    onfi.send_command(0x60);
    onfi.send_addresses(row_address, static_cast<uint8_t>(onfi.num_row_cycles));
    onfi.send_command(0xD0);

    const BusyWindow busy = measure_busy_cycle(kBusyAssertTimeoutNs, kDefaultBusyTimeoutNs);

    const uint8_t status = onfi.get_status();
    onfi.disable_erase();

    return make_timing(busy, status);
}

OperationTiming program_page(onfi_interface &onfi,
                             unsigned int block,
                             unsigned int page,
                             const uint8_t *data,
                             uint32_t length,
                             bool include_spare,
                             bool verbose) {
    if (data == nullptr) {
        throw std::invalid_argument("Data pointer must not be null");
    }
    if (block >= onfi.num_blocks || page >= onfi.num_pages_in_block) {
        throw std::out_of_range("Block/page index out of range");
    }
    if (length > std::numeric_limits<uint16_t>::max()) {
        throw std::invalid_argument("Payload length exceeds transport capabilities");
    }

    ensure_payload_length(onfi, length, include_spare);

    uint8_t address[8] = {0};
    onfi.convert_pagenumber_to_columnrow_address(block, page, address, verbose);

    onfi.enable_erase();
    gpio_set_direction(GPIO_RB, false);
    wait_for_ready_high(kGuardReadyTimeoutNs);

    onfi.send_command(0x80);
    onfi.send_addresses(address, static_cast<uint8_t>(onfi.num_column_cycles + onfi.num_row_cycles));
    onfi.send_data(data, static_cast<uint16_t>(length));
    onfi.send_command(0x10);

    const BusyWindow busy = measure_busy_cycle(kBusyAssertTimeoutNs, kDefaultBusyTimeoutNs);

    const uint8_t status = onfi.get_status();
    onfi.disable_erase();

    return make_timing(busy, status);
}

OperationTiming read_page(onfi_interface &onfi,
                          unsigned int block,
                          unsigned int page,
                          uint8_t *destination,
                          uint32_t length,
                          bool include_spare,
                          bool verbose,
                          bool fetch_data) {
    if (fetch_data && destination == nullptr) {
        throw std::invalid_argument("Destination pointer must not be null when fetch_data is true");
    }
    if (block >= onfi.num_blocks || page >= onfi.num_pages_in_block) {
        throw std::out_of_range("Block/page index out of range");
    }
    if (fetch_data && length > std::numeric_limits<uint16_t>::max()) {
        throw std::invalid_argument("Read length exceeds transport capabilities");
    }

    if (fetch_data) {
        ensure_payload_length(onfi, length, include_spare);
    }

    uint8_t address[8] = {0};
    onfi.convert_pagenumber_to_columnrow_address(block, page, address, verbose);

    gpio_set_direction(GPIO_RB, false);
    wait_for_ready_high(kGuardReadyTimeoutNs);

    const bool pre_zero = (onfi.flash_chip == toshiba_tlc_toggle);
    if (pre_zero) {
        onfi.send_command(0x00);
    }
    onfi.send_command(0x00);
    onfi.send_addresses(address, static_cast<uint8_t>(onfi.num_column_cycles + onfi.num_row_cycles));
    onfi.send_command(0x30);

    const BusyWindow busy = measure_busy_cycle(kBusyAssertTimeoutNs, kDefaultBusyTimeoutNs);

    if (fetch_data) {
        onfi.get_data(destination, static_cast<uint16_t>(length));
    }
    const uint8_t status = onfi.get_status();

    return make_timing(busy, status);
}

} // namespace onfi::timed
