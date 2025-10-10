-- Simple automation example for the NANDWorks LuaJIT integration.
-- Usage:
--   sudo bin/nandworks script examples/scripts/probe.lua

print(string.rep("=", 60))
print("NANDWorks Lua automation demo")
print(string.rep("=", 60))

nandworks.with_session(function(cmd)
    print("ONFI session active:", driver.is_active())

    -- Call CLI commands just like regular Lua functions.
    cmd.probe()

    print("Probe complete. Reading ONFI parameters (bytewise preview)...")
    cmd.parameters({ bytewise = true })
end)

print("Script finished successfully.")
