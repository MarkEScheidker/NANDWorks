#ifndef ONFI_DEVICE_UTILS_HPP
#define ONFI_DEVICE_UTILS_HPP

#include "onfi/device.hpp"
#include "onfi_interface.hpp"

namespace onfi {

// Fill a NandDevice's geometry/interface fields from an initialised onfi_interface.
inline void populate_device(const ::onfi_interface& source, NandDevice& device) {
    device.geometry.page_size_bytes = source.num_bytes_in_page;
    device.geometry.spare_size_bytes = source.num_spare_bytes_in_page;
    device.geometry.pages_per_block = source.num_pages_in_block;
    device.geometry.blocks_per_lun = source.num_blocks;
    device.geometry.column_cycles = source.num_column_cycles;
    device.geometry.row_cycles = source.num_row_cycles;
    device.interface_type = source.interface_type;
    device.chip = source.flash_chip;
}

} // namespace onfi

#endif // ONFI_DEVICE_UTILS_HPP
