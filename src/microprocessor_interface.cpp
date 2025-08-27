#include "microprocessor_interface.h"
#include "gpio.h"
#include "timing.h"
#include "hardware_locations.h"
#include <bcm2835.h>
#include <cstdint>
#include <iostream>

// Helper to set DQ pins
void interface::set_dq_pins(uint8_t data) const {
    uint32_t set_mask = 0;
    uint32_t clr_mask = 0;

    if ((data >> 0) & 0x01) set_mask |= (1 << GPIO_DQ0); else clr_mask |= (1 << GPIO_DQ0);
    if ((data >> 1) & 0x01) set_mask |= (1 << GPIO_DQ1); else clr_mask |= (1 << GPIO_DQ1);
    if ((data >> 2) & 0x01) set_mask |= (1 << GPIO_DQ2); else clr_mask |= (1 << GPIO_DQ2);
    if ((data >> 3) & 0x01) set_mask |= (1 << GPIO_DQ3); else clr_mask |= (1 << GPIO_DQ3);
    if ((data >> 4) & 0x01) set_mask |= (1 << GPIO_DQ4); else clr_mask |= (1 << GPIO_DQ4);
    if ((data >> 5) & 0x01) set_mask |= (1 << GPIO_DQ5); else clr_mask |= (1 << GPIO_DQ5);
    if ((data >> 6) & 0x01) set_mask |= (1 << GPIO_DQ6); else clr_mask |= (1 << GPIO_DQ6);
    if ((data >> 7) & 0x01) set_mask |= (1 << GPIO_DQ7); else clr_mask |= (1 << GPIO_DQ7);

    gpio_clr_multi(clr_mask);
    gpio_set_multi(set_mask);
}

// Helper to read DQ pins
uint8_t interface::read_dq_pins() const {
    uint8_t data = 0;
    data |= (gpio_read(GPIO_DQ0) << 0);
    data |= (gpio_read(GPIO_DQ1) << 1);
    data |= (gpio_read(GPIO_DQ2) << 2);
    data |= (gpio_read(GPIO_DQ3) << 3);
    data |= (gpio_read(GPIO_DQ4) << 4);
    data |= (gpio_read(GPIO_DQ5) << 5);
    data |= (gpio_read(GPIO_DQ6) << 6);
    data |= (gpio_read(GPIO_DQ7) << 7);
    return data;
} 

void interface::open_interface_debug_file() {
#if DEBUG_INTERFACE
	interface_debug_file.open("interface_debug_log.txt",std::fstream::out);
#endif
}

void interface::close_interface_debug_file(bool verbose) {
#if DEBUG_INTERFACE
	if(interface_debug_file)
	{
		interface_debug_file<<"I: Closing the debug file"<<std::endl;
		interface_debug_file.close();
	}else
		std::cout<<"I: Closing the debug file"<<std::endl;
#else
    if (verbose) std::cout << "I: Closing the debug file" << std::endl;
#endif
}

__attribute__((always_inline)) void interface::set_pin_direction_inactive() const {
    // Set all control pins to output and inactive state
    gpio_set_direction(GPIO_WP, true); gpio_write(GPIO_WP, 1);
    gpio_set_direction(GPIO_CLE, true); gpio_write(GPIO_CLE, 0);
    gpio_set_direction(GPIO_ALE, true); gpio_write(GPIO_ALE, 0);
    gpio_set_direction(GPIO_RE, true); gpio_write(GPIO_RE, 1);
    gpio_set_direction(GPIO_WE, true); gpio_write(GPIO_WE, 1);
    gpio_set_direction(GPIO_CE, true); gpio_write(GPIO_CE, 1);

    // Set RB_PIN to input
    gpio_set_direction(GPIO_RB, false);
    gpio_set_pud(GPIO_RB, BCM2835_GPIO_PUD_UP);

    // Set DQ pins to output and low
    set_datalines_direction_default();

    // Set DQS and DQSc to output and low
    gpio_set_direction(GPIO_DQS, true); gpio_write(GPIO_DQS, 0);
    gpio_set_direction(GPIO_DQSC, true); gpio_write(GPIO_DQSC, 0);
}

__attribute__((always_inline)) void interface::set_ce_low() const {
    gpio_write(GPIO_CE, 0);
}

__attribute__((always_inline)) void interface::set_default_pin_values() const {
    gpio_write(GPIO_CE, 1);
    gpio_write(GPIO_RE, 1);
    gpio_write(GPIO_WE, 1);
    gpio_write(GPIO_ALE, 0);
    gpio_write(GPIO_CLE, 0);

    // Set DQ pins to output and low
    set_datalines_direction_default();

    // Set DQS and DQSc to input
    gpio_set_direction(GPIO_DQS, false);
    gpio_set_direction(GPIO_DQSC, false);
}

__attribute__((always_inline)) void interface::set_datalines_direction_default() const {
    gpio_set_direction(GPIO_DQ0, true);
    gpio_set_direction(GPIO_DQ1, true);
    gpio_set_direction(GPIO_DQ2, true);
    gpio_set_direction(GPIO_DQ3, true);
    gpio_set_direction(GPIO_DQ4, true);
    gpio_set_direction(GPIO_DQ5, true);
    gpio_set_direction(GPIO_DQ6, true);
    gpio_set_direction(GPIO_DQ7, true);

    set_dq_pins(0x00); // Reset DQ pins to low
}

__attribute__((always_inline)) void interface::set_datalines_direction_input() const {
    gpio_set_direction(GPIO_DQ0, false);
    gpio_set_direction(GPIO_DQ1, false);
    gpio_set_direction(GPIO_DQ2, false);
    gpio_set_direction(GPIO_DQ3, false);
    gpio_set_direction(GPIO_DQ4, false);
    gpio_set_direction(GPIO_DQ5, false);
    gpio_set_direction(GPIO_DQ6, false);
    gpio_set_direction(GPIO_DQ7, false);
}

__attribute__((always_inline)) void interface::send_command(uint8_t command_to_send) const {
    gpio_write(GPIO_WE, 0);
    gpio_write(GPIO_CE, 0);
    gpio_write(GPIO_CLE, 1);

    set_dq_pins(command_to_send);

    SAMPLE_TIME;

    gpio_write(GPIO_WE, 1);

    HOLD_TIME;

    gpio_write(GPIO_CLE, 0);
    set_default_pin_values();
}

__attribute__((always_inline)) void interface::send_addresses(uint8_t *address_to_send, uint8_t num_address_bytes,
                                                              bool verbose) const {
#if DEBUG_INTERFACE
	if(interface_debug_file)
		interface_debug_file<<"I: Sending Addresses "<<num_address_bytes<<" bytes"<<std::endl;
	else
		std::cout<<"I: Sending Addresses "<<num_address_bytes<<" bytes"<<std::endl;
#else
    if (verbose) std::cout << "I: Sending Addresses " << (int)num_address_bytes << " bytes" << std::endl << "	:";
#endif

    gpio_write(GPIO_CE, 0);
    gpio_write(GPIO_ALE, 1);

    uint8_t i = 0;
    for (i = 0; i < num_address_bytes; i++) {
        gpio_write(GPIO_WE, 0);
        set_dq_pins(address_to_send[i]);

#if DEBUG_INTERFACE
	if(interface_debug_file)
		fprintf(interface_debug_file,"0x%x,", address_to_send[i]);
	else
		fprintf(stdout,"0x%x,", address_to_send[i]);
#else
        if (verbose) fprintf(stdout, "0x%x,", address_to_send[i]);
#endif
        SAMPLE_TIME;

        gpio_write(GPIO_WE, 1);
        HOLD_TIME;
    }
    set_default_pin_values();

#if DEBUG_INTERFACE
	if(interface_debug_file)
		interface_debug_file<<std::endl;
	else
		std::cout<<std::endl;
#else
    if (verbose) std::cout << std::endl;
#endif
}

__attribute__((always_inline)) void interface::send_data(uint8_t *data_to_send, uint16_t num_data) const {
    if (interface_type == asynchronous) {
        gpio_write(GPIO_CE, 0);
        uint16_t i = 0;
        for (i = 0; i < num_data; i++) {
            gpio_write(GPIO_WE, 0);
            set_dq_pins(data_to_send[i]);
            gpio_write(GPIO_WE, 1);
        }
        set_default_pin_values();
    } else {
        set_datalines_direction_default();

        gpio_write(GPIO_CE, 0);
        gpio_write(GPIO_WE, 1);

        gpio_set_direction(GPIO_DQS, true);
        gpio_set_direction(GPIO_DQSC, true);
        gpio_write(GPIO_DQS, 1);

        gpio_write(GPIO_CLE, 0);
        gpio_write(GPIO_ALE, 0);

        gpio_write(GPIO_DQS, 0);
        busy_wait_ns(10);
        uint16_t i = 0;
        for (i = 0; i < num_data; i++) {
            set_dq_pins(data_to_send[i]);
            gpio_write(GPIO_DQS, !gpio_read(GPIO_DQS)); // Toggle DQS
            busy_wait_ns(10);
        }
        set_default_pin_values();
    }
}

void interface::turn_leds_on() {
    gpio_write(GPIO_RLED0, 1);
    gpio_write(GPIO_RLED1, 1);
    gpio_write(GPIO_RLED2, 1);
    gpio_write(GPIO_RLED3, 1);
}

void interface::turn_leds_off() {
    gpio_write(GPIO_RLED0, 0);
    gpio_write(GPIO_RLED1, 0);
    gpio_write(GPIO_RLED2, 0);
    gpio_write(GPIO_RLED3, 0);
}

void interface::test_leds_increment(bool verbose) {
#if DEBUG_INTERFACE
	if(interface_debug_file)	interface_debug_file<<"I: testing LEDs with a shifting lighting pattern"<<std::endl;
	else
		std::cout<<"I: testing LEDs with a shifting lighting pattern"<<std::endl;
#else
    if (verbose) std::cout << "I: testing LEDs with a shifting lighting pattern" << std::endl;
#endif

    // Assuming RLED0_PIN to RLED3_PIN are consecutive for simplicity, or iterate through an array
    int leds[] = {GPIO_RLED0, GPIO_RLED1, GPIO_RLED2, GPIO_RLED3};
    int num_leds = sizeof(leds) / sizeof(leds[0]);

    for (uint8_t reps = 0; reps < 50; reps++) {
        for (int i = 0; i < num_leds; i++) {
            gpio_write(leds[i], 1); // Turn on current LED
            busy_wait_ns(65530000); // Delay
            gpio_write(leds[i], 0); // Turn off current LED
        }
    }

#if DEBUG_INTERFACE
	if(interface_debug_file)	interface_debug_file<<"I: .. testing LEDs completed"<<std::endl;
	else 	std::cout<<"I: .. testing LEDs completed"<<std::endl;
#else
    if (verbose) std::cout << "I: .. testing LEDs completed" << std::endl;
#endif
}
