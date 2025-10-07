#ifndef NANDWORKS_DRIVER_CONTEXT_HPP
#define NANDWORKS_DRIVER_CONTEXT_HPP

#include "onfi_interface.hpp"

#include <memory>

namespace nandworks {

class DriverContext {
public:
    explicit DriverContext(bool verbose);
    ~DriverContext();

    DriverContext(const DriverContext&) = delete;
    DriverContext& operator=(const DriverContext&) = delete;

    DriverContext(DriverContext&&) = delete;
    DriverContext& operator=(DriverContext&&) = delete;

    bool verbose() const noexcept { return verbose_; }
    void set_verbose(bool verbose) noexcept { verbose_ = verbose; }

    onfi_interface& require_onfi();
    onfi_interface& require_onfi_started(param_type type = param_type::ONFI);

    bool has_onfi() const noexcept { return static_cast<bool>(onfi_); }
    bool onfi_started() const noexcept { return started_; }

    void shutdown() noexcept;

private:
    bool verbose_;
    std::unique_ptr<onfi_interface> onfi_;
    bool started_ = false;
    param_type start_type_ = param_type::ONFI;
};

} // namespace nandworks

#endif // NANDWORKS_DRIVER_CONTEXT_HPP
