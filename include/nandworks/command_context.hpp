#ifndef NANDWORKS_COMMAND_CONTEXT_HPP
#define NANDWORKS_COMMAND_CONTEXT_HPP

#include "nandworks/command_arguments.hpp"

#include <iosfwd>

namespace nandworks {

class CommandRegistry;
class DriverContext;
struct Command;

struct CommandContext {
    CommandRegistry& registry;
    DriverContext& driver;
    const Command& command;
    CommandArguments arguments;
    std::ostream& out;
    std::ostream& err;
    bool verbose = false;
    bool force = false;
    bool help_requested = false;
};

} // namespace nandworks

#endif // NANDWORKS_COMMAND_CONTEXT_HPP
