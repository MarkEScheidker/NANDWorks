#include "nandworks/cli_parser.hpp"
#include "nandworks/command_context.hpp"
#include "nandworks/command_registry.hpp"
#include "nandworks/commands/onfi.hpp"
#include "nandworks/driver_context.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

namespace {

bool is_root_user() {
    return ::geteuid() == 0;
}

constexpr const char* kDriverBanner = "nandworks";
constexpr const char* kDriverVersion = "0.3.0-dev";

void print_global_help(const nandworks::CommandRegistry& registry, std::ostream& out, bool verbose) {
    out << kDriverBanner << " (" << kDriverVersion << ")" << (verbose ? " [verbose]" : "") << "\n";
    out << "Usage: nandworks [--verbose] <command> [options]\n";
    out << "       nandworks --help\n";
    out << "       nandworks help <command>\n\n";
    out << "Commands:\n";
    for (const auto& command : registry.commands()) {
        out << "  " << command.name;
        if (!command.aliases.empty()) {
            out << " (";
            for (std::size_t i = 0; i < command.aliases.size(); ++i) {
                out << command.aliases[i];
                if (i + 1 < command.aliases.size()) out << ", ";
            }
            out << ")";
        }
        if (!command.summary.empty()) {
            out << "\n    " << command.summary;
        }
        out << "\n";
    }
}

int help_command(const nandworks::CommandContext& context) {
    if (context.arguments.positional_count() == 0) {
        print_global_help(context.registry, context.out, context.verbose);
        return 0;
    }
    if (context.arguments.positional_count() > 1) {
        context.err << "help takes at most one argument." << "\n";
        return 1;
    }
    const std::string& target = context.arguments.positional(0);
    const nandworks::Command* command = context.registry.find(target);
    if (command == nullptr) {
        context.err << "Unknown command: " << target << "\n";
        return 1;
    }
    nandworks::print_command_usage(*command, context.out);
    return 0;
}

int version_command(const nandworks::CommandContext& context) {
    context.out << kDriverBanner << "\n";
    context.out << "Version: " << kDriverVersion << "\n";
    context.out << "Verbose: " << (context.verbose ? "yes" : "no") << "\n";
    context.out << "Session active: " << (context.driver.onfi_started() ? "yes" : "no") << "\n";
    return 0;
}

void register_builtin_commands(nandworks::CommandRegistry& registry) {
    auto& help = registry.register_command({
        .name = "help",
        .aliases = {"?", "list"},
        .summary = "Display help for all commands or a specific command.",
        .description = "Without arguments prints the global command list; otherwise shows detailed usage for the given command.",
        .usage = "nandworks help [command]",
        .options = {},
        .min_positionals = 0,
        .max_positionals = 1,
        .safety = nandworks::CommandSafety::Safe,
        .handler = help_command,
    });
    help.requires_session = false;
    help.requires_root = false;

    auto& version = registry.register_command({
        .name = "version",
        .aliases = {"about"},
        .summary = "Print the driver version and session state.",
        .description = "Reports the CLI version string and whether an ONFI session is currently active.",
        .usage = "nandworks version",
        .options = {},
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = nandworks::CommandSafety::Safe,
        .handler = version_command,
    });
    version.requires_session = false;
    version.requires_root = false;
}

} // namespace

int main(int argc, char** argv) {
    bool verbose = false;
    bool global_help = false;
    bool list_commands = false;
    std::string command_name;
    std::vector<std::string> raw_args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
            continue;
        }
        if ((arg == "--help" || arg == "-h") && command_name.empty()) {
            global_help = true;
            continue;
        }
        if (arg == "--list-commands" && command_name.empty()) {
            list_commands = true;
            continue;
        }
        if (command_name.empty()) {
            command_name = std::move(arg);
        } else {
            raw_args.emplace_back(std::move(arg));
        }
    }

    nandworks::CommandRegistry registry;
    register_builtin_commands(registry);
    nandworks::commands::register_onfi_commands(registry);

    if ((global_help || list_commands) && command_name.empty()) {
        print_global_help(registry, std::cout, verbose);
        return 0;
    }

    if (command_name.empty()) {
        std::cerr << "No command specified. Use --help to list commands." << "\n";
        return 1;
    }

    const nandworks::Command* command = registry.find(command_name);
    if (command == nullptr) {
        std::cerr << "Unknown command: " << command_name << "\n";
        print_global_help(registry, std::cerr, verbose);
        return 2;
    }

    nandworks::ParsedCommand parsed;
    try {
        parsed = nandworks::parse_command_arguments(*command, raw_args);
    } catch (const std::exception& ex) {
        std::cerr << "Argument error: " << ex.what() << "\n";
        nandworks::print_command_usage(*command, std::cerr);
        return 3;
    }

    if (parsed.help_requested) {
        nandworks::print_command_usage(*command, std::cout);
        return 0;
    }

    if (command->requires_root && !is_root_user()) {
        std::cerr << "Command '" << command->name << "' requires root privileges. Please rerun with sudo." << "\n";
        return 5;
    }

    nandworks::DriverContext driver(verbose);

    nandworks::CommandContext context{registry, driver, *command, std::move(parsed.arguments), std::cout, std::cerr, verbose, parsed.force, parsed.help_requested};

    try {
        return command->handler(context);
    } catch (const std::exception& ex) {
        context.err << "Command '" << command->name << "' failed: " << ex.what() << "\n";
    } catch (...) {
        context.err << "Command '" << command->name << "' failed with an unknown error." << "\n";
    }
    return 4;
}
