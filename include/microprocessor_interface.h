#ifndef MICROPROCESSOR_INTERFACE
#define MICROPROCESSOR_INTERFACE

#include <stdint.h>
#include <fstream>
#include <iostream>
#include "gpio.h"
#include "hardware_locations.h"

/// following flag keeps the debug information active
#define DEBUG_INTERFACE false

/**
our code considers the interface to be asynchronous by deafult
.. but some of the memory do not support asynchronous interface
.. for ex: TOSHIBA TLC chip. They support toggle.
.. this enum handles that
*/
enum default_interface_type {
    asynchronous = 0,
    toggle = 1
};

/**
this enum handles case for some special cases, like TOSHIBA TLC chip where program operation is different than other TLC chip
*/
enum chip_type {
    default_async = 0,
    micron_mlc = 1,
    micron_tlc = 2,
    toshiba_tlc_toggle = 3
};

/**
this interface class defines the pins, location etc required
.. this may change based on system in use
.. please verfiy and change its locations in hardware_locations.h
*/
class interface {
private:
    /**
    define masks and addresses here
    */
protected:
    std::fstream time_info_file;

    // this is the debug file that logs debug messages for hardware interface
    std::fstream interface_debug_file;

public:
    interface() {
        if (!gpio_init()) {
            std::cerr << "GPIO initialisation failed\n";
            exit(1);
        }
    }

    ~interface() {
        // The bcm2835 library doesn't require explicit termination in the same way as pigpio
    }


    default_interface_type interface_type;
    chip_type flash_chip;

    /**
    this function opens a file to log debug information for interface
    */
    void open_interface_debug_file();

    /**
    this function closes the debug file that was open for logging
    */
    void close_interface_debug_file(bool verbose = false);

    /**
    this function turns on the LEDs
    .. can be used as an indicator
    */
    void turn_leds_on();

    /**
    this function turns off the LEDs
    */
    void turn_leds_off();

    /**
    this function is to test the LEDs
    .. lights up the loweest LED and slowly light up others after a delay
    */
    void test_leds_increment(bool verbose = false);

    /**
    we do not touch the DQ pin values here
    .. since CE#, RE# and WE# are active low, we will set them to 1
    .. since ALE and CLE are active high, we will reset them to 0
    ..
    */
    void set_ce_low() const;

    /**
    this function sets the default values of pins.
    we do not touch the DQ pin values here
    .. since CE#, RE# and WE# are active low, we will set them to 1
    .. since ALE and CLE are active high, we will reset them to 0
    */
    void set_default_pin_values() const;

    /**
    function that sets the data lines as input
    .. to be used when data is to be received from NAND
    ... please do not forget to reset them to output once done
    */
    void set_datalines_direction_input() const;

    /**
    function that sets the data lines as output/default
    .. to be used when sending data or sending command
    ... this function must be called once the datalines are set as input
    */
    void set_datalines_direction_default() const;

    /**
    function to initialize the data and command lines all in inactive state
    .. here the data lines are output for MCU and all other lines as well
    ... set data lines as input right before when needed
    R/B signal will be set as input
    */
    void set_pin_direction_inactive() const;

    /**
    \function to send an arbitrary command signal to the NAND device
    \.. the procedure is as follows ( in the sequence )
    \.. Write Enable should go low WE => low
    \.. reset the bit that is connected to WE
    \.. Chip Enable should go low CE => low
    \.. ALE should go low ALE => low
    \.. ALE should be zero from before
    \..RE goes high
    \..RE should be high from before
    \.. CLE should go high CLE => high
    \.. send the command signal in the DQ pins
    \.. the idea is clear the least 8-bits
    \.. copy the values to be sent
    \.. ..the first part reset the DQ pins
    \.. ..the second part has the actual command to send
    \insert delay here
    \disable write enable again
    \insert delay here
    \.. because the command is written on the rising edge of WE
    \tDH = 20 ns
    \HOLD_TIME;
    \disable CLE
    \make sure to call set_default_pin_values()
    */
    void send_command(uint8_t command_to_send) const;

    /**
    \make this function inline in definition
    \param: address_to_send is the address value to be sent to the NAND device
    \num_address_bytes is the number of bytes in the address

    \.. CLE goes low
    \.. CLE should be 0 from before
    \.. ALE goes high
    \.. RE goes high
    \.. RE should be high from before
    \.. Put data on the DQ pin
    \.. .. the idea is clear the least 8-bits
    \.. .. copy the values to be sent
    \.. a simple delay
    \.. Address is loaded from DQ on rising edge of WE
    \.. maintain WE high for certain duration and make it low

    \.. put next address bits on DQ and cause rising edge of WE
    \.. address expected is 5-bytes ColAdd1, ColAdd2, RowAdd1, RowAdd2, RowAdd3

    \insert delay here
    \make sure to call set_default_pin_values()
    */
    void send_addresses(uint8_t *address_to_send, uint8_t num_address_bytes = 1, bool verbose = false) const;

    /**
    \send data to the flash memory
    \first parameter is the array to be sent
    \second parameter is the number of iterms in the array above
    \.. CE should be low
    \.. make WE low and repeat the procedure again for number of bytes required (int num_data)
    \.. put data on DQ and latch WE high for certain duration
    \.. .. the idea is clear clear the least 8-bits
    \.. .. copy the values to be sent
    \.. a simple delay
    \*jumper_address |= WE_mask;

    \insert delay here
    \HOLD_TIME;	//tDH
    \reset all the data on DQ pins
    \.. this might be unnecesary
    \make sure to call set_default_pin_values()
    */
    void send_data(uint8_t *data_to_send, uint16_t num_data) const;

    void set_dq_pins(uint8_t data) const;

    uint8_t read_dq_pins() const;

    // Wait for Ready/Busy (R/B#) to indicate ready (high). Returns true if
    // ready before timeout, false on timeout. Uses CLOCK_MONOTONIC_RAW.
    bool wait_ready(uint64_t timeout_ns) const;
};

#endif
