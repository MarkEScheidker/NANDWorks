#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <pigpio.h>

struct PinInfo {
    std::string name;
    int pin;
};

void wait_for_enter() {
    std::cout << "Press Enter to continue...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int main() {
    std::cout << "GPIO test program is running." << std::endl;
    wait_for_enter();

    if (gpioInitialise() < 0) {
        std::cerr << "pigpio initialisation failed." << std::endl;
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

    // Clear potential leftover input
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    for (const auto &pin_info: pins) {
        std::cout << std::endl;
        std::cout << "Testing Pin: " << pin_info.name << " (GPIO " << pin_info.pin << ")" << std::endl;

        gpioSetMode(pin_info.pin, PI_OUTPUT);

        std::cout << "Pin is OFF. ";
        wait_for_enter();

        std::cout << "Turning pin ON." << std::endl;
        gpioWrite(pin_info.pin, 1);

        std::cout << "Pin is ON. ";
        wait_for_enter();

        std::cout << "Turning pin OFF." << std::endl;
        gpioWrite(pin_info.pin, 0);
    }

    std::cout << std::endl << "GPIO test complete." << std::endl;

    gpioTerminate();
    return 0;
}
