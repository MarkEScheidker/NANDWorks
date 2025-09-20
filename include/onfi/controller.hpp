#ifndef ONFI_CONTROLLER_H
#define ONFI_CONTROLLER_H

#include <stdint.h>
#include "onfi_interface.hpp" // relies on get_data/get_status; rehost later

namespace onfi {

// Thin wrapper around low-level ONFI command sequences.
// Delegates transport to the provided onfi_interface (HAL + data I/O).
class OnfiController {
    onfi_interface& hal_;
public:
    explicit OnfiController(onfi_interface& hal) : hal_(hal) {}

    void reset();

    void page_read(const uint8_t* addr, uint8_t addr_len, bool pre_zero_cmd = false);
    void change_read_column(const uint8_t* col2bytes);
    void prefix_command(uint8_t cmd); // send a single-byte prefix command

    void program_page(const uint8_t* addr5, const uint8_t* data, uint32_t len);
    void program_page_confirm(const uint8_t* addr5, const uint8_t* data, uint32_t len, uint8_t confirm_cmd);
    void erase_block(const uint8_t* row3);
    void partial_erase_block(const uint8_t* row3, uint32_t loop_count);

    void set_features(uint8_t address, const uint8_t data[4], uint8_t command = 0xEF);
    void get_features(uint8_t address, uint8_t out[4], uint8_t command = 0xEE) const;

    void read_data(uint8_t* dst, uint16_t n) const;
    uint8_t get_status() const;
};

} // namespace onfi

#endif // ONFI_CONTROLLER_H
