#include "onfi_interface.hpp"
#include <cstdint>
#include <cstdio>
#include "logging.hpp"
#include "onfi/controller.hpp"
#include "onfi/types.hpp"

void onfi_interface::disable_erase() {
    if (!erase_enabled_) {
        gpio_write(GPIO_WP, 0);
        return;
    }
    wait_ready_blocking();
    gpio_write(GPIO_WP, 0);
    erase_enabled_ = false;
}

void onfi_interface::enable_erase() {
    if (erase_enabled_) {
        gpio_write(GPIO_WP, 1);
        return;
    }
    wait_ready_blocking();
    gpio_write(GPIO_WP, 1);
    erase_enabled_ = true;
}

// following function erases the block address provided as the parameter to the function
// .. the address provided should be three 8-bit number array that corresponds to
// .. .. R1,R2 and R3 for the block to be erased
void onfi_interface::erase_block(unsigned int my_block_number, bool verbose) {
    uint8_t my_test_block_address[8] = {0};
    // following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
    convert_pagenumber_to_columnrow_address(my_block_number, 0, my_test_block_address, verbose);
    uint8_t *row_address = my_test_block_address + 2;

    enable_erase();
    gpio_set_direction(GPIO_RB, false);

    onfi::OnfiController ctrl(*this);
    // ensure ready then issue erase
    wait_ready_blocking();
    // Always perform erase; optionally time it
#if PROFILE_TIME
    uint64_t start_time = get_timestamp_ns();
#endif
    ctrl.erase_block(row_address);
#if PROFILE_TIME
    uint64_t end_time = get_timestamp_ns();
    time_info_file << "  took " << (end_time - start_time) / 1000 << " microseconds\n";
#endif

    LOG_ONFI_INFO_IF(verbose, "Inside Erase Fn: Address is: %02x,%02x,%02x.", row_address[0], row_address[1], row_address[2]);

    // let us read the status register value
    uint8_t status = 0;
    status = ctrl.get_status();
    if (status & 0x20) {
        if (status & 0x01) {
            if(verbose) fprintf(stdout, "Failed Erase Operation\n");
        } else {
            LOG_ONFI_INFO_IF(verbose, "Erase Operation Completed");
        }
    } else {
        LOG_ONFI_WARN_IF(verbose, "Erase Operation, should not be here");
    }

    disable_erase();
}


// following function erases the block address provided as the parameter to the function
// .. the address provided should be three 8-bit number array that corresponds to
// .. .. R1,R2 and R3 for the block to be erased
// .. loop_count is the partial erase times, a value of 10 will correspond to 1 ns delay
void onfi_interface::partial_erase_block(unsigned int my_block_number, unsigned int my_page_number, uint32_t loop_count,
                                         bool verbose) {
    uint8_t my_test_block_address[5];
    convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, my_test_block_address, verbose);
    uint8_t *row_address = my_test_block_address + 2;

    enable_erase();
    gpio_set_direction(GPIO_RB, false);

    onfi::OnfiController ctrl(*this);
    wait_ready_blocking();
#if PROFILE_TIME
    time_info_file << "Partial Erasing block: ";
    uint64_t start_time = get_timestamp_ns();
#endif
    ctrl.partial_erase_block(row_address, loop_count);
#if PROFILE_TIME
    uint64_t end_time = get_timestamp_ns();
    time_info_file << "  took " << (end_time - start_time) / 1000 << " microseconds\n";
#endif

    wait_ready_blocking();

    LOG_ONFI_INFO_IF(verbose, "Inside Erase Fn: Address is: %02x,%02x,%02x.", row_address[0], row_address[1], row_address[2]);

    // let us read the status register value
    uint8_t status = 0;
    status = ctrl.get_status();
    if (status & 0x20) {
        if (status & 0x01) {
            fprintf(stdout, "Failed Erase Operation\n");
        } else {
            LOG_ONFI_INFO_IF(verbose, "Erase Operation Completed");
        }
    } else {
        LOG_ONFI_WARN_IF(verbose, "Erase Operation, should not be here");
    }

    disable_erase();
}




// .. this function will read a random page and tries to verify if it was completely erased
// .. for elaborate verifiying, please use a different function


// this is the function that can be used to elaborately verify the erase operation
// .. the first argument is the address of the block
// .. the second argument defines if the complete block is to be verified (or a provided subset)
bool onfi_interface::verify_block_erase(unsigned int my_block_number, bool complete_block, const uint16_t *page_indices,
                                        uint16_t num_pages, bool verbose) {
    // just a placeholder for return value
    bool return_value = true;
    //uint16_t num_bytes_to_test = num_bytes_in_page+num_spare_bytes_in_page;
    uint16_t num_bytes_to_test = num_bytes_in_page;

    // let us create a local variable that will hold the data read from the pages
    uint8_t *data_read_from_page = ensure_scratch(num_bytes_to_test);
    uint16_t idx = 0;

    const bool check_full_block = complete_block || page_indices == nullptr || num_pages == 0;

    auto check_page = [&](uint16_t page_idx) {
        read_page(my_block_number, page_idx, 5);
        get_data(data_read_from_page, num_bytes_to_test);

        uint16_t fail_count = 0;
        for (uint16_t byte_id = 0; byte_id < num_bytes_to_test; ++byte_id) {
            if (data_read_from_page[byte_id] != 0xff) {
                fail_count++;
                return_value = false;

                if (verbose) {
#if DEBUG_ONFI
                    if (onfi_debug_file) {
                        onfi_debug_file << "E:" << std::hex << byte_id << "," << std::hex << page_idx << ","
                                << std::hex << data_read_from_page[byte_id] << std::endl;
                    } else fprintf(stdout, "E:%x,%x,%x\n", byte_id, page_idx, data_read_from_page[byte_id]);
#else
					fprintf(stdout,"E:%x,%x,%x\n",byte_id,page_idx,data_read_from_page[byte_id]);
#endif
                }
            }
        }
        if (fail_count) std::cout << "The number of bytes in page id " << page_idx << " where erase operation failed is " <<
                        std::dec << fail_count << std::endl;
    };

    if (check_full_block) {
        for (idx = 0; idx < num_pages_in_block; ++idx) {
            check_page(idx);
        }
    } else {
        for (idx = 0; idx < num_pages; ++idx) {
            check_page(page_indices[idx]);
        }
    }
    return return_value;
}


// this function only verifies a single page as indicated by the address provided
// for the arguments,
// .. page_address is the address of the page 5-byte address
// .. data_to_progra is the data expected
// .. including_spare is to indicate if you want to program the spare section as well
// .. .. if including spare  = 1, then length of data_to_program should be (num of bytes in pages + num of spare bytes)
// .. verbose is for priting messages
