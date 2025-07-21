#include "gpio.h"
#include <bcm2835.h>
#include <sched.h>
#include <iostream>

bool gpio_init() {
    if (!bcm2835_init()) {
        std::cerr << "bcm2835_init failed. Are you running as root?" << std::endl;
        return false;
    }

    struct sched_param sp;
    sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
        std::cerr << "Failed to set real-time scheduler. Are you running as root?" << std::endl;
        return false;
    }

    return true;
}

void gpio_set_direction(uint8_t pin, bool is_output) {
    bcm2835_gpio_fsel(pin, is_output ? BCM2835_GPIO_FSEL_OUTP : BCM2835_GPIO_FSEL_INPT);
}

void gpio_write(uint8_t pin, bool value) {
    bcm2835_gpio_write(pin, value);
}

bool gpio_read(uint8_t pin) {
    return bcm2835_gpio_lev(pin);
}

void gpio_set_pud(uint8_t pin, uint8_t pud) {
    bcm2835_gpio_set_pud(pin, pud);
}

void gpio_set_high(uint8_t pin) {
    bcm2835_gpio_set(pin);
}

void gpio_set_low(uint8_t pin) {
    bcm2835_gpio_clr(pin);
}
