#include "nandworks/cli_parser.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace nandworks {

namespace {
struct OptionLookup {
    std::unordered_map<std::string, const OptionSpec*> by_long;
    std::unordered_map<char, const OptionSpec*> by_short;
};

OptionLookup build_lookup(const Command& command) {
    OptionLookup lookup;
    for (const auto& option : command.options) {
        if (option.long_name.empty()) {
            throw std::invalid_argument("Option long name must not be empty");
        }
        if (!lookup.by_long.emplace(option.long_name, &option).second) {
            throw std::invalid_argument("Duplicate option long name: --" + option.long_name);
        }
        if (option.short_name != '\0') {
            if (!lookup.by_short.emplace(option.short_name, &option).second) {
                std::ostringstream oss;
                oss << "Duplicate short option: -" << option.short_name;
                throw std::invalid_argument(oss.str());
            }
        }
    }
    return lookup;
}

const OptionSpec* resolve_long(const OptionLookup& lookup, std::string_view name) {
    const auto it = lookup.by_long.find(std::string(name));
    if (it == lookup.by_long.end()) return nullptr;
    return it->second;
}

const OptionSpec* resolve_short(const OptionLookup& lookup, char name) {
    const auto it = lookup.by_short.find(name);
    if (it == lookup.by_short.end()) return nullptr;
    return it->second;
}

void ensure_repeatable(const OptionSpec& option, std::unordered_map<std::string, std::vector<std::string>>& values) {
    if (!option.repeatable && values.find(option.long_name) != values.end()) {
        throw std::invalid_argument("Option '--" + option.long_name + "' specified multiple times");
    }
}

} // namespace

ParsedCommand parse_command_arguments(const Command& command, const std::vector<std::string>& raw_args) {
    auto lookup = build_lookup(command);

    std::unordered_map<std::string, std::vector<std::string>> option_values;
    std::vector<std::string> positionals;
    bool positional_mode = false;
    bool help = false;
    bool force = false;

    for (std::size_t i = 0; i < raw_args.size(); ++i) {
        const std::string& token = raw_args[i];

        if (!positional_mode && command.stop_parsing_options_after_positionals && !positionals.empty()) {
            positional_mode = true;
        }

        if (!positional_mode) {
            if (token == "--") {
                positional_mode = true;
                continue;
            }
            if (token == "--help" || token == "-h") {
                help = true;
                continue;
            }
            if (token == "--force" || token == "-f") {
                force = true;
                continue;
            }
        }

        const OptionSpec* spec = nullptr;
        std::string value;
        bool value_supplied_inline = false;

        if (!positional_mode && token.rfind("--", 0) == 0) {
            std::string without_prefix = token.substr(2);
            auto equals_pos = without_prefix.find('=');
            std::string opt_name = without_prefix;
            if (equals_pos != std::string::npos) {
                opt_name = without_prefix.substr(0, equals_pos);
                value = without_prefix.substr(equals_pos + 1);
                value_supplied_inline = true;
            }
            spec = resolve_long(lookup, opt_name);
            if (spec == nullptr) {
                throw std::invalid_argument("Unknown option '--" + opt_name + "'");
            }
            if (spec->requires_value) {
                if (!value_supplied_inline) {
                    if (++i >= raw_args.size()) {
                        throw std::invalid_argument("Option '--" + opt_name + "' expects a value");
                    }
                    value = raw_args[i];
                }
            } else if (value_supplied_inline) {
                throw std::invalid_argument("Option '--" + opt_name + "' does not take a value");
            }
        } else if (!positional_mode && token.size() >= 2 && token[0] == '-' && token[1] != '-') {
            char short_name = token[1];
            spec = resolve_short(lookup, short_name);
            if (spec == nullptr) {
                std::ostringstream oss;
                oss << "Unknown option '-" << short_name << "'";
                throw std::invalid_argument(oss.str());
            }
            if (token.size() > 2) {
                value = token.substr(2);
                value_supplied_inline = true;
            }
            if (spec->requires_value) {
                if (!value_supplied_inline) {
                    if (++i >= raw_args.size()) {
                        std::ostringstream oss;
                        oss << "Option '-" << short_name << "' expects a value";
                        throw std::invalid_argument(oss.str());
                    }
                    value = raw_args[i];
                }
            } else if (value_supplied_inline) {
                std::ostringstream oss;
                oss << "Option '-" << short_name << "' does not take a value";
                throw std::invalid_argument(oss.str());
            }
        }

        if (spec) {
            ensure_repeatable(*spec, option_values);
            if (spec->requires_value) {
                option_values[spec->long_name].push_back(value);
            } else {
                option_values[spec->long_name].push_back("true");
            }
            continue;
        }

        positionals.push_back(token);
    }

    if (!help) {
        for (const auto& option : command.options) {
            if (option.required && option_values.find(option.long_name) == option_values.end()) {
                throw std::invalid_argument("Missing required option '--" + option.long_name + "'");
            }
        }
        if (positionals.size() < command.min_positionals) {
            throw std::invalid_argument("Insufficient positional arguments");
        }
        if (command.max_positionals != static_cast<std::size_t>(-1) &&
            positionals.size() > command.max_positionals) {
            throw std::invalid_argument("Too many positional arguments");
        }
        if (command.safety == CommandSafety::RequiresForce && !force) {
            throw std::invalid_argument("Command requires --force to proceed");
        }
    }

    ParsedCommand parsed;
    parsed.arguments = CommandArguments{std::move(option_values), std::move(positionals)};
    parsed.help_requested = help;
    parsed.force = force;
    return parsed;
}

void print_command_usage(const Command& command, std::ostream& out) {
    out << "Usage: " << command.usage << "\n";
    if (!command.summary.empty()) {
        out << command.summary << "\n";
    }
    if (!command.description.empty()) {
        out << command.description << "\n";
    }
    if (!command.options.empty() || command.safety == CommandSafety::RequiresForce) {
        out << "\nOptions:\n";
        for (const auto& option : command.options) {
            out << "  --" << option.long_name;
            if (option.short_name != '\0') {
                out << ", -" << option.short_name;
            }
            if (option.requires_value) {
                out << " <" << (option.value_name.empty() ? "value" : option.value_name) << ">";
            }
            out << "\n";
            if (!option.description.empty()) {
                out << "      " << option.description << "\n";
            }
        }
        if (command.safety == CommandSafety::RequiresForce) {
            out << "  --force, -f\n"
                << "      Confirm execution of destructive operation.\n";
        }
        out << "  --help, -h\n      Show command-specific help.\n";
    }
}

} // namespace nandworks
