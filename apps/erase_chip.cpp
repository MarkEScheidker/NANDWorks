#include "onfi_interface.h"
#include <iostream>

int main() {
    onfi_interface onfi;
    onfi.get_started();
    std::cout << "Erasing entire chip (use with caution) ..." << std::endl;
    for (unsigned int b = 0; b < onfi.num_blocks; ++b) {
        onfi.erase_block(b, /*verbose*/false);
    }
    std::cout << "Done." << std::endl;
    return 0;
}
