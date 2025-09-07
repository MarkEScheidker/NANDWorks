#include "onfi_interface.h"
#include "onfi/controller.h"
#include "onfi/device.h"
#include <iostream>
#include <string>
#include <cstring>

using namespace std;

static void usage(const char *prog) {
    cout << "Usage: " << prog << " [-v]" << endl;
}

int main(int argc, char **argv) {
    cout << "\n--- NAND Flash Chip Eraser ---" << endl;
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            verbose = true;
        else {
            usage(argv[0]);
            return 1;
        }
    }
    (void)verbose; // currently not used after migration

    onfi_interface onfi_instance;
    cout << "Initializing ONFI interface..." << endl;
    onfi_instance.get_started();
    cout << "ONFI interface initialization complete." << endl;

    cout << "Starting full chip erase..." << endl;
    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    dev.geometry.page_size_bytes = onfi_instance.num_bytes_in_page;
    dev.geometry.spare_size_bytes = onfi_instance.num_spare_bytes_in_page;
    dev.geometry.pages_per_block = onfi_instance.num_pages_in_block;
    dev.geometry.blocks_per_lun = onfi_instance.num_blocks;
    dev.geometry.column_cycles = onfi_instance.num_column_cycles;
    dev.geometry.row_cycles = onfi_instance.num_row_cycles;
    dev.interface_type = onfi_instance.interface_type;
    dev.chip = onfi_instance.flash_chip;

    for (unsigned int block = 0; block < onfi_instance.num_blocks; ++block) {
        cout << "Erasing block " << block << "/" << onfi_instance.num_blocks - 1 << endl;
        dev.erase_block(block);
    }
    cout << "Full chip erase complete." << endl;

    return 0;
}
