// Basic ONFI-related types and geometry descriptions
#ifndef ONFI_TYPES_H
#define ONFI_TYPES_H

#include <stdint.h>

namespace onfi {

// Feature command opcodes used with SET/GET FEATURES
enum class FeatureCommand : uint8_t {
    Set = 0xEF,
    Get = 0xEE,
};

// Describes the NAND array organization
struct Geometry {
    uint32_t page_size_bytes = 0;        // main area
    uint32_t spare_size_bytes = 0;       // spare (OOB) area
    uint32_t pages_per_block = 0;
    uint32_t blocks_per_lun = 0;
    uint8_t  column_cycles = 0;
    uint8_t  row_cycles = 0;
};

// Simple container for ONFI version digits
struct Version {
    char major = 'x';
    char minor = 'x';
};

} // namespace onfi

#endif // ONFI_TYPES_H
