#include "microprocessor_interface.h"

#include <cstdint>
#include <iostream>
#include <pigpio.h>

using namespace std;

// Helper to set DQ pins
void interface::set_dq_pins(uint8_t data) {
    gpioWrite(DQ0_PIN, (data >> 0) & 0x01);
    gpioWrite(DQ1_PIN, (data >> 1) & 0x01);
    gpioWrite(DQ2_PIN, (data >> 2) & 0x01);
    gpioWrite(DQ3_PIN, (data >> 3) & 0x01);
    gpioWrite(DQ4_PIN, (data >> 4) & 0x01);
    gpioWrite(DQ5_PIN, (data >> 5) & 0x01);
    gpioWrite(DQ6_PIN, (data >> 6) & 0x01);
    gpioWrite(DQ7_PIN, (data >> 7) & 0x01);
}

// Helper to read DQ pins
uint8_t interface::read_dq_pins() {
    uint8_t data = 0;
    data |= (gpioRead(DQ0_PIN) << 0);
    data |= (gpioRead(DQ1_PIN) << 1);
    data |= (gpioRead(DQ2_PIN) << 2);
    data |= (gpioRead(DQ3_PIN) << 3);
    data |= (gpioRead(DQ4_PIN) << 4);
    data |= (gpioRead(DQ5_PIN) << 5);
    data |= (gpioRead(DQ6_PIN) << 6);
    data |= (gpioRead(DQ7_PIN) << 7);
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
    gpioSetMode(WP_PIN, PI_OUTPUT);
    gpioWrite(WP_PIN, 1); // Active low
    gpioSetMode(CLE_PIN, PI_OUTPUT);
    gpioWrite(CLE_PIN, 0); // Active high
    gpioSetMode(ALE_PIN, PI_OUTPUT);
    gpioWrite(ALE_PIN, 0); // Active high
    gpioSetMode(RE_PIN, PI_OUTPUT);
    gpioWrite(RE_PIN, 1); // Active low
    gpioSetMode(WE_PIN, PI_OUTPUT);
    gpioWrite(WE_PIN, 1); // Active low
    gpioSetMode(CE_PIN, PI_OUTPUT);
    gpioWrite(CE_PIN, 1); // Active low

    // Set RB_PIN to input
    gpioSetMode(RB_PIN, PI_INPUT);

    // Set DQ pins to output and low
    gpioSetMode(DQ0_PIN, PI_OUTPUT);
    gpioWrite(DQ0_PIN, 0);
    gpioSetMode(DQ1_PIN, PI_OUTPUT);
    gpioWrite(DQ1_PIN, 0);
    gpioSetMode(DQ2_PIN, PI_OUTPUT);
    gpioWrite(DQ2_PIN, 0);
    gpioSetMode(DQ3_PIN, PI_OUTPUT);
    gpioWrite(DQ3_PIN, 0);
    gpioSetMode(DQ4_PIN, PI_OUTPUT);
    gpioWrite(DQ4_PIN, 0);
    gpioSetMode(DQ5_PIN, PI_OUTPUT);
    gpioWrite(DQ5_PIN, 0);
    gpioSetMode(DQ6_PIN, PI_OUTPUT);
    gpioWrite(DQ6_PIN, 0);
    gpioSetMode(DQ7_PIN, PI_OUTPUT);
    gpioWrite(DQ7_PIN, 0);

    // Set DQS and DQSc to output and low
    gpioSetMode(DQS_PIN, PI_OUTPUT);
    gpioWrite(DQS_PIN, 0);
    gpioSetMode(DQSc_PIN, PI_OUTPUT);
    gpioWrite(DQSc_PIN, 0);
}

__attribute__((always_inline)) void interface::set_ce_low() {
    gpioWrite(CE_PIN, 0);
}

__attribute__((always_inline)) void interface::set_default_pin_values() {
    gpioWrite(CE_PIN, 1);
    gpioWrite(RE_PIN, 1);
    gpioWrite(WE_PIN, 1);
    gpioWrite(ALE_PIN, 0);
    gpioWrite(CLE_PIN, 0);

    // Set DQ pins to output and low
    gpioSetMode(DQ0_PIN, PI_OUTPUT);
    gpioWrite(DQ0_PIN, 0);
    gpioSetMode(DQ1_PIN, PI_OUTPUT);
    gpioWrite(DQ1_PIN, 0);
    gpioSetMode(DQ2_PIN, PI_OUTPUT);
    gpioWrite(DQ2_PIN, 0);
    gpioSetMode(DQ3_PIN, PI_OUTPUT);
    gpioWrite(DQ3_PIN, 0);
    gpioSetMode(DQ4_PIN, PI_OUTPUT);
    gpioWrite(DQ4_PIN, 0);
    gpioSetMode(DQ5_PIN, PI_OUTPUT);
    gpioWrite(DQ5_PIN, 0);
    gpioSetMode(DQ6_PIN, PI_OUTPUT);
    gpioWrite(DQ6_PIN, 0);
    gpioSetMode(DQ7_PIN, PI_OUTPUT);
    gpioWrite(DQ7_PIN, 0);

    // Set DQS and DQSc to input
    gpioSetMode(DQS_PIN, PI_INPUT);
    gpioSetMode(DQSc_PIN, PI_INPUT);
}

__attribute__((always_inline)) void interface::set_datalines_direction_default() {
    gpioSetMode(DQ0_PIN, PI_OUTPUT);
    gpioSetMode(DQ1_PIN, PI_OUTPUT);
    gpioSetMode(DQ2_PIN, PI_OUTPUT);
    gpioSetMode(DQ3_PIN, PI_OUTPUT);
    gpioSetMode(DQ4_PIN, PI_OUTPUT);
    gpioSetMode(DQ5_PIN, PI_OUTPUT);
    gpioSetMode(DQ6_PIN, PI_OUTPUT);
    gpioSetMode(DQ7_PIN, PI_OUTPUT);

    set_dq_pins(0x00); // Reset DQ pins to low
}

__attribute__((always_inline)) void interface::set_datalines_direction_input() {
    gpioSetMode(DQ0_PIN, PI_INPUT);
    gpioSetMode(DQ1_PIN, PI_INPUT);
    gpioSetMode(DQ2_PIN, PI_INPUT);
    gpioSetMode(DQ3_PIN, PI_INPUT);
    gpioSetMode(DQ4_PIN, PI_INPUT);
    gpioSetMode(DQ5_PIN, PI_INPUT);
    gpioSetMode(DQ6_PIN, PI_INPUT);
    gpioSetMode(DQ7_PIN, PI_INPUT);
}

__attribute__((always_inline)) void interface::send_command(uint8_t command_to_send) {
    gpioWrite(WE_PIN, 0);
    gpioWrite(CE_PIN, 0);
    gpioWrite(CLE_PIN, 1);

    set_dq_pins(command_to_send);

    SAMPLE_TIME;

    gpioWrite(WE_PIN, 1);

    HOLD_TIME;

    gpioWrite(CLE_PIN, 0);
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

    gpioWrite(CE_PIN, 0);
    gpioWrite(ALE_PIN, 1);

    uint8_t i = 0;
    for (i = 0; i < num_address_bytes; i++) {
        gpioWrite(WE_PIN, 0);
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

        gpioWrite(WE_PIN, 1);
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
        gpioWrite(CE_PIN, 0);
        uint16_t i = 0;
        for (i = 0; i < num_data; i++) {
            gpioWrite(WE_PIN, 0);
            set_dq_pins(data_to_send[i]);
            SAMPLE_TIME;

            gpioWrite(WE_PIN, 1);
            HOLD_TIME;
            set_dq_pins(0x00);
        }
        set_default_pin_values();
    } else {
        set_datalines_direction_default();

        gpioWrite(CE_PIN, 0);
        gpioWrite(WE_PIN, 1);

        gpioSetMode(DQS_PIN, PI_OUTPUT);
        gpioSetMode(DQSc_PIN, PI_OUTPUT);
        gpioWrite(DQS_PIN, 1);

        gpioWrite(CLE_PIN, 0);
        gpioWrite(ALE_PIN, 0);

        gpioWrite(DQS_PIN, 0);
        gpioDelay(1);
        uint16_t i = 0;
        for (i = 0; i < num_data; i++) {
            set_dq_pins(data_to_send[i]);
            gpioWrite(DQS_PIN, !gpioRead(DQS_PIN)); // Toggle DQS
            gpioDelay(1);
        }
        set_default_pin_values();
    }
}

void interface::turn_leds_on() {
    gpioWrite(RLED0_PIN, 1);
    gpioWrite(RLED1_PIN, 1);
    gpioWrite(RLED2_PIN, 1);
    gpioWrite(RLED3_PIN, 1);
}

void interface::turn_leds_off() {
    gpioWrite(RLED0_PIN, 0);
    gpioWrite(RLED1_PIN, 0);
    gpioWrite(RLED2_PIN, 0);
    gpioWrite(RLED3_PIN, 0);
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
    int leds[] = {RLED0_PIN, RLED1_PIN, RLED2_PIN, RLED3_PIN};
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
