#ifndef ONFI_ADDRESS_H
#define ONFI_ADDRESS_H

#include <stdint.h>

namespace onfi {

// Compute {col,col,row,row,row} address bytes for a given block/page
// The first `column_cycles` bytes are set to 0 for page-aligned access.
// `out` must point to a buffer large enough to hold (column_cycles+row_cycles) bytes.
void to_col_row_address(uint32_t pages_per_block,
                        uint8_t column_cycles,
                        uint8_t row_cycles,
                        unsigned int block_number,
                        unsigned int page_number,
                        uint8_t* out);

} // namespace onfi

#endif // ONFI_ADDRESS_H

