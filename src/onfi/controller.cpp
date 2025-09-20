#include "onfi/controller.hpp"

namespace onfi {

void OnfiController::reset() {
    hal_.send_command(0xFF);
    hal_.wait_ready_blocking();
}

void OnfiController::page_read(const uint8_t* addr, uint8_t addr_len, bool pre_zero_cmd) {
    // Optional preface command for certain chips (e.g., Toshiba toggle variant)
    if (pre_zero_cmd) hal_.send_command(0x00);

    hal_.send_command(0x00);
    hal_.send_addresses(addr, addr_len);
    hal_.send_command(0x30);
    hal_.wait_ready_blocking();
}

void OnfiController::change_read_column(const uint8_t* col2bytes) {
    hal_.send_command(0x05);
    hal_.send_addresses(col2bytes, 2);
    hal_.send_command(0xE0);
}

void OnfiController::prefix_command(uint8_t cmd) {
    hal_.send_command(cmd);
}

void OnfiController::program_page(const uint8_t* addr5, const uint8_t* data, uint32_t len) {
    hal_.send_command(0x80);
    hal_.send_addresses(addr5, 5);
    hal_.send_data(data, static_cast<uint16_t>(len));
    hal_.send_command(0x10);
    hal_.wait_ready_blocking();
}

void OnfiController::program_page_confirm(const uint8_t* addr5, const uint8_t* data, uint32_t len, uint8_t confirm_cmd) {
    hal_.send_command(0x80);
    hal_.send_addresses(addr5, 5);
    hal_.send_data(data, static_cast<uint16_t>(len));
    hal_.send_command(confirm_cmd);
    hal_.wait_ready_blocking();
}

void OnfiController::erase_block(const uint8_t* row3) {
    hal_.send_command(0x60);
    hal_.send_addresses(row3, 3);
    hal_.send_command(0xD0);
    hal_.wait_ready_blocking();
}

void OnfiController::partial_erase_block(const uint8_t* row3, uint32_t loop_count) {
    hal_.send_command(0x60);
    hal_.send_addresses(row3, 3);
    hal_.send_command(0xD0);
    hal_.delay_function(loop_count);
    hal_.send_command(0xFF); // reset to terminate partial erase
    hal_.wait_ready_blocking();
}

void OnfiController::set_features(uint8_t address, const uint8_t data[4], uint8_t command) {
    hal_.send_command(command);
    hal_.send_addresses(&address, 1);
    hal_.send_data(data, 4);
    hal_.wait_ready_blocking();
}

void OnfiController::get_features(uint8_t address, uint8_t out[4], uint8_t command) const {
    hal_.send_command(command);
    hal_.send_addresses(&address, 1);
    hal_.wait_ready_blocking();
    hal_.get_data(out, 4);
}

void OnfiController::read_data(uint8_t* dst, uint16_t n) const {
    hal_.get_data(dst, n);
}

uint8_t OnfiController::get_status() const {
    return hal_.get_status();
}

} // namespace onfi
