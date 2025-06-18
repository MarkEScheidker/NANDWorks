#include "onfi_interface.h"
#include <vector>

int main() {
    onfi_interface onfi;
    onfi.get_started();
    std::vector<uint8_t> buffer(4096);
    onfi.read_page_and_return_value(0, 0, 0, buffer.data(), false);
    return 0;
}
