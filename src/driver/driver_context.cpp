#include "nandworks/driver_context.hpp"

#include <utility>

namespace nandworks {

DriverContext::DriverContext(bool verbose)
    : verbose_(verbose) {}

DriverContext::~DriverContext() {
    shutdown();
}

onfi_interface& DriverContext::require_onfi() {
    if (!onfi_) {
        onfi_ = std::make_unique<onfi_interface>();
    }
    return *onfi_;
}

onfi_interface& DriverContext::require_onfi_started(param_type type) {
    onfi_interface& controller = require_onfi();
    if (!started_) {
        try {
            controller.get_started(type);
            started_ = true;
            start_type_ = type;
        } catch (...) {
            try {
                controller.deinitialize_onfi(verbose_);
            } catch (...) {
                // Suppress cleanup exceptions during failure recovery.
            }
            onfi_.reset();
            started_ = false;
            throw;
        }
    } else if (type != start_type_) {
        controller.read_parameters(type, /*bytewise*/true, verbose_);
        start_type_ = type;
    }
    return controller;
}

void DriverContext::shutdown() noexcept {
    if (onfi_) {
        if (started_) {
            try {
                onfi_->deinitialize_onfi(verbose_);
            } catch (...) {
                // Suppress exceptions during shutdown.
            }
            started_ = false;
        }
        onfi_.reset();
    }
}

} // namespace nandworks
