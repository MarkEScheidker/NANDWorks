#include "nandworks/scripting/lua_engine.hpp"

#if NANDWORKS_WITH_LUAJIT

#include "nandworks/cli_parser.hpp"
#include "nandworks/command_context.hpp"
#include "nandworks/driver_context.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <unistd.h>

extern "C" {
#include "lua.hpp"
}

namespace {

bool is_root_user() {
    return ::geteuid() == 0;
}

std::string sanitize_identifier(std::string_view name) {
    std::string sanitized;
    sanitized.reserve(name.size());
    for (char ch : name) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch)) {
            sanitized.push_back(static_cast<char>(std::tolower(uch)));
        } else if (ch == '-' || ch == '_') {
            sanitized.push_back('_');
        } else {
            sanitized.push_back('_');
        }
    }
    if (!sanitized.empty() && std::isdigit(static_cast<unsigned char>(sanitized.front()))) {
        sanitized.insert(sanitized.begin(), '_');
    }
    return sanitized;
}

std::string normalize_option_key(std::string_view key) {
    std::string normalized;
    normalized.reserve(key.size());
    for (char ch : key) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (ch == '_') {
            normalized.push_back('-');
        } else {
            normalized.push_back(static_cast<char>(std::tolower(uch)));
        }
    }
    return normalized;
}

const nandworks::OptionSpec* find_option_spec(const nandworks::Command& command, std::string_view name) {
    for (const auto& option : command.options) {
        if (option.long_name == name) {
            return &option;
        }
    }
    return nullptr;
}

int absolute_index(lua_State* L, int index) {
    if (index > 0 || index <= LUA_REGISTRYINDEX) {
        return index;
    }
    return lua_gettop(L) + index + 1;
}

std::string lua_to_string(lua_State* L, int index, const char* context) {
    const int type = lua_type(L, index);
    switch (type) {
    case LUA_TSTRING:
    case LUA_TNUMBER: {
        size_t len = 0;
        const char* value = lua_tolstring(L, index, &len);
        return std::string(value, len);
    }
    case LUA_TBOOLEAN:
        return lua_toboolean(L, index) ? "true" : "false";
    case LUA_TNIL:
        luaL_error(L, "%s cannot be nil", context);
        break;
    default:
        luaL_error(L, "%s must be a string, number, or boolean", context);
        break;
    }
    return {};
}

std::vector<std::string> collect_array_strings(lua_State* L, int index, const char* context) {
    std::vector<std::string> values;
    const int abs_index = absolute_index(L, index);
    const size_t len = lua_objlen(L, abs_index);
    values.reserve(len);
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, abs_index, static_cast<int>(i));
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        if (lua_istable(L, -1)) {
            lua_pop(L, 1);
            luaL_error(L, "%s does not accept nested tables", context);
        }
        values.push_back(lua_to_string(L, -1, context));
        lua_pop(L, 1);
    }
    return values;
}

param_type parse_param_type_arg(const char* mode_cstr, lua_State* L) {
    const std::string original = mode_cstr ? std::string(mode_cstr) : std::string("onfi");
    std::string lowered = original;
    std::transform(lowered.begin(),
                   lowered.end(),
                   lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (lowered.empty() || lowered == "onfi") {
        return param_type::ONFI;
    }
    if (lowered == "jedec") {
        return param_type::JEDEC;
    }
    luaL_error(L, "Invalid session type '%s' (expected 'onfi' or 'jedec')", original.c_str());
    return param_type::ONFI; // Unreachable, suppress compiler warning.
}

} // namespace

namespace nandworks::scripting {

LuaEngine::LuaEngine(CommandRegistry& registry,
                     DriverContext& driver,
                     std::ostream& out,
                     std::ostream& err,
                     bool verbose)
    : registry_(registry),
      driver_(driver),
      out_(out),
      err_(err),
      verbose_(verbose),
      state_(luaL_newstate()) {
    if (state_ == nullptr) {
        throw std::runtime_error("Failed to initialise LuaJIT state");
    }
}

LuaEngine::~LuaEngine() {
    if (state_ != nullptr) {
        lua_close(state_);
        state_ = nullptr;
    }
}

void LuaEngine::ensure_state() const {
    if (state_ == nullptr) {
        throw std::runtime_error("Lua state is not initialised");
    }
}

void LuaEngine::open_standard_libraries(bool allow_unsafe) {
    ensure_state();
    luaL_openlibs(state_);
    if (!allow_unsafe) {
        lua_pushnil(state_);
        lua_setglobal(state_, LUA_OSLIBNAME);
        lua_pushnil(state_);
        lua_setglobal(state_, LUA_IOLIBNAME);
    }
}

LuaEngine& LuaEngine::from_upvalue(lua_State* L) {
    void* context = lua_touserdata(L, lua_upvalueindex(1));
    if (context == nullptr) {
        luaL_error(L, "missing scripting context");
    }
    return *static_cast<LuaEngine*>(context);
}

int LuaEngine::lua_exec(lua_State* L) {
    LuaEngine& engine = from_upvalue(L);
    int nargs = lua_gettop(L);
    if (nargs < 1) {
        return luaL_error(L, "exec expects at least one argument (command name)");
    }

    const char* command_name = luaL_checkstring(L, 1);
    std::vector<std::string> args;
    for (int i = 2; i <= nargs; ++i) {
        if (lua_isnil(L, i)) continue;
        size_t len = 0;
        const char* value = luaL_checklstring(L, i, &len);
        args.emplace_back(value, len);
    }

    int status = engine.invoke_command(command_name, args);
    lua_pushinteger(L, status);
    return 1;
}

int LuaEngine::lua_driver_start(lua_State* L) {
    LuaEngine& engine = from_upvalue(L);
    const char* mode = luaL_optstring(L, 1, "onfi");
    param_type type = parse_param_type_arg(mode, L);

    try {
        engine.driver_.require_onfi_started(type);
    } catch (const std::exception& ex) {
        return luaL_error(L, "failed to start ONFI session: %s", ex.what());
    } catch (...) {
        return luaL_error(L, "failed to start ONFI session: unknown error");
    }
    lua_pushboolean(L, 1);
    return 1;
}

int LuaEngine::lua_driver_shutdown(lua_State* L) {
    LuaEngine& engine = from_upvalue(L);
    engine.driver_.shutdown();
    lua_pushboolean(L, 1);
    return 1;
}

int LuaEngine::lua_driver_active(lua_State* L) {
    LuaEngine& engine = from_upvalue(L);
    lua_pushboolean(L, engine.driver_.onfi_started() ? 1 : 0);
    return 1;
}

void LuaEngine::register_bindings() {
    ensure_state();

    lua_pushlightuserdata(state_, this);
    lua_pushcclosure(state_, &LuaEngine::lua_exec, 1);
    lua_setglobal(state_, "exec");

    lua_newtable(state_);
    const int driver_index = lua_gettop(state_);

    lua_pushlightuserdata(state_, this);
    lua_pushcclosure(state_, &LuaEngine::lua_driver_start, 1);
    lua_setfield(state_, driver_index, "start_session");

    lua_pushlightuserdata(state_, this);
    lua_pushcclosure(state_, &LuaEngine::lua_driver_shutdown, 1);
    lua_setfield(state_, driver_index, "shutdown");

    lua_pushlightuserdata(state_, this);
    lua_pushcclosure(state_, &LuaEngine::lua_driver_active, 1);
    lua_setfield(state_, driver_index, "is_active");

    lua_setglobal(state_, "driver");

    lua_newtable(state_);
    const int commands_index = lua_gettop(state_);

    const auto& registered_commands = registry_.commands();
    for (const auto& command : registered_commands) {
        auto bind_command = [&](const std::string& key) {
            if (key.empty()) return;
            lua_pushlightuserdata(state_, this);
            lua_pushlightuserdata(state_, const_cast<Command*>(&command));
            lua_pushcclosure(state_, &LuaEngine::lua_command_dispatch, 2);
            lua_setfield(state_, commands_index, key.c_str());
        };

        bind_command(command.name);
        std::string sanitized = sanitize_identifier(command.name);
        if (!sanitized.empty() && sanitized != command.name) {
            bind_command(sanitized);
        }
        for (const auto& alias : command.aliases) {
            if (alias.empty()) continue;
            bind_command(alias);
            std::string alias_id = sanitize_identifier(alias);
            if (!alias_id.empty() && alias_id != alias) {
                bind_command(alias_id);
            }
        }
    }

    lua_pushvalue(state_, commands_index);
    lua_setglobal(state_, "commands");

    lua_newtable(state_);
    const int module_index = lua_gettop(state_);

    lua_pushlightuserdata(state_, this);
    lua_pushcclosure(state_, &LuaEngine::lua_exec, 1);
    lua_setfield(state_, module_index, "exec");

    lua_getglobal(state_, "driver");
    lua_setfield(state_, module_index, "driver");

    lua_pushvalue(state_, commands_index);
    lua_setfield(state_, module_index, "commands");

    lua_pushlightuserdata(state_, this);
    lua_pushcclosure(state_, &LuaEngine::lua_with_session, 1);
    lua_setfield(state_, module_index, "with_session");

    lua_pushlightuserdata(state_, this);
    lua_pushcclosure(state_, &LuaEngine::lua_with_session, 1);
    lua_setglobal(state_, "with_session");

    lua_pushvalue(state_, module_index);
    lua_setglobal(state_, "nandworks");

    lua_pop(state_, 1); // module table
    lua_pop(state_, 1); // commands table
}

void LuaEngine::push_script_arguments(const ScriptOptions& options) {
    ensure_state();
    lua_newtable(state_);

    lua_pushinteger(state_, 0);
    lua_pushlstring(state_, options.path.c_str(), options.path.size());
    lua_settable(state_, -3);

    for (std::size_t index = 0; index < options.args.size(); ++index) {
        lua_pushinteger(state_, static_cast<lua_Integer>(index + 1));
        const std::string& value = options.args[index];
        lua_pushlstring(state_, value.c_str(), value.size());
        lua_settable(state_, -3);
    }

    lua_setglobal(state_, "arg");
}

int LuaEngine::lua_command_dispatch(lua_State* L) {
    LuaEngine& engine = from_upvalue(L);
    void* command_ptr = lua_touserdata(L, lua_upvalueindex(2));
    if (command_ptr == nullptr) {
        return luaL_error(L, "internal error: missing command binding");
    }
    const Command& command = *static_cast<Command*>(command_ptr);

    std::vector<std::string> option_tokens;
    std::vector<std::string> positional_tokens;
    bool force_flag = false;
    bool help_flag = false;
    bool allow_failure = false;

    const int nargs = lua_gettop(L);
    int positional_start = 1;

    if (nargs >= 1 && lua_istable(L, 1)) {
        const int table_index = absolute_index(L, 1);
        lua_pushnil(L);
        while (lua_next(L, table_index) != 0) {
            if (lua_type(L, -2) != LUA_TSTRING) {
                lua_pop(L, 1);
                return luaL_error(L, "options table keys must be strings");
            }
            const char* raw_key = lua_tostring(L, -2);
            std::string key = raw_key ? raw_key : "";
            std::string normalized = normalize_option_key(key);

            if (normalized == "args" || normalized == "positionals") {
                if (!lua_istable(L, -1)) {
                    lua_pop(L, 1);
                    return luaL_error(L, "options.%s must be an array", normalized.c_str());
                }
                const std::string context = "options." + normalized;
                auto values = collect_array_strings(L, -1, context.c_str());
                for (auto& value : values) {
                    positional_tokens.push_back(std::move(value));
                }
                lua_pop(L, 1);
                continue;
            }

            if (normalized == "meta") {
                if (!lua_istable(L, -1)) {
                    lua_pop(L, 1);
                    return luaL_error(L, "options.meta must be a table");
                }
                const int meta_index = absolute_index(L, -1);
                lua_getfield(L, meta_index, "allow_failure");
                if (!lua_isnil(L, -1)) {
                    allow_failure = lua_toboolean(L, -1) != 0;
                }
                lua_pop(L, 1);
                lua_pop(L, 1);
                continue;
            }

            if (normalized == "allow-failure") {
                if (!lua_isboolean(L, -1) && !lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    return luaL_error(L, "options.allow_failure expects a boolean");
                }
                allow_failure = lua_toboolean(L, -1) != 0;
                lua_pop(L, 1);
                continue;
            }

            if (normalized == "force") {
                if (!lua_isboolean(L, -1) && !lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    return luaL_error(L, "options.force expects a boolean");
                }
                force_flag = lua_toboolean(L, -1) != 0;
                lua_pop(L, 1);
                continue;
            }

            if (normalized == "help") {
                if (!lua_isboolean(L, -1) && !lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    return luaL_error(L, "options.help expects a boolean");
                }
                help_flag = lua_toboolean(L, -1) != 0;
                lua_pop(L, 1);
                continue;
            }

            const OptionSpec* spec = find_option_spec(command, normalized);
            if (spec == nullptr) {
                lua_pop(L, 1);
                return luaL_error(L, "unknown option '%s' for command '%s'", key.c_str(), command.name.c_str());
            }

            if (!spec->requires_value) {
                if (!lua_isboolean(L, -1) && !lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    return luaL_error(L, "option '--%s' expects a boolean", spec->long_name.c_str());
                }
                if (lua_toboolean(L, -1)) {
                    option_tokens.push_back(std::string("--") + spec->long_name);
                }
                lua_pop(L, 1);
                continue;
            }

            if (lua_istable(L, -1)) {
                const std::string context = "option '--" + spec->long_name + "'";
                auto values = collect_array_strings(L, -1, context.c_str());
                if (values.empty()) {
                    lua_pop(L, 1);
                    return luaL_error(L, "option '--%s' expects at least one value", spec->long_name.c_str());
                }
                if (!spec->repeatable && values.size() > 1) {
                    lua_pop(L, 1);
                    return luaL_error(L, "option '--%s' does not accept multiple values", spec->long_name.c_str());
                }
                for (auto& value : values) {
                    option_tokens.push_back(std::string("--") + spec->long_name);
                    option_tokens.push_back(std::move(value));
                }
                lua_pop(L, 1);
                continue;
            }

            const std::string context = "option '--" + spec->long_name + "'";
            std::string value = lua_to_string(L, -1, context.c_str());
            option_tokens.push_back(std::string("--") + spec->long_name);
            option_tokens.push_back(std::move(value));
            lua_pop(L, 1);
        }
        positional_start = 2;
    }

    for (int index = positional_start; index <= nargs; ++index) {
        if (lua_isnil(L, index)) continue;
        if (lua_istable(L, index)) {
            return luaL_error(L, "positional arguments must be string-like values");
        }
        positional_tokens.push_back(lua_to_string(L, index, "positional argument"));
    }

    if (!help_flag && command.safety == CommandSafety::RequiresForce && !force_flag) {
        return luaL_error(L,
                          "command '%s' requires confirmation. Set options.force = true to pass --force.",
                          command.name.c_str());
    }

    if (force_flag) {
        option_tokens.emplace_back("--force");
    }
    if (help_flag) {
        option_tokens.emplace_back("--help");
    }

    std::vector<std::string> args;
    args.reserve(option_tokens.size() + positional_tokens.size());
    for (auto& token : option_tokens) {
        args.push_back(std::move(token));
    }
    for (auto& token : positional_tokens) {
        args.push_back(std::move(token));
    }

    int status = engine.invoke_command(command.name, args);
    if (status != 0) {
        if (!allow_failure) {
            return luaL_error(L, "command '%s' failed with status %d", command.name.c_str(), status);
        }
        lua_pushboolean(L, 0);
        lua_pushinteger(L, status);
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

int LuaEngine::lua_with_session(lua_State* L) {
    LuaEngine& engine = from_upvalue(L);
    luaL_checktype(L, 1, LUA_TFUNCTION);

    const char* mode_cstr = luaL_optstring(L, 2, "onfi");
    std::string mode_label = mode_cstr ? std::string(mode_cstr) : std::string("onfi");
    param_type type = parse_param_type_arg(mode_cstr, L);

    lua_settop(L, 1);

    const bool already_active = engine.driver_.onfi_started();
    try {
        engine.driver_.require_onfi_started(type);
    } catch (const std::exception& ex) {
        return luaL_error(L, "failed to start %s session: %s", mode_label.c_str(), ex.what());
    } catch (...) {
        return luaL_error(L, "failed to start %s session: unknown error", mode_label.c_str());
    }

    const bool started_here = !already_active;

    lua_getglobal(L, "commands");
    if (lua_isnil(L, -1)) {
        if (started_here) {
            engine.driver_.shutdown();
        }
        return luaL_error(L, "nandworks scripting environment unavailable (commands table missing)");
    }

    lua_getglobal(L, "nandworks");
    if (lua_isnil(L, -1)) {
        if (started_here) {
            engine.driver_.shutdown();
        }
        return luaL_error(L, "nandworks scripting environment unavailable (module missing)");
    }

    const int status = lua_pcall(L, 2, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        if (started_here) {
            engine.driver_.shutdown();
        }
        const char* message = lua_tostring(L, -1);
        if (message == nullptr) {
            lua_pop(L, 1);
            lua_pushliteral(L, "with_session callback failed");
        }
        return lua_error(L);
    }

    const int result_count = lua_gettop(L);
    if (started_here) {
        engine.driver_.shutdown();
    }
    return result_count;
}

int LuaEngine::run_file(const ScriptOptions& options) {
    ensure_state();
    push_script_arguments(options);

    const int load_status = luaL_loadfile(state_, options.path.c_str());
    if (load_status != LUA_OK) {
        const char* message = lua_tostring(state_, -1);
        err_ << "Failed to load script '" << options.path << "': "
             << (message ? message : "unknown error") << "\n";
        lua_pop(state_, 1);
        return load_status;
    }

    const int call_status = lua_pcall(state_, 0, LUA_MULTRET, 0);
    if (call_status != LUA_OK) {
        const char* message = lua_tostring(state_, -1);
        err_ << "Script '" << options.path << "' failed: "
             << (message ? message : "unknown error") << "\n";
        lua_pop(state_, 1);
        return call_status;
    }
    return LUA_OK;
}

int LuaEngine::invoke_command(const std::string& name, const std::vector<std::string>& args) {
    const Command* command = registry_.find(name);
    if (command == nullptr) {
        err_ << "exec: unknown command '" << name << "'\n";
        return 1;
    }

    ParsedCommand parsed;
    try {
        parsed = parse_command_arguments(*command, args);
    } catch (const std::exception& ex) {
        err_ << "exec: argument error for '" << name << "': " << ex.what() << "\n";
        print_command_usage(*command, err_);
        return 2;
    }

    if (parsed.help_requested) {
        print_command_usage(*command, out_);
        return 0;
    }

    if (command->requires_root && !is_root_user()) {
        err_ << "exec: command '" << command->name << "' requires root privileges\n";
        return 5;
    }

    CommandContext context{
        registry_,
        driver_,
        *command,
        std::move(parsed.arguments),
        out_,
        err_,
        verbose_,
        parsed.force,
        parsed.help_requested};

    try {
        return command->handler(context);
    } catch (const std::exception& ex) {
        err_ << "exec: command '" << command->name << "' failed: " << ex.what() << "\n";
    } catch (...) {
        err_ << "exec: command '" << command->name << "' failed with an unknown error\n";
    }
    return 4;
}

} // namespace nandworks::scripting

#endif // NANDWORKS_WITH_LUAJIT
