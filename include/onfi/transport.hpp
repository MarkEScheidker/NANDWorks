#ifndef ONFI_TRANSPORT_HPP
#define ONFI_TRANSPORT_HPP

#include <stdint.h>

namespace onfi {

class Transport {
public:
    virtual ~Transport() = default;

    virtual void send_command(uint8_t command) const = 0;
    virtual void send_addresses(const uint8_t* address, uint8_t count, bool verbose = false) const = 0;
    virtual void send_data(const uint8_t* data, uint16_t count) const = 0;
    virtual void wait_ready_blocking() const = 0;
    virtual void delay_function(uint32_t loop_count) = 0;
    virtual void get_data(uint8_t* dst, uint16_t count) const = 0;
    virtual uint8_t get_status() = 0;
};

} // namespace onfi

#endif // ONFI_TRANSPORT_HPP
