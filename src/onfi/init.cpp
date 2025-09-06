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

void onfi_interface::get_started(param_type ONFI_OR_JEDEC) {
    bool bytewise = true;
    initialize_onfi();

    /**now that the object is created
    .. the object should go and initialize itself by looking at the addresses and map to them
    .. let us test if the LEDs test will succeed
    */
    test_onfi_leds();

#if PROFILE_TIME
    open_time_profile_file();
#endif

    /**for the NAND chip, let us initialize the chip
    .. this should follow a pattern of power cycle as mentioned in the datasheet
    .. then we will do a reset operation
    */
    device_initialization();
    read_id();

    /**
    now let test the NAND chip and print its properties as read from the chip memory itself
    .. please make sure that the pin connections is appropriate as mentioned in hardware_locations.h
    .. this function call is also important to establish the parameters like:
    .. onfi_instance.num_bytes_in_page, onfi_instance.num_spare_bytes_in_page and num_pages_in_block
    */
    if (flash_chip == toshiba_tlc_toggle) {
        ONFI_OR_JEDEC = JEDEC;
        bytewise = false;
    }

    read_parameters(ONFI_OR_JEDEC, bytewise, true);

    // Set timing mode to 4 (25ns tRC/tWC)
    uint8_t timing_mode_data[4] = {0x04, 0x00, 0x00, 0x00};
    set_features(0x01, timing_mode_data, true);
}

// open_onfi_debug_file removed (legacy)

void onfi_interface::open_onfi_data_file() {
    onfi_data_file.open("data_file.txt", std::fstream::out);
}

void onfi_interface::open_time_profile_file() {
#if PROFILE_TIME
    time_info_file.open("time_info_file.txt", std::fstream::out);
#endif
}

// follow the following function call by get_data() function call
// .. please change this if the device has multiple dies
void onfi_interface::initialize_onfi(bool verbose) {
    if (verbose) LOG_ONFI_INFO("initialize_onfi() start");
    open_interface_debug_file(); // no-op (legacy)
    // open the data file
    open_onfi_data_file();

    // No need for bridge_base_virtual or fd with pigpio

    if (verbose) LOG_ONFI_INFO("GPIO and pin modes initialized");
}

void onfi_interface::deinitialize_onfi(bool verbose) {
#if DEBUG_ONFI
    if (onfi_debug_file.is_open()) {
        onfi_debug_file.close();
    }
#endif
    if (onfi_data_file.is_open()) {
        onfi_data_file.close();
    }
#if PROFILE_TIME
    if (time_info_file.is_open()) {
        time_info_file.close();
    }
#endif
    // No need for unmap_physical or close_physical with pigpio
}

// this function can be used to test the LEDs if they are properly set up
//  since it is called as a part of onfi_interface, if this works
// .. the interface is set up properly
void onfi_interface::test_onfi_leds(bool verbose) {
    if (verbose) LOG_ONFI_INFO("Testing LEDs");
    //just a simple delay for LEDs to stay ON
    turn_leds_on();
    busy_wait_ns(20000000);
    turn_leds_off();

    if (verbose) LOG_ONFI_INFO("Testing LEDs completed");
}

// function to receive data from the NAND device
// .. data is output from the cache regsiter of selected die
// .. it is supported following a read operation of NAND array
void onfi_interface::device_initialization(bool verbose) {
    if (verbose) LOG_ONFI_INFO("Initializing device with a reset cycle");
    if (verbose) LOG_ONFI_DEBUG("Setting pin direction inactive");
    set_pin_direction_inactive();
    if (verbose) LOG_ONFI_DEBUG("Setting default pin values");
    set_default_pin_values();

    // we need to set CE to low for RESET to work
    if (verbose) LOG_ONFI_DEBUG("Setting CE low");
    set_ce_low();

    // Wait for R/B signal to go high
    if (verbose) LOG_ONFI_DEBUG("Waiting for R/B signal to go high");
    wait_ready_blocking();

    // now issue RESET command
    if (verbose) LOG_ONFI_INFO("Issuing reset");
    reset_device(verbose);
    if (verbose) LOG_ONFI_INFO("Reset complete");
}

// function to reset the whole device
// .. issue a command 0xFF for reset operation
// .. following operations should be performed for this
// .. .. enable command cycle
// .. .. command to be sent is 0xFF
// .. .. check for R/B signal to be high after certain duration (should go low(busy) and go high (ready))
void onfi_interface::reset_device(bool verbose) {
    // oxff is reset command
    send_command(0xff);
    // No fixed delay; just wait for ready with a timeout
    wait_ready_blocking();
}
