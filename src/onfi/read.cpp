#include "onfi_interface.hpp"
#include "gpio.hpp"
#include "timing.hpp"
#include <cstdint>
#include "logging.hpp"
#include "onfi/controller.hpp"

void onfi_interface::read_page(unsigned int my_block_number, unsigned int my_page_number, uint8_t address_length,
                               bool verbose) {
    const uint8_t expected_length = static_cast<uint8_t>(num_column_cycles + num_row_cycles);
    if (address_length != expected_length) {
        LOG_ONFI_WARN("Address length mismatch (requested %u, expected %u); using expected length",
                      static_cast<unsigned int>(address_length),
                      static_cast<unsigned int>(expected_length));
        address_length = expected_length;
    }
    uint8_t address[8] = {0};
    convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, address, verbose);

#if PROFILE_TIME
    time_info_file << "Reading page:..";
    uint64_t start_time = get_timestamp_ns();
#endif

    onfi::OnfiController ctrl(*this);
    const bool pre_zero = (flash_chip == toshiba_tlc_toggle);
    ctrl.page_read(address, address_length, pre_zero);

#if PROFILE_TIME
    uint64_t end_time = get_timestamp_ns();
    if (verbose) fprintf(stdout, "Read page completed \n");
    time_info_file << "  took " << (end_time - start_time) / 1000 << " microseconds\n";
#endif
}

// this function opens a file named time_info_file.txt
// .. this file will log all the duration from the timing operations as necessary
void onfi_interface::change_read_column(const uint8_t *col_address) {
    onfi::OnfiController ctrl(*this);
    ctrl.change_read_column(col_address);
}
