#include "nandworks/commands/script.hpp"

#include "nandworks/command_context.hpp"
#include "nandworks/command_registry.hpp"
#include "nandworks/command_arguments.hpp"
#include "nandworks/scripting/lua_engine.hpp"

#include <exception>
#include <ostream>
#include <string>
#include <vector>

#if NANDWORKS_WITH_LUAJIT
#include <filesystem>
extern "C" {
#include "lua.hpp"
}
#endif

namespace nandworks::commands {
namespace {

int script_command(const CommandContext& context) {
#if NANDWORKS_WITH_LUAJIT
    const auto& positionals = context.arguments.positionals();
    if (positionals.empty()) {
        context.err << "script command expects a Lua file path.\n";
        return 1;
    }

    scripting::ScriptOptions options;
    options.path = positionals.front();
    options.allow_unsafe_libraries = context.arguments.has("allow-unsafe");
    for (std::size_t i = 1; i < positionals.size(); ++i) {
        options.args.push_back(positionals[i]);
    }

    std::filesystem::path script_path(options.path);
    if (!script_path.is_absolute()) {
        script_path = std::filesystem::absolute(script_path);
        options.path = script_path.string();
    }

    if (!std::filesystem::exists(script_path)) {
        context.err << "Script file '" << options.path << "' does not exist.\n";
        return 1;
    }
    if (!std::filesystem::is_regular_file(script_path)) {
        context.err << "Script path '" << options.path << "' is not a regular file.\n";
        return 1;
    }

    try {
        scripting::LuaEngine engine(context.registry,
                                    context.driver,
                                    context.out,
                                    context.err,
                                    context.verbose);
        engine.open_standard_libraries(options.allow_unsafe_libraries);
        engine.register_bindings();
        const int status = engine.run_file(options);
        if (status != LUA_OK) {
            context.err << "Lua interpreter returned status code " << status << ".\n";
            return 6;
        }
    } catch (const std::exception& ex) {
        context.err << "Failed to run Lua script: " << ex.what() << "\n";
        return 6;
    }
    return 0;
#else
    context.err << "Lua scripting support is not enabled. Rebuild with WITH_LUAJIT=1.\n";
    return 64;
#endif
}

} // namespace

void register_script_commands(CommandRegistry& registry) {
    auto& script = registry.register_command({
        .name = "script",
        .aliases = {"lua"},
        .summary = "Execute a Lua script using the NANDWorks automation API.",
        .description = "Runs the specified Lua file with access to the standard nandworks command registry via exec(). Additional positional arguments are exposed to the script in the global 'arg' table.",
        .usage = "nandworks script [--allow-unsafe] <script.lua> [args...]",
        .options = {
            OptionSpec{
                .long_name = "allow-unsafe",
                .short_name = '\0',
                .requires_value = false,
                .required = false,
                .repeatable = false,
                .value_name = "",
                .description = "Expose Lua's os/io libraries (disabled by default). Use with caution."
            },
        },
        .min_positionals = 1,
        .max_positionals = static_cast<std::size_t>(-1),
        .safety = CommandSafety::Safe,
        .requires_session = false,
        .requires_root = false,
        .stop_parsing_options_after_positionals = true,
        .handler = script_command,
    });
    (void)script;
}

} // namespace nandworks::commands
