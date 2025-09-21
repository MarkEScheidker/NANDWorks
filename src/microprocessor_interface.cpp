#include "microprocessor_interface.hpp"
#include "gpio.hpp"
#include "timing.hpp"
#include "hardware_locations.hpp"
#include <bcm2835.h>
#include <cstdint>
#include <iostream>
#include "logging.hpp"

namespace {
    static uint32_t dq_all_mask = 0;
    static uint32_t dq_set_mask[256];
    static uint32_t dq_clear_mask[256];
    static bool dq_lut_inited = false;

    constexpr uint8_t kDqPins[] = {
        GPIO_DQ0, GPIO_DQ1, GPIO_DQ2, GPIO_DQ3,
        GPIO_DQ4, GPIO_DQ5, GPIO_DQ6, GPIO_DQ7,
    };

    constexpr uint8_t kLedPins[] = {
        GPIO_RLED0, GPIO_RLED1, GPIO_RLED2, GPIO_RLED3,
    };

    inline uint32_t bit(uint8_t pin) { return static_cast<uint32_t>(1u) << pin; }

    void init_dq_lut() {
        if (dq_lut_inited) return;
        dq_all_mask = 0;
        for (uint8_t pin : kDqPins) {
            dq_all_mask |= bit(pin);
        }
        for (int v = 0; v < 256; ++v) {
            uint32_t mask = 0;
            if (v & 0x01) mask |= bit(GPIO_DQ0);
            if (v & 0x02) mask |= bit(GPIO_DQ1);
            if (v & 0x04) mask |= bit(GPIO_DQ2);
            if (v & 0x08) mask |= bit(GPIO_DQ3);
            if (v & 0x10) mask |= bit(GPIO_DQ4);
            if (v & 0x20) mask |= bit(GPIO_DQ5);
            if (v & 0x40) mask |= bit(GPIO_DQ6);
            if (v & 0x80) mask |= bit(GPIO_DQ7);
            dq_set_mask[v] = mask;
            dq_clear_mask[v] = dq_all_mask & ~mask;
        }
        dq_lut_inited = true;
    }
}

// Helper to set DQ pins using LUT for speed
void interface::set_dq_pins(uint8_t data) const {
    if (!dq_lut_inited) init_dq_lut();
    gpio_clr_multi(dq_clear_mask[data]);
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

void interface::open_interface_debug_file() {}

void interface::close_interface_debug_file(bool verbose) {
    if (verbose) LOG_HAL_INFO("Closing interface debug");
}

__attribute__((always_inline)) void interface::set_pin_direction_inactive() const {
    // Set all control pins to output and inactive state
    gpio_set_direction(GPIO_WP, true); gpio_write(GPIO_WP, 1);
    gpio_set_direction(GPIO_CLE, true); gpio_write(GPIO_CLE, 0);
    gpio_set_direction(GPIO_ALE, true); gpio_write(GPIO_ALE, 0);
    gpio_set_direction(GPIO_RE, true); gpio_write(GPIO_RE, 1);
    gpio_set_direction(GPIO_WE, true); gpio_write(GPIO_WE, 1);
    gpio_set_direction(GPIO_CE, true); gpio_write(GPIO_CE, 1);

    // Set RB_PIN to input with pull-up
    gpio_set_direction(GPIO_RB, false);
    gpio_set_pud(GPIO_RB, BCM2835_GPIO_PUD_UP);

    // Data bus defaults to outputs pulled low
    set_datalines_direction_default();

    // Set DQS and DQSc to output and low by default
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

    set_datalines_direction_default();

    // Set DQS and DQSc to input when idle
    gpio_set_direction(GPIO_DQS, false);
    gpio_set_direction(GPIO_DQSC, false);
}

__attribute__((always_inline)) void interface::set_datalines_direction_default() const {
    for (uint8_t pin : kDqPins) {
        gpio_set_direction(pin, true);
    }
    set_dq_pins(0x00); // Reset DQ pins to low
}

__attribute__((always_inline)) void interface::set_datalines_direction_input() const {
    for (uint8_t pin : kDqPins) {
        gpio_set_direction(pin, false);
    }
}

__attribute__((always_inline)) void interface::restore_control_pins(bool release_data_bus) const {
    gpio_write(GPIO_CE, 1);
    gpio_write(GPIO_WE, 1);
    gpio_write(GPIO_RE, 1);
    gpio_write(GPIO_ALE, 0);
    gpio_write(GPIO_CLE, 0);

    if (release_data_bus) {
        set_datalines_direction_default();
        gpio_set_direction(GPIO_DQS, false);
        gpio_set_direction(GPIO_DQSC, false);
    }
}

__attribute__((always_inline)) void interface::send_command(uint8_t command_to_send) const {
    gpio_write(GPIO_CE, 0);
    gpio_write(GPIO_CLE, 1);

    bcm2835_gpio_clr(GPIO_WE);
    set_dq_pins(command_to_send);
    bcm2835_gpio_set(GPIO_WE);

    gpio_write(GPIO_CLE, 0);
    restore_control_pins(false);
}

__attribute__((always_inline)) void interface::send_addresses(const uint8_t *address_to_send, uint8_t num_address_bytes,
                                                              bool verbose) const {
    LOG_HAL_DEBUG_IF(verbose, "Sending %u address bytes", static_cast<unsigned>(num_address_bytes));

    gpio_write(GPIO_CE, 0);
    gpio_write(GPIO_ALE, 1);

    for (uint8_t i = 0; i < num_address_bytes; ++i) {
        bcm2835_gpio_clr(GPIO_WE);
        set_dq_pins(address_to_send[i]);
        bcm2835_gpio_set(GPIO_WE);
    }

    restore_control_pins(false);

    (void)verbose;
}

__attribute__((always_inline)) void interface::send_data(const uint8_t *data_to_send, uint16_t num_data) const {
    if (interface_type == asynchronous) {
        gpio_write(GPIO_CE, 0);
        for (uint16_t i = 0; i < num_data; ++i) {
            bcm2835_gpio_clr(GPIO_WE);
            set_dq_pins(data_to_send[i]);
            bcm2835_gpio_set(GPIO_WE);
        }
        restore_control_pins(false);
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
        for (uint16_t i = 0; i < num_data; ++i) {
            set_dq_pins(data_to_send[i]);
            gpio_write(GPIO_DQS, !gpio_read(GPIO_DQS)); // Toggle DQS
            busy_wait_ns(10);
        }
        restore_control_pins(true);
    }
}

void interface::turn_leds_on() {
    for (uint8_t pin : kLedPins) {
        gpio_write(pin, 1);
    }
}

void interface::turn_leds_off() {
    for (uint8_t pin : kLedPins) {
        gpio_write(pin, 0);
    }
}

void interface::test_leds_increment(bool verbose) {
    LOG_HAL_INFO_IF(verbose, "Testing LEDs with a shifting lighting pattern");

    for (uint8_t reps = 0; reps < 50; ++reps) {
        for (uint8_t pin : kLedPins) {
            gpio_write(pin, 1);
            busy_wait_ns(65530000);
            gpio_write(pin, 0);
        }
    }

    LOG_HAL_INFO_IF(verbose, ".. Testing LEDs completed");
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
