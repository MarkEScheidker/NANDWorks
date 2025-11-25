#include "onfi_interface.hpp"
#include "gpio.hpp"
#include "timing.hpp"
#include <bcm2835.h>
#include <cstdio>
#include <cstring>
#include "logging.hpp"
#include "onfi/controller.hpp"
#include "onfi/address.hpp"
#include "onfi/types.hpp"

uint8_t* onfi_interface::ensure_scratch(size_t size) {
    if (scratch_buffer_.size() < size) {
        scratch_buffer_.resize(size);
    }
    return scratch_buffer_.data();
}

uint8_t onfi_interface::get_status() {
    // 0x70 is the command for getting status register value
    send_command(0x70);
    uint8_t status = 0;
    get_data(&status, 1);
    return status;
}

void onfi_interface::print_status_on_fail() {
    uint8_t status = get_status();
    if (status & 0x01) {
        LOG_ONFI_WARN("Last Operation failed");
    }
}

void onfi_interface::get_data(uint8_t *data_received, uint16_t num_data) const {
    if (interface_type == asynchronous) {
        // Ensure caller knows: this will drive CE low and put DQ into input mode, then restore defaults.
        set_default_pin_values();
        set_datalines_direction_input();
        wait_ready_blocking();
        gpio_write(GPIO_CE, 0);

        // Loop through the number of data to be received
        for (uint16_t i = 0; i < num_data; ++i) {
            bcm2835_gpio_clr(GPIO_RE);              // drive RE low
            data_received[i] = read_dq_pins();      // read DQ
            bcm2835_gpio_set(GPIO_RE);              // latch on rising edge
        }

        set_datalines_direction_default();
        set_default_pin_values();

    } else if (interface_type == toggle) {
        if (num_data % 2) {
            std::printf("E: In TOGGLE mode, num_data for data out cycle must be even number (currently is %d).\n", num_data);
        }
        // Ensure caller knows: this will drive CE low and put DQ into input mode, then restore defaults.
        set_default_pin_values();
        set_datalines_direction_input();
        gpio_write(GPIO_CE, 0);

        // Initialize RE/DQS
        bcm2835_gpio_clr(GPIO_RE);
        bcm2835_gpio_set(GPIO_DQS);
        gpio_set_direction(GPIO_DQS, true);
        gpio_set_direction(GPIO_DQSC, true);
        bcm2835_gpio_clr(GPIO_DQS);
        bcm2835_gpio_set(GPIO_RE);

        bool re_level = true;
        bool dqs_level = false;
        for (uint16_t i = 0; i < num_data; ++i) {
            data_received[i] = read_dq_pins();

            re_level = !re_level;
            if (re_level) bcm2835_gpio_set(GPIO_RE);
            else bcm2835_gpio_clr(GPIO_RE);
            dqs_level = !dqs_level;
            if (dqs_level) bcm2835_gpio_set(GPIO_DQS);
            else bcm2835_gpio_clr(GPIO_DQS);
        }
        bcm2835_gpio_set(GPIO_RE);

        set_datalines_direction_default();
        set_default_pin_values();
    } else {
        // Unknown interface type; no-op.
    }
}

void onfi_interface::set_features(uint8_t address, const uint8_t *data_to_send, onfi::FeatureCommand command) {
    onfi::OnfiController ctrl(*this);
    ctrl.set_features(address, data_to_send, command);
}

void onfi_interface::get_features(uint8_t address, uint8_t* data_received, onfi::FeatureCommand command) const {
    onfi::OnfiController ctrl(const_cast<onfi_interface&>(*this));
    ctrl.get_features(address, data_received, command);
}

// Translate block/page to column/row address tuple for ONFI commands.
void onfi_interface::convert_pagenumber_to_columnrow_address(unsigned int my_block_number, unsigned int my_page_number,
                                                             uint8_t *my_test_block_address, bool verbose) {
    LOG_ONFI_DEBUG_IF(verbose, "Converting block %u page %u to {col,col,row,row,row}", my_block_number, my_page_number);
    onfi::to_col_row_address(
        static_cast<uint32_t>(num_pages_in_block),
        num_column_cycles,
        num_row_cycles,
        my_block_number,
        my_page_number,
        my_test_block_address);
    LOG_ONFI_DEBUG_IF(verbose, ".. converted to %d,%d,%d,%d,%d",
                      (int)my_test_block_address[0], (int)my_test_block_address[1], (int)my_test_block_address[2],
                      (int)my_test_block_address[3], (int)my_test_block_address[4]);
    (void)verbose;
}

void onfi_interface::delay_function(uint32_t loop_count) {
    if (loop_count == 0) return;
    const uint64_t total_wait_ns = static_cast<uint64_t>(loop_count) * 1000ULL;
    busy_wait_ns(total_wait_ns);
}

void onfi_interface::profile_time() {
    uint32_t delay = 10000;
    for (; delay <= 60000; delay += 5000) {
#if PROFILE_DELAY_TIME
        time_info_file << "Delay times for " << delay << ": ";
        uint64_t start_time = get_timestamp_ns();
#endif
        delay_function(delay);
#if PROFILE_DELAY_TIME
        uint64_t end_time = get_timestamp_ns();
        time_info_file << "  took " << (end_time - start_time) / 1000 << " microseconds\n";
#endif
    }
}
