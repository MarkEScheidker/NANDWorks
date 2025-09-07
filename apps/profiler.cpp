#include "onfi_interface.h"
#include <iostream>

int main() {
    onfi_interface onfi;
    onfi.get_started();
    onfi.open_time_profile_file();
    std::cout << "Profiling: reading parameter page and a few operations..." << std::endl;
    onfi.read_parameters(ONFI, true, true);
    onfi.profile_time();
    std::cout << "Done." << std::endl;
    return 0;
}
