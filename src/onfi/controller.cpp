#include "onfi/controller.hpp"

namespace onfi {

void OnfiController::reset() {
    transport_.send_command(0xFF);
    transport_.wait_ready_blocking();
}

void OnfiController::page_read(const uint8_t* addr, uint8_t addr_len, bool pre_zero_cmd) {
    // Optional preface command for certain chips (e.g., Toshiba toggle variant)
    if (pre_zero_cmd) transport_.send_command(0x00);

    transport_.send_command(0x00);
    transport_.send_addresses(addr, addr_len);
    transport_.send_command(0x30);
    transport_.wait_ready_blocking();
}

void OnfiController::change_read_column(const uint8_t* col2bytes) {
    transport_.send_command(0x05);
    transport_.send_addresses(col2bytes, 2);
    transport_.send_command(0xE0);
}

void OnfiController::prefix_command(uint8_t cmd) {
    transport_.send_command(cmd);
}

void OnfiController::program_page(const uint8_t* addr5, const uint8_t* data, uint32_t len) {
    transport_.send_command(0x80);
    transport_.send_addresses(addr5, 5);
    transport_.send_data(data, static_cast<uint16_t>(len));
    transport_.send_command(0x10);
    transport_.wait_ready_blocking();
}

void OnfiController::program_page_confirm(const uint8_t* addr5, const uint8_t* data, uint32_t len, uint8_t confirm_cmd) {
    transport_.send_command(0x80);
    transport_.send_addresses(addr5, 5);
    transport_.send_data(data, static_cast<uint16_t>(len));
    transport_.send_command(confirm_cmd);
    transport_.wait_ready_blocking();
}

void OnfiController::erase_block(const uint8_t* row3) {
    transport_.send_command(0x60);
    transport_.send_addresses(row3, 3);
    transport_.send_command(0xD0);
    transport_.wait_ready_blocking();
}

void OnfiController::partial_erase_block(const uint8_t* row3, uint32_t loop_count) {
    transport_.send_command(0x60);
    transport_.send_addresses(row3, 3);
    transport_.send_command(0xD0);
    transport_.delay_function(loop_count);
    transport_.send_command(0xFF); // reset to terminate partial erase
    transport_.wait_ready_blocking();
}

void OnfiController::set_features(uint8_t address, const uint8_t data[4], FeatureCommand command) {
    transport_.send_command(static_cast<uint8_t>(command));
    transport_.send_addresses(&address, 1);
    transport_.send_data(data, 4);
    transport_.wait_ready_blocking();
}

void OnfiController::get_features(uint8_t address, uint8_t out[4], FeatureCommand command) const {
    transport_.send_command(static_cast<uint8_t>(command));
    transport_.send_addresses(&address, 1);
    transport_.wait_ready_blocking();
    transport_.get_data(out, 4);
}

void OnfiController::read_data(uint8_t* dst, uint16_t n) const {
    transport_.get_data(dst, n);
}

uint8_t OnfiController::get_status() {
    return transport_.get_status();
}

} // namespace onfi
