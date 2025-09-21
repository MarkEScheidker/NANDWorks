#include "onfi_interface.hpp"
#include <vector>

int main() {
    onfi_interface onfi;
    onfi.get_started();
    std::vector<uint8_t> data(4096, 0xff);
    onfi.program_page(0, 0, data.data(), true, true);
    return 0;
}
