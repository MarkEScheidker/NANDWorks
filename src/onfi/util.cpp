#include "onfi_interface.h"
#include "gpio.h"
#include "timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <algorithm>


/**
Following is the list of carefull chosen page indices in any block
.. please read "Testing Latency on Shared Pages" in readme.md in "sources/Data" folder
*/
uint16_t num_pages_selected = 100;
uint16_t page_indices_selected[] = {
    0, 2, 4, 6, 8, 10, 12, 14, 16, 18, //these are the pages at the beginning of the block (they are not shared)

    32, 34, 36, 38, 40, 42, 44, 46, 48, 50, // these are the pages at the beginning of shared location
    64, 68, 72, 76, 80, 84, 88, 92, 96, 100, // .. these are MSB pages to the pages above

    105, 113, 121, 129, 137, 145, 153, 161, 169, 177,
    // these are the pages that follow different relation for LSB and MSB pages
    168, 176, 184, 192, 200, 208, 216, 224, 232, 240, // .. these are MSB for above LSB pages

    601, 609, 617, 625, 633, 641, 649, 657, 665, 673, // these pages are somewhere in the middle of the block
    664, 672, 680, 688, 696, 704, 712, 720, 728, 736, // .. these pages are MSB to above LSB pages

    941, 943, 945, 947, 949, 951, 953, 955, 957, 959, // .. these are at the tail of shared pages
    1004, 1006, 1008, 1010, 1012, 1014, 1016, 1018, 1020, 1022, // .. these are MSB to above LSB pages

    1003, 1005, 1009, 1011, 1013, 1015, 1017, 1019, 1021, 1023
}; // .. these are unshared pages at the tail of the block



uint8_t onfi_interface::get_status() {
    // 0x70 is the command for getting status register value
    send_command(0x70);
    uint8_t status = 0;
    get_data(&status, 1);
    return status;
}

void onfi_interface::print_status_on_fail() {
    uint8_t status = get_status();
    if (status & 0x01) {
        if (onfi_debug_file)
            onfi_debug_file << "I: Last Operation failed" << std::endl;
        else
            std::cout << "I: Last Operation failed" << std::endl;
    }
}

//let us create a function that performs initialization
// .. opens ONFI debug file
// .. this function gets the bridge based address
// .. converts the peripheral addresses
// .. tests the LEDs
void onfi_interface::get_data(uint8_t *data_received, uint16_t num_data) const {
    if (interface_type == asynchronous) {
        set_default_pin_values();
        set_datalines_direction_input();

        // .. data can be received when on ready state (RDY signal)
        // .. ensure RDY is high
        wait_ready_blocking();
        
        // set CE to low to enable the chip
        gpio_write(GPIO_CE, 0);

        //Loop through the number of data to be received
        uint16_t i;
        for (i = 0; i < num_data; i++) {
            // set the RE to low for next cycle
            gpio_set_low(GPIO_RE);

            // read the data
            data_received[i] = read_dq_pins();

            // data is available at DQ pins on the rising edge of RE pin (RE is also input to NAND)
            gpio_set_high(GPIO_RE);
        }

        // set the pins as output
        set_datalines_direction_default();
        //make sure to call set_default_pin_values()
        set_default_pin_values();

    }else if (interface_type == toggle) {
        if (num_data % 2) {
            printf("E: In TOGGLE mode, num_data for data out cycle must be even number (currently is %d).\n", num_data);
            // printf(".. E: Program will terminate.\n");
            // exit(-1);
        }
        set_default_pin_values();
        set_datalines_direction_input();

        gpio_write(GPIO_CE, 0);
        busy_wait_ns(1000);

        // now take RE# to low
        gpio_write(GPIO_RE, 0);
        // now take DQS to low
        // set DQS as output
        gpio_write(GPIO_DQS, 1);
        gpio_set_direction(GPIO_DQS, true);
        gpio_set_direction(GPIO_DQSC, true);
        gpio_write(GPIO_DQS, 0);
        busy_wait_ns(1000);
        gpio_set_high(GPIO_RE);
        busy_wait_ns(1000);
        // now in a loop read the data
        uint16_t i = 0;
        for (i = 0; i < num_data; i += 1) {
            // read the data
            data_received[i] = read_dq_pins();

            gpio_write(GPIO_RE, !gpio_read(GPIO_RE)); // Toggle RE
            gpio_write(GPIO_DQS, !gpio_read(GPIO_DQS)); // Toggle DQS
        }
        gpio_write(GPIO_RE, 1);

        // set the pins as output
        set_datalines_direction_default();
        //make sure to call set_default_pin_values()
        set_default_pin_values();
    } else {
    }
}

// function to initialize the NAND device
// .. following are the tasks to be performed for initialization of NAND device
// .. provide Vcc (ie ramp Vcc)
// .. host must wait for R/B to be valid (R/B should be low for certain duration and be high
// .. .. to indicate device is ready) R/B is output from the NAND device
// .. issue 0xFF command after R/B goes high (Reset is the first command to be issued)
// .. R/B should be monotired again after issuing 0XFF command
void onfi_interface::convert_pagenumber_to_columnrow_address(unsigned int my_block_number, unsigned int my_page_number,
                                                             uint8_t *my_test_block_address, bool verbose) {
#if DEBUG_ONFI
    if (onfi_debug_file)
        onfi_debug_file << "I: Converting block: " << my_block_number << " and page: " << my_page_number <<
                " to {col,col,row,row,row} format." << std::endl;
    else
        std::cout << "I: Converting block: " << my_block_number << " and page: " << my_page_number <<
                " to {col,col,row,row,row} format." << std::endl;
#else
	if(verbose) std::cout<<"I: Converting block: "<<my_block_number<<" and page: "<<my_page_number<<" to {col,col,row,row,row} format." << std::endl;
#endif

    unsigned int page_number = (my_block_number * num_pages_in_block + my_page_number);

    for (uint8_t i = 0; i < num_column_cycles; i++) {
        my_test_block_address[i] = 0;
    }
    for (uint8_t i = num_column_cycles; i < (num_column_cycles + num_row_cycles); i++) {
        my_test_block_address[i] = (page_number & 0xff);
        page_number = page_number >> 8;
    }
#if DEBUG_ONFI
    if (onfi_debug_file)
        onfi_debug_file << "I: .. converted to " << (int) my_test_block_address[0] << "," << (int) my_test_block_address
                [1] << "," << (int) my_test_block_address[2] << "," << (int) my_test_block_address[3] << "," << (int)
                my_test_block_address[4] << std::endl;
    else
        std::cout << "I: .. converted to " << (int) my_test_block_address[0] << "," << (int) my_test_block_address[1] << ","
                << (int) my_test_block_address[2] << "," << (int) my_test_block_address[3] << "," << (int)
                my_test_block_address[4] << std::endl;
#else
	if(verbose) std::cout<<"I: .. converted to "<<(int)my_test_block_address[0]<<","<<(int)my_test_block_address[1]<<","<<(int)my_test_block_address[2]<<","<<(int)my_test_block_address[3]<<","<<(int)my_test_block_address[4]<<std::endl;
#endif
}

// following function reads the ONFI parameter page
// .. the ONFI parameter page consists of ONFI parameter data structure
// .. this data structure consists of critical information about the internal organization of memory
// .. the data structure is 256 bytes
// .. the data will be available for read with a command sequence of ECh followed by an address of 40h
// .. this function will read a random page and tries to verify if it was completely erased
// .. for elaborate verifiying, please use a different function
void onfi_interface::set_features(uint8_t address, uint8_t *data_to_send, uint8_t command) {
    // check if it is out of Busy cycle
    while (gpio_read(GPIO_RB) == 0);

    //send command
    send_command(command);
    //send the address
    send_addresses(&address);

    // now send the parameters
    send_data(data_to_send, 4);

    // check if it is out of Busy cycle
    while (gpio_read(GPIO_RB) == 0);
}

//The GET FEATURES (EEh) command reads the subfeature parameters (P1-P4) from the
// .. specified feature address. This command is accepted by the target only when all die
// .. (LUNs) on the target are idle.
// .. the parameters P1-P4 are in data_received argument
void onfi_interface::get_features(uint8_t address, uint8_t* data_received, uint8_t command) const {
    // check if it is out of Busy cycle
    while (gpio_read(GPIO_RB) == 0);

    //send command
    send_command(command);
    //send the address
    send_addresses(&address);

    // check if it is out of Busy cycle
    while (gpio_read(GPIO_RB) == 0);

    get_data(data_received, 4);
}

// following function will convert a block from MLC mode to SLC mode
// .. it uses set_features option to convert the block from SLC to MLC
__attribute__((always_inline)) void onfi_interface::delay_function(uint32_t loop_count) {
    for (; loop_count > 0; loop_count--)
        busy_wait_ns(1000);
}

void onfi_interface::profile_time() {
    uint32_t delay = 10000;
    for (; delay <= 60000; delay += 5000) {
#if PROFILE_DELAY_TIME
        time_info_file << "Delay times for " << delay << ": ";
        uint64_t start_time = get_timestamp_ns();
#endif
            delay_function(delay);
#if PROFILE_DELAY_TIME
        uint64_t end_time = get_timestamp_ns();
        time_info_file << "  took " << (end_time - start_time) / 1000 << " microseconds\n";
#endif
    }
}
