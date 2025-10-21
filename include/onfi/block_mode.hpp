#ifndef ONFI_BLOCK_MODE_HPP
#define ONFI_BLOCK_MODE_HPP

#include <cstdint>
#include <string_view>

namespace onfi {

/**
 * @brief Enumerates the logical operating mode for a NAND block when
 *        toggling between SLC and MLC representations.
 */
enum class BlockMode : uint8_t {
    Unknown = 0,
    Slc = 1,
    Mlc = 2,
};

/**
 * @brief Convert a block mode value to a human readable token.
 */
std::string_view to_string(BlockMode mode);

/**
 * @brief Parse a textual description into a block mode.
 * @throws std::invalid_argument when the token is not recognised.
 */
BlockMode parse_block_mode(std::string_view token);

} // namespace onfi

#endif // ONFI_BLOCK_MODE_HPP
