#ifndef NANDWORKS_COMMAND_REGISTRY_HPP
#define NANDWORKS_COMMAND_REGISTRY_HPP

#include "nandworks/command.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nandworks {

class CommandRegistry {
public:
    CommandRegistry();

    Command& register_command(Command command);

    const Command* find(std::string_view name) const;

    const std::vector<Command>& commands() const noexcept { return commands_; }

private:
    std::vector<Command> commands_;
    std::unordered_map<std::string, std::size_t> lookup_;
};

} // namespace nandworks

#endif // NANDWORKS_COMMAND_REGISTRY_HPP
