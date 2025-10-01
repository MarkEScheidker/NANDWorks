#include "nandworks/command_arguments.hpp"


namespace nandworks {

namespace {
const std::vector<std::string> kEmptyValues;
}

CommandArguments::CommandArguments() = default;

CommandArguments::CommandArguments(std::unordered_map<std::string, std::vector<std::string>> options,
                                   std::vector<std::string> positionals)
    : options_(std::move(options)), positionals_(std::move(positionals)) {}

bool CommandArguments::has(std::string_view long_name) const {
    return options_.find(std::string(long_name)) != options_.end();
}

const std::vector<std::string>& CommandArguments::values(std::string_view long_name) const {
    const auto it = options_.find(std::string(long_name));
    if (it == options_.end()) {
        return kEmptyValues;
    }
    return it->second;
}

std::optional<std::string> CommandArguments::value(std::string_view long_name) const {
    const auto it = options_.find(std::string(long_name));
    if (it == options_.end() || it->second.empty()) {
        return std::nullopt;
    }
    return it->second.front();
}

std::string CommandArguments::value_or(std::string_view long_name, std::string_view fallback) const {
    auto opt = value(long_name);
    if (opt) {
        return *opt;
    }
    return std::string(fallback);
}

int64_t CommandArguments::value_as_int(std::string_view long_name, int64_t fallback) const {
    auto opt = value(long_name);
    if (!opt) {
        return fallback;
    }
    const std::string& token = *opt;
    std::size_t idx = 0;
    try {
        long long parsed = std::stoll(token, &idx, 0);
        if (idx != token.size()) {
            throw std::invalid_argument("");
        }
        return static_cast<int64_t>(parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument("Option '" + std::string(long_name) + "' expects an integer value");
    }
}

int64_t CommandArguments::require_int(std::string_view long_name) const {
    auto opt = value(long_name);
    if (!opt) {
        throw std::invalid_argument("Missing required option '--" + std::string(long_name) + "'");
    }
    return value_as_int(long_name, 0);
}

std::size_t CommandArguments::positional_count() const noexcept {
    return positionals_.size();
}

const std::string& CommandArguments::positional(std::size_t index) const {
    if (index >= positionals_.size()) {
        throw std::out_of_range("positional argument index out of range");
    }
    return positionals_.at(index);
}

} // namespace nandworks
