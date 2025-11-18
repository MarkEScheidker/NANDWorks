#include "onfi_interface.hpp"
#include "onfi/controller.hpp"
#include "onfi/device.hpp"
#include "onfi/device_utils.hpp"
#include <vector>

int main() {
    onfi_interface onfi;
    onfi.get_started();

    onfi::OnfiController ctrl(onfi);
    onfi::NandDevice dev(ctrl);
    onfi::populate_device(onfi, dev);

    std::vector<uint8_t> buffer;
    dev.read_page(0, 0, /*including_spare*/false, /*bytewise*/false, buffer);
    return 0;
}
