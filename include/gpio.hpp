#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

// Initialize the GPIO, including setting the real-time scheduler
bool gpio_init();

// Set the direction of a GPIO pin
void gpio_set_direction(uint8_t pin, bool is_output);

// Write a value to a GPIO pin
void gpio_write(uint8_t pin, bool value);

// Read a value from a GPIO pin
bool gpio_read(uint8_t pin);

// Set the pull-up/down state of a GPIO pin
void gpio_set_pud(uint8_t pin, uint8_t pud);

// Directly set a GPIO pin high
void gpio_set_high(uint8_t pin);

// Directly set a GPIO pin low
void gpio_set_low(uint8_t pin);

// Set multiple GPIO pins high using a mask
void gpio_set_multi(uint32_t mask);

// Set multiple GPIO pins low using a mask
void gpio_clr_multi(uint32_t mask);

// Read levels for GPIO 0..31 in a single register access (GPLEV0)
// Useful for fast sampling of multiple input pins.
uint32_t gpio_read_levels0();

// Restore scheduler state and release bcm2835 resources when finished.
void gpio_shutdown();

#endif // GPIO_H
