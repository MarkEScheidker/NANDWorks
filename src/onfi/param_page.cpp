#include "onfi/param_page.hpp"

namespace onfi {

static inline uint32_t u32_from_bytes(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0)
{
    return (((uint32_t)b3 << 24) | ((uint32_t)b2 << 16) | ((uint32_t)b1 << 8) | (uint32_t)b0);
}

Version decode_onfi_version(uint8_t byte4, uint8_t byte5)
{
    Version v;
    if (byte4 == 0x30 && (byte5 & 0x04)) { v.major = '4'; v.minor = '1'; return v; }
    if (byte4 == 0x30 && (byte5 & 0x02)) { v.major = '4'; v.minor = '0'; return v; }
    if (byte5 & 0x80) { v.major = '3'; v.minor = '1'; return v; }
    if (byte5 & 0x40) { v.major = '3'; v.minor = '0'; return v; }
    if (byte5 & 0x20) { v.major = '2'; v.minor = '3'; return v; }
    if (byte5 & 0x10) { v.major = '2'; v.minor = '2'; return v; }
    if (byte5 & 0x08) { v.major = '2'; v.minor = '1'; return v; }
    if (byte5 & 0x04) { v.major = '2'; v.minor = '0'; return v; }
    if (byte5 & 0x02) { v.major = '1'; v.minor = '0'; return v; }
    // else {'x','x'}
    return v;
}

uint32_t parse_page_size(uint8_t b83, uint8_t b82, uint8_t b81, uint8_t b80)
{
    uint32_t v = u32_from_bytes(b83, b82, b81, b80);
    if (v == 0 || v == 0xFFFFFFFFu) return 2048u;
    return v;
}

uint32_t parse_spare_size(uint8_t b85, uint8_t b84)
{
    uint32_t v = ((uint32_t)b85 << 8) | (uint32_t)b84;
    if (v == 0 || v == 0xFFFFu) return 128u;
    return v;
}

uint32_t parse_pages_per_block(uint8_t b95, uint8_t b94, uint8_t b93, uint8_t b92)
{
    uint32_t v = u32_from_bytes(b95, b94, b93, b92);
    if (v == 0 || v == 0xFFFFFFFFu) return 64u;
    return v;
}

uint32_t parse_blocks_per_lun(uint8_t b99, uint8_t b98, uint8_t b97, uint8_t b96)
{
    uint32_t v = u32_from_bytes(b99, b98, b97, b96);
    if (v == 0 || v == 0xFFFFFFFFu) return 64u;
    return v;
}

void parse_geometry_from_parameters(const uint8_t* p, Geometry& out)
{
    out.page_size_bytes   = parse_page_size(p[83], p[82], p[81], p[80]);
    out.spare_size_bytes  = parse_spare_size(p[85], p[84]);
    out.pages_per_block   = parse_pages_per_block(p[95], p[94], p[93], p[92]);
    out.blocks_per_lun    = parse_blocks_per_lun(p[99], p[98], p[97], p[96]);
    out.column_cycles     = (p[101] & 0xF0) >> 4;
    out.row_cycles        = (p[101] & 0x0F);
}

} // namespace onfi

