#ifndef ONFI_TIMED_COMMANDS_HPP
#define ONFI_TIMED_COMMANDS_HPP

#include "onfi_interface.hpp"
#include <cstdint>

namespace onfi::timed {

struct OperationTiming {
    uint64_t duration_ns = 0;
    uint8_t status = 0;
    bool ready = false;
    bool pass = false;
    bool busy_detected = false;
    bool timed_out = false;

    [[nodiscard]] bool succeeded() const { return busy_detected && ready && pass && !timed_out; }
};

OperationTiming erase_block(onfi_interface &onfi,
                            unsigned int block,
                            bool verbose = false);

OperationTiming program_page(onfi_interface &onfi,
                             unsigned int block,
                             unsigned int page,
                             const uint8_t *data,
                             uint32_t length,
                             bool include_spare,
                             bool verbose = false);

OperationTiming read_page(onfi_interface &onfi,
                          unsigned int block,
                          unsigned int page,
                          uint8_t *destination,
                          uint32_t length,
                          bool include_spare,
                          bool verbose = false,
                          bool fetch_data = true);

} // namespace onfi::timed

#endif // ONFI_TIMED_COMMANDS_HPP
