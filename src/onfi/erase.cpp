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

void onfi_interface::disable_erase() {
    // check to see if the device is busy
    // .. wait if busy
    wait_ready_blocking();

    // wp to low
    gpio_write(GPIO_WP, 0);
}

void onfi_interface::enable_erase() {
    // check to see if the device is busy
    // .. wait if busy
    wait_ready_blocking();

    // wp to high
    gpio_write(GPIO_WP, 1);
}

// following function erases the block address provided as the parameter to the function
// .. the address provided should be three 8-bit number array that corresponds to
// .. .. R1,R2 and R3 for the block to be erased
void onfi_interface::erase_block(unsigned int my_block_number, bool verbose) {
    uint8_t my_test_block_address[5] = {0, 0, 0, 0, 0};
    // following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
    convert_pagenumber_to_columnrow_address(my_block_number, 0, my_test_block_address, verbose);
    uint8_t *row_address = my_test_block_address + 2;

    enable_erase();
    gpio_set_direction(GPIO_RB, false);

    // check if it is out of Busy cycle
    wait_ready_blocking();

    send_command(0x60);
    send_addresses(row_address, 3);
#if PROFILE_TIME
    uint64_t start_time = get_timestamp_ns();
#endif
        send_command(0xd0);

        // check if it is out of Busy cycle
        wait_ready_blocking();

#if PROFILE_TIME
    uint64_t end_time = get_timestamp_ns();
    time_info_file << "  took " << (end_time - start_time) / 1000 << " microseconds\n";
#endif

    LOG_ONFI_INFO_IF(verbose, "Inside Erase Fn: Address is: %02x,%02x,%02x.", row_address[0], row_address[1], row_address[2]);

    // let us read the status register value
    uint8_t status = 0;
    status = get_status();
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

    // check if it is out of Busy cycle
    wait_ready_blocking();

    send_command(0x60);
    send_addresses(row_address, 3);
#if PROFILE_TIME
    time_info_file << "Partial Erasing block: ";
    uint64_t start_time = get_timestamp_ns();
#endif
        send_command(0xd0);

        delay_function(loop_count);

        // let us issue reset command here
        send_command(0xff);

#if PROFILE_TIME
    uint64_t end_time = get_timestamp_ns();
    time_info_file << "  took " << (end_time - start_time) / 1000 << " microseconds\n";
#endif

    // check if it is out of Busy cycle
    wait_ready_blocking();

    LOG_ONFI_INFO_IF(verbose, "Inside Erase Fn: Address is: %02x,%02x,%02x.", row_address[0], row_address[1], row_address[2]);

    // let us read the status register value
    uint8_t status = 0;
    status = get_status();
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
bool onfi_interface::verify_block_erase_sample(unsigned int my_block_number, bool verbose) {
    // let us initialize a random seed
    srand(time(NULL));
    //pick a random page index to test to see if erase works
    uint16_t page_num_to_verify = rand() % num_pages_in_block;

    uint8_t my_test_block_address[5] = {0, 0, 0, 0, 0};
    // following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
    convert_pagenumber_to_columnrow_address(my_block_number, page_num_to_verify, my_test_block_address, verbose);
    uint8_t *page_address = my_test_block_address;


    // just a placeholder for return value
    bool return_value = true;


#if DEBUG_ONFI
    if (onfi_debug_file) {
        char my_temp[200];
        sprintf(my_temp, "Verifying erase operation on page no: %d (%02x,%02x,%02x,%02x,%02x)", page_num_to_verify,
                page_address[0], page_address[1], page_address[2], page_address[3], page_address[4]);
        onfi_debug_file << my_temp << std::endl;
    } else fprintf(stdout, "Verifying erase operation on page no: %d (%02x,%02x,%02x,%02x,%02x)\n", page_num_to_verify,
                   page_address[0], page_address[1], page_address[2], page_address[3], page_address[4]);
#else
	if(verbose) fprintf(stdout,"Verifying erase operation on page no: %d (%02x,%02x,%02x,%02x,%02x)\n",page_num_to_verify,page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#endif

    // let us read from that page and compare each value read from the page to 0xff
    uint8_t *data_read_from_page = new uint8_t[num_bytes_in_page + num_spare_bytes_in_page];

    //let us initialize the array to 0x00
    memset(data_read_from_page, 0x00, num_bytes_in_page + num_spare_bytes_in_page);

    //let us read the page data to the cache memory of flash
    read_page(my_block_number, page_num_to_verify, 5);
    //let us acquire the data from the cache to the local variable
    get_data(data_read_from_page, num_bytes_in_page + num_spare_bytes_in_page);

    //now let us iterate through all the bytes and check
    uint16_t byte_id = 0;
    uint16_t fail_count = 0;
    for (byte_id = 0; byte_id < (num_bytes_in_page + num_spare_bytes_in_page); byte_id++) {
        if (data_read_from_page[byte_id] != 0xff) {
            fail_count++;
            return_value = false;
            if (verbose) {
#if DEBUG_ONFI
                if (onfi_debug_file) {
                    onfi_debug_file << "E:" << std::hex << byte_id << "," << std::hex << page_num_to_verify << "," <<
                            std::hex << data_read_from_page[byte_id] << std::endl;
                } else fprintf(stdout, "E:%x,%x,%x\n", byte_id, page_num_to_verify, data_read_from_page[byte_id]);
#else
					fprintf(stdout,"E:%x,%x,%x\n",byte_id,page_num_to_verify,data_read_from_page[byte_id]);
#endif
            }
        }
    }
    if (fail_count) std::cout << "The number of bytes in page id " << page_num_to_verify <<
                    " where erase operation failed is " << std::dec << fail_count << std::endl;
    else std::cout << "Erase operation SUCCESS!!. Tested on page id " << page_num_to_verify << std::endl;

    delete[] data_read_from_page;
    return return_value;
}


// this is the function that can be used to elaborately verify the erase operation
// .. the first argument is the address of the block
// .. the second argument defines if the complete block is to be verified
// .. .. if the second argument is false, then the pages from page_indices_selected are taken
bool onfi_interface::verify_block_erase(unsigned int my_block_number, bool complete_block, uint16_t *page_indices,
                                        uint16_t num_pages, bool verbose) {
    // just a placeholder for return value
    bool return_value = true;
    //uint16_t num_bytes_to_test = num_bytes_in_page+num_spare_bytes_in_page;
    uint16_t num_bytes_to_test = num_bytes_in_page;

    // let us create a local variable that will hold the data read from the pages
    uint8_t *data_read_from_page = new uint8_t[num_bytes_to_test];
    uint16_t idx = 0;

    //test to see if the user wants the complete block or just the selected pages
    if (!complete_block) {
        // this means we are operating only on selected pages in the block
        // this means we are working on all the pages in the block
        // .. let us iterate through each pages in the block
        uint16_t curr_page_index = 0;
        for (idx = 0; idx < num_pages; idx++) {
            // let us grab the index from the array
            curr_page_index = page_indices[idx];

            // let us first reset all the values in the local variables to 0x00
            memset(data_read_from_page, 0x00, num_bytes_to_test);
            //first let us get the data from the page to the cache memory
            read_page(my_block_number, curr_page_index, 5);
            // now let us get the values from the cache memory to our local variable
            get_data(data_read_from_page, num_bytes_to_test);

            //now iterate through each of them to see if they are correct
            uint16_t byte_id = 0;
            uint16_t fail_count = 0;
            for (byte_id = 0; byte_id < (num_bytes_to_test); byte_id++) {
                if (data_read_from_page[byte_id] != 0xff) {
                    fail_count++;
                    return_value = false;

                    if (verbose) {
#if DEBUG_ONFI
                        if (onfi_debug_file) {
                            onfi_debug_file << "E:" << std::hex << byte_id << "," << std::hex << curr_page_index << ","
                                    << std::hex << data_read_from_page[byte_id] << std::endl;
                        } else fprintf(stdout, "E:%x,%x,%x\n", byte_id, curr_page_index, data_read_from_page[byte_id]);
#else
							fprintf(stdout,"E:%x,%x,%x\n",byte_id,curr_page_index,data_read_from_page[byte_id]);
#endif
                    }
                }
            }
            if (fail_count) std::cout << "The number of bytes in page id " << curr_page_index <<
                            " where erase operation failed is " << std::dec << fail_count << std::endl;
        }
    } else {
        // this means we are working on all the pages in the block
        // .. let us iterate through each pages in the block
        for (idx = 0; idx < num_pages_in_block; idx++) {
            // let us first reset all the values in the local variables to 0x00
            memset(data_read_from_page, 0x00, num_bytes_to_test);
            //first let us get the data from the page to the cache memory
            read_page(my_block_number, idx, 5);
            // now let us get the values from the cache memory to our local variable
            get_data(data_read_from_page, num_bytes_to_test);

            //now iterate through each of them to see if they are correct
            uint16_t byte_id = 0;
            uint16_t fail_count = 0;
            for (byte_id = 0; byte_id < (num_bytes_to_test); byte_id++) {
                if (data_read_from_page[byte_id] != 0xff) {
                    fail_count++;
                    return_value = false;

                    if (verbose) {
#if DEBUG_ONFI
                        if (onfi_debug_file) {
                            onfi_debug_file << "E:" << std::hex << byte_id << "," << std::hex << idx << "," << std::hex
                                    << data_read_from_page[byte_id] << std::endl;
                        } else fprintf(stdout, "E:%x,%x,%x\n", byte_id, idx, data_read_from_page[byte_id]);
#else
							fprintf(stdout,"E:%x,%x,%x\n",byte_id,idx,data_read_from_page[byte_id]);
#endif
                    }
                }
            }
            if (fail_count) std::cout << "The number of bytes in page id " << idx << " where erase operation failed is " <<
                            std::dec << fail_count << std::endl;
        }
    }
    delete[] data_read_from_page;
    return return_value;
}


// this function only verifies a single page as indicated by the address provided
// for the arguments,
// .. page_address is the address of the page 5-byte address
// .. data_to_progra is the data expected
// .. including_spare is to indicate if you want to program the spare section as well
// .. .. if including spare  = 1, then length of data_to_program should be (num of bytes in pages + num of spare bytes)
// .. verbose is for priting messages
void onfi_interface::convert_to_slc_set_features(unsigned int my_block_number) {
    uint8_t my_test_block_address[5] = {0, 0, 0, 0, 0};
    // following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
    convert_pagenumber_to_columnrow_address(my_block_number, 0, my_test_block_address, false);

    // to convert to SLC using set features, we have to first make sure
    // .. at least half the pages are programmed
    // .. let us just program all the pages in the block
    program_pages_in_a_block(my_block_number, true, false, nullptr, 0, false);

    // now that we have programmed the block,
    // .. we will issue set_features command
    uint8_t parameters[4] = {01, 01, 0, 0};
    // let us send the command for feature
    set_features(0x91, parameters);

    // now we must erase the block to initialize in SLC mode
    erase_block(my_block_number);

    // now the block should be in SLC mode
}

void onfi_interface::revert_to_mlc_set_features() {
    // .. we will issue set_features command
    uint8_t parameters[4] = {02, 01, 0, 0};
    // let us send the command for feature
    set_features(0x91, parameters);
}

void onfi_interface::convert_to_slc(unsigned int my_block_number, bool first_time) {
    uint8_t my_test_block_address[5] = {0, 0, 0, 0, 0};
    // following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
    convert_pagenumber_to_columnrow_address(my_block_number, 0, my_test_block_address, false);

    // to convert to SLC, we have to first make sure
    // .. at least half the pages are programmed
    // .. let us just program all the pages in the block
    if (first_time) {
        erase_block(my_block_number);
        program_pages_in_a_block(my_block_number, true, false, nullptr, 0, false);
    }

    send_command(0xda);
        
    send_command(0x60);
    send_addresses(my_test_block_address + 2);
    send_command(0xd0);
        
    while (gpio_read(GPIO_RB) == 0);;

    //perform erase operation to init
    if (first_time) erase_block(my_block_number);
}

void onfi_interface::revert_to_mlc(unsigned int my_block_number) {
    uint8_t my_test_block_address[5] = {0, 0, 0, 0, 0};
    // following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
    convert_pagenumber_to_columnrow_address(my_block_number, 0, my_test_block_address, false);

    send_command(0xdf);
        
    send_command(0x60);
    send_addresses(my_test_block_address + 2);
    send_command(0xd0);
        
    while (gpio_read(GPIO_RB) == 0);;
}
