#include "onfi/address.h"

namespace onfi {

void to_col_row_address(uint32_t pages_per_block,
                        uint8_t column_cycles,
                        uint8_t row_cycles,
                        unsigned int block_number,
                        unsigned int page_number,
                        uint8_t* out) {
    // Compute the absolute page number
    unsigned int global_page = block_number * pages_per_block + page_number;

    // Column bytes (page-aligned accesses start at column 0)
    for (uint8_t i = 0; i < column_cycles; ++i) {
        out[i] = 0;
    }

    // Row bytes (LSB-first ordering)
    for (uint8_t i = 0; i < row_cycles; ++i) {
        out[column_cycles + i] = static_cast<uint8_t>(global_page & 0xFF);
        global_page >>= 8;
    }
}

} // namespace onfi

