#include "nandworks/cli_parser.hpp"
#include "nandworks/command_registry.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

using namespace nandworks;

int main() {
    CommandRegistry registry;

    Command parse_cmd{
        .name = "sample",
        .aliases = {"alias"},
        .summary = "",
        .description = "",
        .usage = "nandworks sample --count <n> <a> [b]",
        .options = {
            OptionSpec{"count", 'c', true, true, false, "n", ""}
        },
        .min_positionals = 1,
        .max_positionals = 2,
        .safety = CommandSafety::Safe,
        .handler = [](const CommandContext&) { return 0; }
    };

    Command& stored = registry.register_command(parse_cmd);

    ParsedCommand parsed = parse_command_arguments(stored, {"--count", "5", "alpha", "beta"});
    assert(!parsed.help_requested);
    assert(!parsed.force);
    auto count_value = parsed.arguments.value("count");
    assert(count_value.has_value());
    assert(*count_value == "5");
    const auto& count_values = parsed.arguments.values("count");
    assert(count_values.size() == 1);
    assert(count_values.front() == "5");
    try {
        auto count = parsed.arguments.require_int("count");
        assert(count == 5);
    } catch (const std::exception&) {
        assert(false && "require_int threw unexpectedly");
    }
    assert(parsed.arguments.positional_count() == 2);
    assert(parsed.arguments.positional(0) == "alpha");
    assert(parsed.arguments.positional(1) == "beta");

    const Command* by_alias = registry.find("alias");
    assert(by_alias == &stored);

    bool threw = false;
    try {
        (void)parse_command_arguments(stored, {});
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);

    ParsedCommand help_parse = parse_command_arguments(stored, {"--help"});
    assert(help_parse.help_requested);

    Command destructive{
        .name = "danger",
        .aliases = {},
        .summary = "",
        .description = "",
        .usage = "nandworks danger",
        .options = {},
        .min_positionals = 0,
        .max_positionals = 0,
        .safety = CommandSafety::RequiresForce,
        .handler = [](const CommandContext&) { return 0; }
    };

    Command& stored_destructive = registry.register_command(destructive);

    threw = false;
    try {
        (void)parse_command_arguments(stored_destructive, {});
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);

    ParsedCommand forced = parse_command_arguments(stored_destructive, {"--force"});
    assert(forced.force);

    return 0;
}
