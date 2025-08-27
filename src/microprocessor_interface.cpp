#include "microprocessor_interface.h"
#include "gpio.h"
#include "timing.h"
#include "hardware_locations.h"
#include <bcm2835.h>
#include <cstdint>
#include <iostream>

namespace {
    static uint32_t dq_all_mask = 0;
    static uint32_t dq_set_mask[256];
    static bool dq_lut_inited = false;

    inline uint32_t bit(uint8_t pin) { return (uint32_t)1u << pin; }

    void init_dq_lut() {
        if (dq_lut_inited) return;
        dq_all_mask = bit(GPIO_DQ0) | bit(GPIO_DQ1) | bit(GPIO_DQ2) | bit(GPIO_DQ3) |
                      bit(GPIO_DQ4) | bit(GPIO_DQ5) | bit(GPIO_DQ6) | bit(GPIO_DQ7);
        for (int v = 0; v < 256; ++v) {
            uint32_t m = 0;
            if (v & 0x01) m |= bit(GPIO_DQ0);
            if (v & 0x02) m |= bit(GPIO_DQ1);
            if (v & 0x04) m |= bit(GPIO_DQ2);
            if (v & 0x08) m |= bit(GPIO_DQ3);
            if (v & 0x10) m |= bit(GPIO_DQ4);
            if (v & 0x20) m |= bit(GPIO_DQ5);
            if (v & 0x40) m |= bit(GPIO_DQ6);
            if (v & 0x80) m |= bit(GPIO_DQ7);
            dq_set_mask[v] = m;
        }
        dq_lut_inited = true;
    }
}

// Helper to set DQ pins using LUT for speed
void interface::set_dq_pins(uint8_t data) const {
    if (!dq_lut_inited) init_dq_lut();
    gpio_clr_multi(dq_all_mask);
    gpio_set_multi(dq_set_mask[data]);
}

// Helper to read DQ pins (single GPLEV0 read for speed)
uint8_t interface::read_dq_pins() const {
    const uint32_t levels = gpio_read_levels0();
    uint8_t data = 0;
    data |= ((levels >> GPIO_DQ0) & 0x1) << 0;
    data |= ((levels >> GPIO_DQ1) & 0x1) << 1;
    data |= ((levels >> GPIO_DQ2) & 0x1) << 2;
    data |= ((levels >> GPIO_DQ3) & 0x1) << 3;
    data |= ((levels >> GPIO_DQ4) & 0x1) << 4;
    data |= ((levels >> GPIO_DQ5) & 0x1) << 5;
    data |= ((levels >> GPIO_DQ6) & 0x1) << 6;
    data |= ((levels >> GPIO_DQ7) & 0x1) << 7;
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
    gpio_set_low(GPIO_WE);
    gpio_write(GPIO_CE, 0);
    gpio_write(GPIO_CLE, 1);

    set_dq_pins(command_to_send);

    gpio_set_high(GPIO_WE);

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
        gpio_set_low(GPIO_WE);
        set_dq_pins(address_to_send[i]);

#if DEBUG_INTERFACE
	if(interface_debug_file)
		fprintf(interface_debug_file,"0x%x,", address_to_send[i]);
	else
		fprintf(stdout,"0x%x,", address_to_send[i]);
#else
        if (verbose) fprintf(stdout, "0x%x,", address_to_send[i]);
#endif
        gpio_set_high(GPIO_WE);
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
            gpio_set_low(GPIO_WE);
            set_dq_pins(data_to_send[i]);
            gpio_set_high(GPIO_WE);
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

bool interface::wait_ready(uint64_t timeout_ns) const {
    const uint64_t start = get_timestamp_ns();
    while (gpio_read(GPIO_RB) == 0) {
        if ((get_timestamp_ns() - start) > timeout_ns) return false;
    }
    return true;
}

void interface::wait_ready_blocking() const {
    while (gpio_read(GPIO_RB) == 0) {
        // tight spin, minimal overhead
    }
}
