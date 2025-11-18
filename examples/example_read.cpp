#include "onfi_interface.hpp"
#include "onfi/controller.hpp"
#include "onfi/device.hpp"
#include "onfi/device_config.hpp"
#include <vector>

int main() {
    onfi_interface onfi;
    onfi.get_started();

    onfi::OnfiController ctrl(onfi);
    onfi::NandDevice dev(ctrl);
    const auto config = onfi::make_device_config(onfi);
    onfi::apply_device_config(config, dev);

    std::vector<uint8_t> buffer;
    dev.read_page(0, 0, /*including_spare*/false, /*bytewise*/false, buffer);
    return 0;
}
