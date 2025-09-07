#include "onfi_interface.h"
#include "gpio.h"
#include "timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include "logging.h"
#include "onfi/controller.h"

void onfi_interface::read_page(unsigned int my_block_number, unsigned int my_page_number, uint8_t address_length,
                               bool verbose) {
    if (address_length != num_column_cycles + num_row_cycles) {
        fprintf(stdout, "E: Address Length Mismatch.");
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
void onfi_interface::change_read_column(uint8_t *col_address) {
    onfi::OnfiController ctrl(*this);
    ctrl.change_read_column(col_address);
}

// function to disable Program and Erase operation
// .. when WP is low, program and erase operation are disabled
// .. when WP is high, program and erase operation are enabled
 

 

// this function reads the pages in the block
// .. since the complete data from the block might be too much data, the parameter num_pages can be used to limit the data
// .. the num_pages should indicate the number of pages in block starting from beginning
// .. verbose indicates the debug messages to be printed
 

// this function reads the pages in the block
// .. since the complete data from the block might be too much data, the parameter complete_block can be used to limit the data
// .. if complete_block is true, the compete block will be read
// .. if complete_block is false, the indices of the pages to be read cane be provided as an array
// .. if the indices of pages is used, the num_pages should indicate the numb er of pages listed in the array
// .. verbose indicates the debug messages to be printed
 

//  This function read the page specified by the index value in the
// .. block and puts the read value into the array passed " return_value" as argument
 

//The SET FEATURES (EFh) command writes the subfeature parameters (P1-P4) to the
// .. specified feature address to enable or disable target-specific features. This command is
// .. accepted by the target only when all die (LUNs) on the target are idle.
// .. the parameters P1-P4 are in data_to_send argument
