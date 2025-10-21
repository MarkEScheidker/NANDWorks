#include "onfi/block_mode.hpp"

#include "logging.hpp"
#include "onfi_interface.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace onfi {

std::string_view to_string(BlockMode mode) {
    switch (mode) {
    case BlockMode::Slc: return "slc";
    case BlockMode::Mlc: return "mlc";
    case BlockMode::Unknown: default: return "unknown";
    }
}

BlockMode parse_block_mode(std::string_view token) {
    std::string lowered(token.begin(), token.end());
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (lowered == "slc" || lowered == "1" || lowered == "single") {
        return BlockMode::Slc;
    }
    if (lowered == "mlc" || lowered == "multi" || lowered == "2") {
        return BlockMode::Mlc;
    }
    if (lowered == "unknown" || lowered.empty()) {
        return BlockMode::Unknown;
    }
    throw std::invalid_argument("Unrecognised block mode token: " + lowered);
}

} // namespace onfi

namespace {

constexpr uint8_t kMicronBlockModeFeature = 0x90;
constexpr uint8_t kMicronBlockAddressFeature = 0x91;

} // namespace

using onfi::BlockMode;

void onfi_interface::set_feature_hooks(FeatureWriteHook write_hook, FeatureReadHook read_hook) const {
    std::lock_guard<std::mutex> lock(block_mode_mutex_);
    feature_write_hook_ = std::move(write_hook);
    feature_read_hook_ = std::move(read_hook);
}

void onfi_interface::ensure_block_mode_cache() {
    if (block_mode_cache_.size() != num_blocks) {
        block_mode_cache_.assign(num_blocks, BlockMode::Unknown);
    }
}

void onfi_interface::encode_block_address(unsigned int block, std::array<uint8_t, 4>& payload) const {
    payload = {0, 0, 0, 0};
    payload[0] = static_cast<uint8_t>(block & 0xFF);
    payload[1] = static_cast<uint8_t>((block >> 8) & 0xFF);
}

std::array<uint8_t, 4> onfi_interface::encode_block_mode_payload(BlockMode mode) const {
    std::array<uint8_t, 4> payload{0, 0, 0, 0};
    switch (mode) {
    case BlockMode::Slc:
        payload[0] = 0x01;
        break;
    case BlockMode::Mlc:
        payload[0] = 0x00;
        break;
    case BlockMode::Unknown:
    default:
        payload[0] = 0xFF;
        break;
    }
    return payload;
}

BlockMode onfi_interface::decode_block_mode_payload(const std::array<uint8_t, 4>& payload) const {
    const uint8_t value = payload[0];
    if (value == 0x01) return BlockMode::Slc;
    if (value == 0x00) return BlockMode::Mlc;
    return BlockMode::Unknown;
}

bool onfi_interface::fetch_block_mode(unsigned int block, std::array<uint8_t, 4>& payload) {
    std::array<uint8_t, 4> address_payload{};
    encode_block_address(block, address_payload);
    set_features(kMicronBlockAddressFeature, address_payload.data(), onfi::FeatureCommand::Set);
    wait_ready_blocking();
    get_features(kMicronBlockModeFeature, payload.data(), onfi::FeatureCommand::Get);
    return true;
}

bool onfi_interface::supports_block_mode_toggle() const {
    return block_mode_supported_;
}

BlockMode onfi_interface::get_block_mode(unsigned int block, bool refresh) {
    if (!supports_block_mode_toggle()) {
        throw std::runtime_error("Block mode toggling not supported on this device");
    }
    if (block >= num_blocks) {
        throw std::out_of_range("Block index out of range");
    }

    std::lock_guard<std::mutex> lock(block_mode_mutex_);
    ensure_block_mode_cache();

    if (refresh || block_mode_cache_[block] == BlockMode::Unknown) {
        std::array<uint8_t, 4> raw{};
        if (fetch_block_mode(block, raw)) {
            block_mode_cache_[block] = decode_block_mode_payload(raw);
        }
    }
    return block_mode_cache_[block];
}

void onfi_interface::set_block_mode(unsigned int block,
                                    BlockMode mode,
                                    bool force_erase,
                                    bool verify,
                                    bool verbose) {
    if (!supports_block_mode_toggle()) {
        throw std::runtime_error("Block mode toggling not supported on this device");
    }
    if (block >= num_blocks) {
        throw std::out_of_range("Block index out of range");
    }

    {
        std::lock_guard<std::mutex> lock(block_mode_mutex_);
        ensure_block_mode_cache();
    }

    if (!force_erase) {
    if (verbose) {
        LOG_ONFI_INFO("Erasing block %u prior to block-mode change", block);
    }
        erase_block(block, verbose);
        wait_ready_blocking();
    }

    std::array<uint8_t, 4> address_payload{};
    encode_block_address(block, address_payload);
    set_features(kMicronBlockAddressFeature, address_payload.data(), onfi::FeatureCommand::Set);
    wait_ready_blocking();

    auto mode_payload = encode_block_mode_payload(mode);
    set_features(kMicronBlockModeFeature, mode_payload.data(), onfi::FeatureCommand::Set);
    wait_ready_blocking();

    BlockMode confirmed = mode;
    if (verify) {
        std::array<uint8_t, 4> raw{};
        if (fetch_block_mode(block, raw)) {
            confirmed = decode_block_mode_payload(raw);
        }
    }

    if (verbose) {
        const std::string confirmed_str(onfi::to_string(confirmed));
        const std::string requested_str(onfi::to_string(mode));
        LOG_ONFI_INFO("Block %u configured for %s mode (requested %s)",
                      block,
                      confirmed_str.c_str(),
                      requested_str.c_str());
    }

    std::lock_guard<std::mutex> lock(block_mode_mutex_);
    block_mode_cache_[block] = confirmed;
}

void onfi_interface::invalidate_block_mode_cache() {
    std::lock_guard<std::mutex> lock(block_mode_mutex_);
    block_mode_cache_.assign(block_mode_cache_.size(), BlockMode::Unknown);
}

void onfi_interface::update_block_mode_support(bool supported) {
    block_mode_supported_ = supported;
    if (!supported) {
        std::lock_guard<std::mutex> lock(block_mode_mutex_);
        block_mode_cache_.clear();
    }
}
