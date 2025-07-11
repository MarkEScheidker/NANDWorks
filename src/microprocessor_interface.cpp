#include "microprocessor_interface.h"

#include <cstdint>
#include <iostream>
#include <pigpio.h>

using namespace std;

// Helper to set DQ pins
void interface::set_dq_pins(uint8_t data) {
    gpioWrite(GPIO_DQ0, (data >> 0) & 0x01);
    gpioWrite(GPIO_DQ1, (data >> 1) & 0x01);
    gpioWrite(GPIO_DQ2, (data >> 2) & 0x01);
    gpioWrite(GPIO_DQ3, (data >> 3) & 0x01);
    gpioWrite(GPIO_DQ4, (data >> 4) & 0x01);
    gpioWrite(GPIO_DQ5, (data >> 5) & 0x01);
    gpioWrite(GPIO_DQ6, (data >> 6) & 0x01);
    gpioWrite(GPIO_DQ7, (data >> 7) & 0x01);
}

// Helper to read DQ pins
uint8_t interface::read_dq_pins() {
    uint8_t data = 0;
    data |= (gpioRead(GPIO_DQ0) << 0);
    data |= (gpioRead(GPIO_DQ1) << 1);
    data |= (gpioRead(GPIO_DQ2) << 2);
    data |= (gpioRead(GPIO_DQ3) << 3);
    data |= (gpioRead(GPIO_DQ4) << 4);
    data |= (gpioRead(GPIO_DQ5) << 5);
    data |= (gpioRead(GPIO_DQ6) << 6);
    data |= (gpioRead(GPIO_DQ7) << 7);
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
		interface_debug_file<<"I: Closing the debug file"<<endl;
		interface_debug_file.close();
	}else
		cout<<"I: Closing the debug file"<<endl;
#else
    if (verbose) cout << "I: Closing the debug file" << endl;
#endif
}

__attribute__((always_inline)) void interface::set_pin_direction_inactive() {
    // Set all control pins to output and inactive state
    gpioSetMode(GPIO_WP, PI_OUTPUT);
    gpioWrite(GPIO_WP, 1); // Active low
    gpioSetMode(GPIO_CLE, PI_OUTPUT);
    gpioWrite(GPIO_CLE, 0); // Active high
    gpioSetMode(GPIO_ALE, PI_OUTPUT);
    gpioWrite(GPIO_ALE, 0); // Active high
    gpioSetMode(GPIO_RE, PI_OUTPUT);
    gpioWrite(GPIO_RE, 1); // Active low
    gpioSetMode(GPIO_WE, PI_OUTPUT);
    gpioWrite(GPIO_WE, 1); // Active low
    gpioSetMode(GPIO_CE, PI_OUTPUT);
    gpioWrite(GPIO_CE, 1); // Active low

    // Set RB_PIN to input
    gpioSetMode(GPIO_RB, PI_INPUT);
    gpioSetPullUpDown(GPIO_RB, PI_PUD_UP);

    // Set DQ pins to output and low
    gpioSetMode(GPIO_DQ0, PI_OUTPUT);
    gpioWrite(GPIO_DQ0, 0);
    gpioSetMode(GPIO_DQ1, PI_OUTPUT);
    gpioWrite(GPIO_DQ1, 0);
    gpioSetMode(GPIO_DQ2, PI_OUTPUT);
    gpioWrite(GPIO_DQ2, 0);
    gpioSetMode(GPIO_DQ3, PI_OUTPUT);
    gpioWrite(GPIO_DQ3, 0);
    gpioSetMode(GPIO_DQ4, PI_OUTPUT);
    gpioWrite(GPIO_DQ4, 0);
    gpioSetMode(GPIO_DQ5, PI_OUTPUT);
    gpioWrite(GPIO_DQ5, 0);
    gpioSetMode(GPIO_DQ6, PI_OUTPUT);
    gpioWrite(GPIO_DQ6, 0);
    gpioSetMode(GPIO_DQ7, PI_OUTPUT);
    gpioWrite(GPIO_DQ7, 0);

    // Set DQS and DQSc to output and low
    gpioSetMode(GPIO_DQS, PI_OUTPUT);
    gpioWrite(GPIO_DQS, 0);
    gpioSetMode(GPIO_DQSC, PI_OUTPUT);
    gpioWrite(GPIO_DQSC, 0);
}

__attribute__((always_inline)) void interface::set_ce_low() {
    gpioWrite(GPIO_CE, 0);
}

__attribute__((always_inline)) void interface::set_default_pin_values() {
    gpioWrite(GPIO_CE, 1);
    gpioWrite(GPIO_RE, 1);
    gpioWrite(GPIO_WE, 1);
    gpioWrite(GPIO_ALE, 0);
    gpioWrite(GPIO_CLE, 0);

    // Set DQ pins to output and low
    gpioSetMode(GPIO_DQ0, PI_OUTPUT);
    gpioWrite(GPIO_DQ0, 0);
    gpioSetMode(GPIO_DQ1, PI_OUTPUT);
    gpioWrite(GPIO_DQ1, 0);
    gpioSetMode(GPIO_DQ2, PI_OUTPUT);
    gpioWrite(GPIO_DQ2, 0);
    gpioSetMode(GPIO_DQ3, PI_OUTPUT);
    gpioWrite(GPIO_DQ3, 0);
    gpioSetMode(GPIO_DQ4, PI_OUTPUT);
    gpioWrite(GPIO_DQ4, 0);
    gpioSetMode(GPIO_DQ5, PI_OUTPUT);
    gpioWrite(GPIO_DQ5, 0);
    gpioSetMode(GPIO_DQ6, PI_OUTPUT);
    gpioWrite(GPIO_DQ6, 0);
    gpioSetMode(GPIO_DQ7, PI_OUTPUT);
    gpioWrite(GPIO_DQ7, 0);

    // Set DQS and DQSc to input
    gpioSetMode(GPIO_DQS, PI_INPUT);
    gpioSetMode(GPIO_DQSC, PI_INPUT);
}

__attribute__((always_inline)) void interface::set_datalines_direction_default() {
    gpioSetMode(GPIO_DQ0, PI_OUTPUT);
    gpioSetMode(GPIO_DQ1, PI_OUTPUT);
    gpioSetMode(GPIO_DQ2, PI_OUTPUT);
    gpioSetMode(GPIO_DQ3, PI_OUTPUT);
    gpioSetMode(GPIO_DQ4, PI_OUTPUT);
    gpioSetMode(GPIO_DQ5, PI_OUTPUT);
    gpioSetMode(GPIO_DQ6, PI_OUTPUT);
    gpioSetMode(GPIO_DQ7, PI_OUTPUT);

    set_dq_pins(0x00); // Reset DQ pins to low
}

__attribute__((always_inline)) void interface::set_datalines_direction_input() {
    gpioSetMode(GPIO_DQ0, PI_INPUT);
    gpioSetMode(GPIO_DQ1, PI_INPUT);
    gpioSetMode(GPIO_DQ2, PI_INPUT);
    gpioSetMode(GPIO_DQ3, PI_INPUT);
    gpioSetMode(GPIO_DQ4, PI_INPUT);
    gpioSetMode(GPIO_DQ5, PI_INPUT);
    gpioSetMode(GPIO_DQ6, PI_INPUT);
    gpioSetMode(GPIO_DQ7, PI_INPUT);
}

__attribute__((always_inline)) void interface::send_command(uint8_t command_to_send) {
    gpioWrite(GPIO_WE, 0);
    gpioWrite(GPIO_CE, 0);
    gpioWrite(GPIO_CLE, 1);

    set_dq_pins(command_to_send);

    SAMPLE_TIME;

    gpioWrite(GPIO_WE, 1);

    HOLD_TIME;

    gpioWrite(GPIO_CLE, 0);
    set_default_pin_values();
}

__attribute__((always_inline)) void interface::send_addresses(uint8_t *address_to_send, uint8_t num_address_bytes,
                                                              bool verbose) {
#if DEBUG_INTERFACE
	if(interface_debug_file)
		interface_debug_file<<"I: Sending Addresses "<<num_address_bytes<<" bytes"<<endl;
	else
		cout<<"I: Sending Addresses "<<num_address_bytes<<" bytes"<<endl;
#else
    if (verbose) cout << "I: Sending Addresses " << num_address_bytes << " bytes" << endl << "\t:";
#endif

    gpioWrite(GPIO_CE, 0);
    gpioWrite(GPIO_ALE, 1);

    uint8_t i = 0;
    for (i = 0; i < num_address_bytes; i++) {
        gpioWrite(GPIO_WE, 0);
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

        gpioWrite(GPIO_WE, 1);
        HOLD_TIME;
    }
    set_default_pin_values();

#if DEBUG_INTERFACE
	if(interface_debug_file)
		interface_debug_file<<endl;
	else
		cout<<endl;
#else
    if (verbose) cout << endl;
#endif
}

__attribute__((always_inline)) void interface::send_data(uint8_t *data_to_send, uint16_t num_data) {
    if (interface_type == asynchronous) {
        gpioWrite(GPIO_CE, 0);
        uint16_t i = 0;
        for (i = 0; i < num_data; i++) {
            gpioWrite(GPIO_WE, 0);
            set_dq_pins(data_to_send[i]);
            SAMPLE_TIME;

            gpioWrite(GPIO_WE, 1);
            HOLD_TIME;
            set_dq_pins(0x00);
        }
        set_default_pin_values();
    } else {
        set_datalines_direction_default();

        gpioWrite(GPIO_CE, 0);
        gpioWrite(GPIO_WE, 1);

        gpioSetMode(GPIO_DQS, PI_OUTPUT);
        gpioSetMode(GPIO_DQSC, PI_OUTPUT);
        gpioWrite(GPIO_DQS, 1);

        gpioWrite(GPIO_CLE, 0);
        gpioWrite(GPIO_ALE, 0);

        gpioWrite(GPIO_DQS, 0);
        gpioDelay(1);
        uint16_t i = 0;
        for (i = 0; i < num_data; i++) {
            set_dq_pins(data_to_send[i]);
            gpioWrite(GPIO_DQS, !gpioRead(GPIO_DQS)); // Toggle DQS
            gpioDelay(1);
        }
        set_default_pin_values();
    }
}

void interface::turn_leds_on() {
    gpioWrite(GPIO_RLED0, 1);
    gpioWrite(GPIO_RLED1, 1);
    gpioWrite(GPIO_RLED2, 1);
    gpioWrite(GPIO_RLED3, 1);
}

void interface::turn_leds_off() {
    gpioWrite(GPIO_RLED0, 0);
    gpioWrite(GPIO_RLED1, 0);
    gpioWrite(GPIO_RLED2, 0);
    gpioWrite(GPIO_RLED3, 0);
}

void interface::test_leds_increment(bool verbose) {
#if DEBUG_INTERFACE
	if(interface_debug_file)	interface_debug_file<<"I: testing LEDs with a shifting lighting pattern"<<endl;
	else
		cout<<"I: testing LEDs with a shifting lighting pattern"<<endl;
#else
    if (verbose) cout << "I: testing LEDs with a shifting lighting pattern" << endl;
#endif

    // Assuming RLED0_PIN to RLED3_PIN are consecutive for simplicity, or iterate through an array
    int leds[] = {GPIO_RLED0, GPIO_RLED1, GPIO_RLED2, GPIO_RLED3};
    int num_leds = sizeof(leds) / sizeof(leds[0]);

    for (uint8_t reps = 0; reps < 50; reps++) {
        for (int i = 0; i < num_leds; i++) {
            gpioWrite(leds[i], 1); // Turn on current LED
            gpioDelay(65530); // Delay
            gpioWrite(leds[i], 0); // Turn off current LED
        }
    }

#if DEBUG_INTERFACE
	if(interface_debug_file)	interface_debug_file<<"I: .. testing LEDs completed"<<endl;
	else 	cout<<"I: .. testing LEDs completed"<<endl;
#else
    if (verbose) cout << "I: .. testing LEDs completed" << endl;
#endif
}
