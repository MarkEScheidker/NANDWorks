#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <bcm2835.h>

struct PinInfo {
    std::string name;
    int pin;
};

static void wait_for_enter() {
    std::cout << "Press Enter to continue...";
    std::cout.flush();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int main() {
    std::cout << "GPIO test program is running." << std::endl;
    wait_for_enter();

    if (!bcm2835_init()) {
        std::cerr << "Failed to initialise bcm2835 library." << std::endl;
        return 1;
    }

    std::vector<PinInfo> pins = {
        {"io0", 21}, {"io1", 20}, {"io2", 16}, {"io3", 12},
        {"io4", 25}, {"io5", 24}, {"io6", 23}, {"io7", 18},
        {"WP", 26},
        {"WE", 19},
        {"ALE", 13},
        {"CLE", 11},
        {"CE", 22},
        {"RE", 27},
        {"RB", 17}
    };

    for (const auto &pin_info: pins) {
        std::cout << std::endl;
        std::cout << "Testing Pin: " << pin_info.name << " (GPIO " << pin_info.pin << ")" << std::endl;

        bcm2835_gpio_fsel(pin_info.pin, BCM2835_GPIO_FSEL_OUTP);

        std::cout << "Pin is OFF. ";
        wait_for_enter();

        std::cout << "Turning pin ON." << std::endl;
        bcm2835_gpio_write(pin_info.pin, HIGH);

        std::cout << "Pin is ON. ";
        wait_for_enter();

        std::cout << "Turning pin OFF." << std::endl;
        bcm2835_gpio_write(pin_info.pin, LOW);
    }

    std::cout << std::endl << "GPIO test complete." << std::endl;

    bcm2835_close();
    return 0;
}
