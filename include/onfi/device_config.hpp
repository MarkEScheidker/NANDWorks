#ifndef ONFI_DEVICE_CONFIG_HPP
#define ONFI_DEVICE_CONFIG_HPP

#include "onfi/device.hpp"

class onfi_interface;

namespace onfi {

struct DeviceConfig {
    Geometry geometry{};
    default_interface_type interface_type = asynchronous;
    chip_type chip = default_async;
};

inline void apply_device_config(const DeviceConfig& config, NandDevice& device) {
    device.geometry = config.geometry;
    device.interface_type = config.interface_type;
    device.chip = config.chip;
}

DeviceConfig make_device_config(const ::onfi_interface& source);

} // namespace onfi

#endif // ONFI_DEVICE_CONFIG_HPP
