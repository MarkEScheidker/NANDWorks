#include "onfi/device_config.hpp"
#include "onfi_interface.hpp"

namespace onfi {

DeviceConfig make_device_config(const ::onfi_interface& source) {
    DeviceConfig config;
    config.geometry.page_size_bytes = source.num_bytes_in_page;
    config.geometry.spare_size_bytes = source.num_spare_bytes_in_page;
    config.geometry.pages_per_block = source.num_pages_in_block;
    config.geometry.blocks_per_lun = source.num_blocks;
    config.geometry.column_cycles = source.num_column_cycles;
    config.geometry.row_cycles = source.num_row_cycles;
    config.interface_type = source.interface_type;
    config.chip = source.flash_chip;
    return config;
}

} // namespace onfi
