#include "nandworks/command_registry.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace nandworks {

namespace {
std::string normalize(std::string_view name) {
    std::string normalized{name};
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return normalized;
}
}

CommandRegistry::CommandRegistry() = default;

Command& CommandRegistry::register_command(Command command) {
    if (command.name.empty()) {
        throw std::invalid_argument("command name must not be empty");
    }
    const std::string canonical = normalize(command.name);
    if (lookup_.count(canonical) != 0U) {
        throw std::invalid_argument("duplicate command name: " + command.name);
    }
    const std::size_t index = commands_.size();
    commands_.push_back(std::move(command));
    lookup_.emplace(canonical, index);

    for (const auto& alias : commands_.back().aliases) {
        const std::string alias_key = normalize(alias);
        if (lookup_.count(alias_key) != 0U) {
            throw std::invalid_argument("duplicate command alias: " + alias);
        }
        lookup_.emplace(alias_key, index);
    }

    return commands_.back();
}

const Command* CommandRegistry::find(std::string_view name) const {
    const auto it = lookup_.find(normalize(name));
    if (it == lookup_.end()) return nullptr;
    return &commands_.at(it->second);
}

} // namespace nandworks
