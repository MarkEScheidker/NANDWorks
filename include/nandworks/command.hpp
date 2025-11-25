#ifndef NANDWORKS_COMMAND_HPP
#define NANDWORKS_COMMAND_HPP

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace nandworks {

struct CommandContext;

enum class CommandSafety {
    Safe,
    RequiresForce
};

struct OptionSpec {
    std::string long_name;
    char short_name = '\0';
    bool requires_value = false;
    bool required = false;
    bool repeatable = false;
    std::string value_name;
    std::string description;
};

using CommandHandler = std::function<int(const CommandContext&)>;

struct Command {
    std::string name;
    std::vector<std::string> aliases;
    std::string summary;
    std::string description;
    std::string usage;
    std::vector<OptionSpec> options;
    std::size_t min_positionals = 0;
    std::size_t max_positionals = static_cast<std::size_t>(-1);
    CommandSafety safety = CommandSafety::Safe;
    bool requires_session = true;
    bool requires_root = true;
    bool stop_parsing_options_after_positionals = false;
    CommandHandler handler;
};

} // namespace nandworks

#endif // NANDWORKS_COMMAND_HPP
