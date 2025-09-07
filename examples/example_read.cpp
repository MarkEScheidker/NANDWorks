#include "onfi_interface.h"
#include "onfi/controller.h"
#include "onfi/device.h"
#include <vector>

int main() {
    onfi_interface onfi;
    onfi.get_started();

    onfi::OnfiController ctrl(onfi);
    onfi::NandDevice dev(ctrl);
    dev.geometry.page_size_bytes = onfi.num_bytes_in_page;
    dev.geometry.spare_size_bytes = onfi.num_spare_bytes_in_page;
    dev.geometry.pages_per_block = onfi.num_pages_in_block;
    dev.geometry.blocks_per_lun = onfi.num_blocks;
    dev.geometry.column_cycles = onfi.num_column_cycles;
    dev.geometry.row_cycles = onfi.num_row_cycles;
    dev.interface_type = onfi.interface_type;
    dev.chip = onfi.flash_chip;

    std::vector<uint8_t> buffer;
    dev.read_page(0, 0, /*including_spare*/false, /*bytewise*/false, buffer);
    return 0;
}
