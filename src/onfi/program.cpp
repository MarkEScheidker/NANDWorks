#include "onfi_interface.hpp"
#include "gpio.hpp"
#include "timing.hpp"
#include <stdio.h>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <limits>
#include "logging.hpp"
#include "onfi/controller.hpp"

bool onfi_interface::verify_program_page(unsigned int my_block_number, unsigned int my_page_number,
                                         uint8_t *data_to_program, bool verbose, int max_allowed_errors) {
    
    
    //uint16_t num_bytes_to_test = num_bytes_in_page+num_spare_bytes_in_page;
    uint16_t num_bytes_to_test = num_bytes_in_page;

    uint8_t *data_read_from_page = ensure_scratch(num_bytes_to_test);
    //first let us get the data from the page to the cache memory
    read_page(my_block_number, my_page_number, 5);
    // now let us get the values from the cache memory to our local variable
    get_data(data_read_from_page, num_bytes_to_test);

    //now iterate through each of them to see if they are correct
    uint16_t byte_id = 0;
    uint16_t byte_fail_count = 0;
    uint32_t bit_fail_count = 0;
    for (byte_id = 0; byte_id < (num_bytes_to_test); byte_id++) {
        uint8_t diff = data_read_from_page[byte_id] ^ data_to_program[byte_id];
        if (diff) {
            byte_fail_count++;
            bit_fail_count += static_cast<uint32_t>(__builtin_popcount(diff));

            if (verbose) {
#if DEBUG_ONFI
                if (onfi_debug_file) {
                    onfi_debug_file << std::hex << byte_id << "," << std::hex << 0 << "," << std::hex <<
                            data_read_from_page[byte_id] << std::endl;
                } else fprintf(stdout, "P:%x,%x,%x\n", byte_id, 0, data_read_from_page[byte_id]);
#else
					fprintf(stdout,"P:%x,%x,%x\n",byte_id,0,data_read_from_page[byte_id]);
#endif
            }
        }
    }
    if (byte_fail_count) {
        std::cout << "For page " << my_page_number << " of block " << my_block_number
                << ", program operation failed at " << std::dec << byte_fail_count << " bytes (" << std::fixed << std::setprecision(2)
                << (static_cast<double>(byte_fail_count) * 100.0 / num_bytes_to_test)
                << "%) and " << bit_fail_count << " bits." << std::endl;
    } else {
        std::cout << "For page " << my_page_number << " of block " << my_block_number
                << ", program operation did not fail." << std::endl;
    }

    fflush(stdout);

    return byte_fail_count <= max_allowed_errors;
}

// this function only programs a single page as indicated by the address provided
// for the arguments,
// .. page_address is the address of the page 5-byte address
// .. data_to_progra is the data to write
// .. including_spare is to indicate if you want to program the spare section as well
// .. .. if including spare  = 1, then length of data_to_program should be (num of bytes in pages + num of spare bytes)
// .. verbose is for priting messages
void onfi_interface::program_page(unsigned int my_block_number, unsigned int my_page_number, uint8_t *data_to_program,
                                  bool including_spare, bool verbose) {
    uint8_t page_address[8] = {0};
    // following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
    convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, page_address, verbose);

    enable_erase();

#if DEBUG_ONFI
    if (onfi_debug_file) {
        char my_temp[100];
        sprintf(my_temp, "Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.", page_address[0], page_address[1],
                page_address[2], page_address[3], page_address[4]);
        onfi_debug_file << my_temp << std::endl;
    } else
        fprintf(stdout, "Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.", page_address[0], page_address[1],
                page_address[2], page_address[3], page_address[4]);
#else
	if(verbose) fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#endif

    onfi::OnfiController ctrl(*this);
    ctrl.program_page(page_address, data_to_program, including_spare ? (num_bytes_in_page + num_spare_bytes_in_page) : (num_bytes_in_page));

#if PROFILE_TIME
    time_info_file << "program page: ";
    uint64_t start_time = get_timestamp_ns();
#endif
        /* busy handled by controller */
#if PROFILE_TIME
    uint64_t end_time = get_timestamp_ns();
    time_info_file << "  took " << (end_time - start_time) / 1000 << " microseconds\n";
#endif

    uint8_t status = 0;
    status = ctrl.get_status();
    // .. use  the commended code for multi-plane die
    // read_status_enhanced(&status_value,(address+2));
    if (status & 0x20) {
        if (status & 0x01) {
            fprintf(stdout, "Failed Program Operation: %d,%d,%d\n", page_address[2], page_address[3], page_address[4]);
        } else {
#if DEBUG_ONFI
            if (onfi_debug_file) onfi_debug_file << "Program Operation Completed" << std::endl;
            else fprintf(stdout, "Program Operation Completed\n");
#else
	if(verbose) fprintf(stdout,"Program Operation Completed\n");
#endif
        }
    } else {
#if DEBUG_ONFI
        if (onfi_debug_file) onfi_debug_file << "Program Operation, should not be here" << std::endl;
        else fprintf(stdout, "Program Operation, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Program Operation, should not be here\n");
#endif
    }

    disable_erase();
}

// provide the subpage number to the page of TLC subpage to program i3 subpages independently
// my subpage number should be eihter 1, 2 or 3
// program all three subpages sequentially, meaning, subpage1, then subpage2 and then subpage3
void onfi_interface::program_page_tlc_toshiba_subpage(unsigned int my_block_number, unsigned int my_page_number,
                                                      unsigned int my_subpage_number, uint8_t *data_to_program,
                                                      bool including_spare, bool verbose) {
    if (flash_chip != toshiba_tlc_toggle) {
        fprintf(stdout, "E: Trying to programm using TOSHIBA TLC function. Not allowed.\n");
        exit(-1);
    }


    uint8_t page_address[5] = {0, 0, 0, 0, 0};
    // following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
    convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, page_address, verbose);

    // write program routine here
    enable_erase();

#if DEBUG_ONFI
    if (onfi_debug_file) {
        char my_temp[100];
        sprintf(my_temp, "Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.", page_address[0], page_address[1],
                page_address[2], page_address[3], page_address[4]);
        onfi_debug_file << my_temp << std::endl;
    } else
        fprintf(stdout, "Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.", page_address[0], page_address[1],
                page_address[2], page_address[3], page_address[4]);
#else
	if(verbose) fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#endif

    // for first subpage
    send_command((uint8_t) my_subpage_number);
    send_command(0x80);
    send_addresses(page_address, 5);

    

    send_data(data_to_program, including_spare ? (num_bytes_in_page + num_spare_bytes_in_page) : (num_bytes_in_page));

#if PROFILE_TIME
    time_info_file << "program page: ";
    uint64_t start_time = get_timestamp_ns();
#endif
        if (my_subpage_number < 3) send_command(0x1a);
        else send_command(0x10);

        
        // check if it is out of Busy cycle
        wait_ready_blocking();
#if PROFILE_TIME
    uint64_t end_time = get_timestamp_ns();
    time_info_file << "  took " << (end_time - start_time) / 1000 << " microseconds\n";
#endif

    uint8_t status = 0;
    status = get_status();
    // .. use  the commended code for multi-plane die
    // read_status_enhanced(&status_value,(address+2));
    if (status & 0x20) {
        if (status & 0x01) {
            fprintf(stdout,
                    "Failed Program Operation of %u subpage: %d,%d,%d\n",
                    my_subpage_number,
                    page_address[2], page_address[3], page_address[4]);
        } else {
#if DEBUG_ONFI
            if (onfi_debug_file) onfi_debug_file << "Program Operation Completed of " << my_subpage_number << " subpage"
                                 << std::endl;
            else fprintf(stdout, "Program Operation Completed of %u subpage\n", my_subpage_number);
#else
        if(verbose) fprintf(stdout,"Program Operation Completed of %u subpage\n", my_subpage_number);
#endif
        }
    } else {
#if DEBUG_ONFI
        if (onfi_debug_file) onfi_debug_file << "Program Operation of " << my_subpage_number <<
                             " subpage, should not be here" << std::endl;
        else fprintf(stdout, "Program Operation of %u subpage, should not be here\n", my_subpage_number);
#else
        if(verbose) fprintf(stdout,"Program Operation of %u subpage, should not be here\n", my_subpage_number);
#endif
    }

    if (my_subpage_number >= 3) {
        disable_erase();
    }
}

void onfi_interface::program_page_tlc_toshiba(unsigned int my_block_number, unsigned int my_page_number,
                                              uint8_t *data_to_program, bool including_spare, bool verbose) {
    if (flash_chip != toshiba_tlc_toggle) {
        fprintf(stdout, "E: Trying to programm using TOSHIBA TLC function. Not allowed.\n");
        exit(-1);
    }


    uint8_t page_address[5] = {0, 0, 0, 0, 0};
    // following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
    convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, page_address, verbose);

    // write program routine here
    enable_erase();

#if DEBUG_ONFI
    if (onfi_debug_file) {
        char my_temp[100];
        sprintf(my_temp, "Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.", page_address[0], page_address[1],
                page_address[2], page_address[3], page_address[4]);
        onfi_debug_file << my_temp << std::endl;
    } else
        fprintf(stdout, "Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.", page_address[0], page_address[1],
                page_address[2], page_address[3], page_address[4]);
#else
	if(verbose) fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#endif
    const size_t total_payload = including_spare ? static_cast<size_t>(num_bytes_in_page) + num_spare_bytes_in_page
                                                 : static_cast<size_t>(num_bytes_in_page);
    const size_t slice_size = total_payload / 3;
    const bool slice_payload = (total_payload % 3 == 0) && (slice_size <= std::numeric_limits<uint16_t>::max());
    auto load_subpage_payload = [&](unsigned index, uint16_t &length) -> uint8_t* {
        if (!slice_payload) {
            length = static_cast<uint16_t>(total_payload);
            return data_to_program;
        }
        length = static_cast<uint16_t>(slice_size);
        return data_to_program + slice_size * index;
    };
    // for first subpage
    send_command(0x01);
    send_command(0x80);
    send_addresses(page_address, 5);

    

    uint16_t chunk_length = 0;
    uint8_t* chunk_ptr = load_subpage_payload(0, chunk_length);
    send_data(chunk_ptr, chunk_length);

#if PROFILE_TIME
    time_info_file << "program page: ";
    uint64_t start_time = get_timestamp_ns();
#endif
        send_command(0x1a);

        
        // check if it is out of Busy cycle
        wait_ready_blocking();
#if PROFILE_TIME
    uint64_t end_time = get_timestamp_ns();
    time_info_file << "  took " << (end_time - start_time) / 1000 << " microseconds\n";
#endif

    uint8_t status = 0;
    status = get_status();
    // .. use  the commended code for multi-plane die
    // read_status_enhanced(&status_value,(address+2));
    if (status & 0x20) {
        if (status & 0x01) {
            fprintf(stdout, "Failed Program Operation of first subpage: %d,%d,%d\n", page_address[2], page_address[3],
                    page_address[4]);
        } else {
            LOG_ONFI_INFO_IF(verbose, "Program Operation Completed of first subpage");
        }
    } else {
#if DEBUG_ONFI
        if (onfi_debug_file) onfi_debug_file << "Program Operation of first subpage, should not be here" << std::endl;
        else fprintf(stdout, "Program Operation of first subpage, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Program Operation of first subpage, should not be here\n");
#endif
    }

    // for second subpage
    send_command(0x02);
    send_command(0x80);
    send_addresses(page_address, 5);

    

    chunk_ptr = load_subpage_payload(1, chunk_length);
    send_data(chunk_ptr, chunk_length);

#if PROFILE_TIME
    time_info_file << "program page of second subpage: ";
    uint64_t start_time2 = get_timestamp_ns();
#endif
        send_command(0x1a);

        
        // check if it is out of Busy cycle
        while (gpio_read(GPIO_RB) == 0);
#if PROFILE_TIME
    uint64_t end_time2 = get_timestamp_ns();
    time_info_file << "  took " << (end_time2 - start_time2) / 1000 << " microseconds\n";
#endif

    status = get_status();
    // .. use  the commended code for multi-plane die
    // read_status_enhanced(&status_value,(address+2));
    if (status & 0x20) {
        if (status & 0x01) {
            fprintf(stdout, "Failed Program Operation of second subpage: %d,%d,%d\n", page_address[2], page_address[3],
                    page_address[4]);
        }
        else {
            LOG_ONFI_INFO_IF(verbose, "Program Operation Completed of second subpage");
        }
    } else {
#if DEBUG_ONFI
        if (onfi_debug_file) onfi_debug_file << "Program Operation of second subpage, should not be here" << std::endl;
        else fprintf(stdout, "Program Operation of second subpage, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Program Operation of second subpage, should not be here\n");
#endif
    }

    // for third subpage
    send_command(0x03);
    send_command(0x80);
    send_addresses(page_address, 5);

    

    chunk_ptr = load_subpage_payload(2, chunk_length);
    send_data(chunk_ptr, chunk_length);

#if PROFILE_TIME
    time_info_file << "program page of third subpage: ";
    uint64_t start_time3 = get_timestamp_ns();
#endif
        send_command(0x10);

        
        // check if it is out of Busy cycle
        while (gpio_read(GPIO_RB) == 0);
#if PROFILE_TIME
    uint64_t end_time3 = get_timestamp_ns();
    time_info_file << "  took " << (end_time3 - start_time3) / 1000 << " microseconds\n";
#endif

    status = get_status();
    // .. use  the commended code for multi-plane die
    // read_status_enhanced(&status_value,(address+2));
    if (status & 0x20) {
        if (status & 0x01) {
            fprintf(stdout, "Failed Program Operation of third subpage: %d,%d,%d\n", page_address[2], page_address[3],
                    page_address[4]);
        } else {
            LOG_ONFI_INFO_IF(verbose, "Program Operation Completed of third subpage");
        }
    } else {
#if DEBUG_ONFI
        if (onfi_debug_file) onfi_debug_file << "Program Operation of third subpage, should not be here" << std::endl;
        else fprintf(stdout, "Program Operation of third subpage, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Program Operation of third subpage, should not be here\n");
#endif
    }
    disable_erase();
}

void onfi_interface::partial_program_page(unsigned int my_block_number, unsigned int my_page_number,
                                          uint32_t loop_count, uint8_t *data_to_program, bool including_spare,
                                          bool verbose) {
    uint8_t page_address[5] = {0, 0, 0, 0, 0};
    // following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
    convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, page_address, verbose);

    enable_erase();

#if DEBUG_ONFI
    if (onfi_debug_file) {
        char my_temp[100];
        sprintf(my_temp, "Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.", page_address[0], page_address[1],
                page_address[2], page_address[3], page_address[4]);
        onfi_debug_file << my_temp << std::endl;
    } else
        fprintf(stdout, "Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.", page_address[0], page_address[1],
                page_address[2], page_address[3], page_address[4]);
#else
	if(verbose) fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#endif

    send_command(0x80);
    send_addresses(page_address, 5);

    

    send_data(data_to_program, including_spare ? (num_bytes_in_page + num_spare_bytes_in_page) : (num_bytes_in_page));

#if PROFILE_TIME
    time_info_file << "program page: ";
    uint64_t start_time = get_timestamp_ns();
#endif
        send_command(0x10);

        // tWB;
        // let us introduce a delay
        delay_function(loop_count);

        // let us issue reset command here
        send_command(0xff);

#if PROFILE_TIME
    uint64_t end_time = get_timestamp_ns();
    time_info_file << "  took " << (end_time - start_time) / 1000 << " microseconds\n";
#endif
    // check if it is out of Busy cycle
    wait_ready_blocking();

    uint8_t status = 0;
    status = get_status();
    // .. use  the commended code for multi-plane die
    // read_status_enhanced(&status_value,(address+2));
    if (status & 0x20) {
        if (status & 0x01) {
            fprintf(stdout, "Failed Program Operation\n");
        } else {
#if DEBUG_ONFI
            if (onfi_debug_file) onfi_debug_file << "Program Operation Completed" << std::endl;
            else fprintf(stdout, "Program Operation Completed\n");
#else
	if(verbose) fprintf(stdout,"Program Operation Completed\n");
#endif
        }
    } else {
#if DEBUG_ONFI
        if (onfi_debug_file) onfi_debug_file << "Program Operation, should not be here" << std::endl;
        else fprintf(stdout, "Program Operation, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Program Operation, should not be here\n");
#endif
    }

    disable_erase();
}

// let us program 512 pages in the block with all 0s
// the paramters are:
// .. my_test_block_address is the address of the block (starting address)
// .. num_pages is the number of pages in the page_indices array
// .. verbose is for printing
 

// this function will program the pages in a block with an array provided by user
// .. if the user wants to give an array
// .. .. it is indicated through bool array_provided
// .. .. the actual array is supposed to be in uint8_t* provided_array (lenght should be total number of bytes in a page)
 

// let us program pages in the block with all uint8_t* provided_data
// the paramters are:
// .. my_test_block_address is the address of the block (starting address)
// .. complete_block if this is true all the pages in the block will be programmed
// .. page_indices is an array of indices inside the block that we want to program
// .. num_pages is the number of pages in the page_indices array
// .. verbose is for printing
 

 

// let us verify program pages in the block
// the paramters are:
// .. my_test_block_address is the address of the block (starting address)
// .. complete_block if this is true all the pages in the block will be verified
// .. page_indices is an array of indices inside the block that we want to verify
// .. num_pages is the number of pages in the page_indices array
// .. verbose is for printing
 

// this function goes through the block and verifies program values in all of
// .. 512 pages in a block
 

// this function reads the single page address provided
// each value is hex while the sequence is terminated by newline character
