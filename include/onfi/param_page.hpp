#ifndef ONFI_PARAM_PAGE_H
#define ONFI_PARAM_PAGE_H

#include <stdint.h>
#include "onfi/types.hpp"

namespace onfi {

// Decode version digits from ONFI parameter bytes 4-5
Version decode_onfi_version(uint8_t byte4, uint8_t byte5);

// Parse sizes from ONFI parameter bytes, with fallbacks matching existing behavior
uint32_t parse_page_size(uint8_t b83, uint8_t b82, uint8_t b81, uint8_t b80);
uint32_t parse_spare_size(uint8_t b85, uint8_t b84);
uint32_t parse_pages_per_block(uint8_t b95, uint8_t b94, uint8_t b93, uint8_t b92);
uint32_t parse_blocks_per_lun(uint8_t b99, uint8_t b98, uint8_t b97, uint8_t b96);

// Convenience: extract geometry and cycles from the full 256-byte parameter page
void parse_geometry_from_parameters(const uint8_t* params, Geometry& out);

} // namespace onfi

#endif // ONFI_PARAM_PAGE_H

