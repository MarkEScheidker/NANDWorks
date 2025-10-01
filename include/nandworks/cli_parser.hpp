#ifndef NANDWORKS_CLI_PARSER_HPP
#define NANDWORKS_CLI_PARSER_HPP

#include "nandworks/command.hpp"
#include "nandworks/command_arguments.hpp"

#include <ostream>
#include <string>
#include <vector>

namespace nandworks {

struct ParsedCommand {
    CommandArguments arguments;
    bool help_requested = false;
    bool force = false;
};

ParsedCommand parse_command_arguments(const Command& command, const std::vector<std::string>& raw_args);

void print_command_usage(const Command& command, std::ostream& out);

} // namespace nandworks

#endif // NANDWORKS_CLI_PARSER_HPP
