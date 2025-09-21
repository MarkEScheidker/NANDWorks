#include "gpio.hpp"
#include <bcm2835.h>
#include <sched.h>
#include <sys/mman.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
    unsigned int g_init_refcount = 0;
    bool g_scheduler_elevated = false;
    int g_prev_policy = SCHED_OTHER;
    struct sched_param g_prev_param = {};
    bool g_memory_locked = false;
    bool g_affinity_pinned = false;
    cpu_set_t g_prev_affinity;
    bool g_prev_affinity_valid = false;
    int g_pinned_cpu = 0;
}

bool gpio_init() {
    if (g_init_refcount > 0) {
        ++g_init_refcount;
        return true;
    }

    if (!bcm2835_init()) {
        std::cerr << "bcm2835_init failed. Are you running as root?" << std::endl;
        return false;
    }

    if (!g_memory_locked) {
        if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
            std::cerr << "Warning: mlockall failed (" << std::strerror(errno) << ")" << std::endl;
        } else {
            g_memory_locked = true;
        }
    }

    g_prev_policy = sched_getscheduler(0);
    if (g_prev_policy == -1) {
        std::cerr << "Failed to query current scheduler policy." << std::endl;
        bcm2835_close();
        return false;
    }
    if (sched_getparam(0, &g_prev_param) != 0) {
        std::cerr << "Failed to query current scheduler parameters." << std::endl;
        bcm2835_close();
        return false;
    }

    struct sched_param sp{};
    sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
        std::cerr << "Failed to set real-time scheduler. Are you running as root?" << std::endl;
        bcm2835_close();
        return false;
    }

    if (!g_affinity_pinned) {
        CPU_ZERO(&g_prev_affinity);
        if (sched_getaffinity(0, sizeof(cpu_set_t), &g_prev_affinity) == 0) {
            g_prev_affinity_valid = true;
        } else {
            std::cerr << "Warning: sched_getaffinity failed (" << std::strerror(errno) << ")" << std::endl;
        }

        const char* env_cpu = std::getenv("ONFI_PIN_CPU");
        if (env_cpu) {
            char* endptr = nullptr;
            long parsed = std::strtol(env_cpu, &endptr, 10);
            if (endptr && *endptr == '\0' && parsed >= 0 && parsed < CPU_SETSIZE) {
                g_pinned_cpu = static_cast<int>(parsed);
            } else {
                std::cerr << "Warning: invalid ONFI_PIN_CPU value '" << env_cpu << "', defaulting to CPU 0" << std::endl;
                g_pinned_cpu = 0;
            }
        }

        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(static_cast<unsigned>(g_pinned_cpu), &set);
        if (sched_setaffinity(0, sizeof(cpu_set_t), &set) != 0) {
            std::cerr << "Warning: sched_setaffinity failed (" << std::strerror(errno) << ")" << std::endl;
        } else {
            g_affinity_pinned = true;
        }
    }

    g_scheduler_elevated = true;
    g_init_refcount = 1;
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

void gpio_set_multi(uint32_t mask) {
    bcm2835_gpio_set_multi(mask);
}

void gpio_clr_multi(uint32_t mask) {
    bcm2835_gpio_clr_multi(mask);
}

uint32_t gpio_read_levels0() {
    // Read the level register for GPIO 0..31 in one shot.
    // Use the mapped GPIO base plus the GPLEV0 offset (word offset).
    return bcm2835_peri_read(bcm2835_gpio + (BCM2835_GPLEV0 / 4));
}

void gpio_shutdown() {
    if (g_init_refcount == 0) {
        return;
    }

    if (--g_init_refcount > 0) {
        return;
    }

    if (g_scheduler_elevated && g_prev_policy != -1) {
        if (sched_setscheduler(0, g_prev_policy, &g_prev_param) != 0) {
            std::cerr << "Failed to restore scheduler policy." << std::endl;
        }
    }
    g_scheduler_elevated = false;

    if (g_affinity_pinned && g_prev_affinity_valid) {
        if (sched_setaffinity(0, sizeof(cpu_set_t), &g_prev_affinity) != 0) {
            std::cerr << "Warning: failed to restore CPU affinity (" << std::strerror(errno) << ")" << std::endl;
        }
    }
    g_affinity_pinned = false;
    g_prev_affinity_valid = false;

    bcm2835_close();
    g_prev_policy = SCHED_OTHER;
    g_prev_param = {};
}


GpioSession::GpioSession(bool throw_on_failure)
    : active_(gpio_init())
{
    if (!active_ && throw_on_failure) {
        throw std::runtime_error("gpio_init failed");
    }
}

GpioSession::GpioSession(GpioSession&& other) noexcept
    : active_(other.active_)
{
    other.active_ = false;
}

GpioSession& GpioSession::operator=(GpioSession&& other) noexcept
{
    if (this != &other) {
        if (active_) {
            gpio_shutdown();
        }
        active_ = other.active_;
        other.active_ = false;
    }
    return *this;
}

GpioSession::~GpioSession()
{
    if (active_) {
        gpio_shutdown();
    }
}
