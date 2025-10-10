#ifndef NANDWORKS_SCRIPTING_LUA_ENGINE_HPP
#define NANDWORKS_SCRIPTING_LUA_ENGINE_HPP

#include "nandworks/command_registry.hpp"

#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

namespace nandworks {
class DriverContext;
}

#if NANDWORKS_WITH_LUAJIT
extern "C" {
struct lua_State;
}
#endif

namespace nandworks::scripting {

struct ScriptOptions {
    std::string path;
    std::vector<std::string> args;
    bool allow_unsafe_libraries = false;
};

#if NANDWORKS_WITH_LUAJIT

class LuaEngine {
public:
    LuaEngine(CommandRegistry& registry,
              DriverContext& driver,
              std::ostream& out,
              std::ostream& err,
              bool verbose);
    ~LuaEngine();

    LuaEngine(const LuaEngine&) = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

    LuaEngine(LuaEngine&&) = delete;
    LuaEngine& operator=(LuaEngine&&) = delete;

    void open_standard_libraries(bool allow_unsafe);
    void register_bindings();
    int run_file(const ScriptOptions& options);

private:
    CommandRegistry& registry_;
    DriverContext& driver_;
    std::ostream& out_;
    std::ostream& err_;
    bool verbose_;
    ::lua_State* state_ = nullptr;

    void push_script_arguments(const ScriptOptions& options);

    static int lua_exec(::lua_State* L);
    static int lua_driver_start(::lua_State* L);
    static int lua_driver_shutdown(::lua_State* L);
    static int lua_driver_active(::lua_State* L);
    static int lua_command_dispatch(::lua_State* L);
    static int lua_with_session(::lua_State* L);
    static LuaEngine& from_upvalue(::lua_State* L);

    int invoke_command(const std::string& name, const std::vector<std::string>& args);
    void ensure_state() const;
};

#else // NANDWORKS_WITH_LUAJIT

class LuaEngine {
public:
    LuaEngine(CommandRegistry&, DriverContext&, std::ostream&, std::ostream&, bool) {
        throw std::runtime_error("LuaJIT support not enabled in this build");
    }
    void open_standard_libraries(bool) {}
    void register_bindings() {}
    int run_file(const ScriptOptions&) { return 1; }
};

#endif // NANDWORKS_WITH_LUAJIT

} // namespace nandworks::scripting

#endif // NANDWORKS_SCRIPTING_LUA_ENGINE_HPP
