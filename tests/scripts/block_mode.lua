--[[
Micron block-mode regression script. Attempts to switch a block to SLC and back to MLC
using the CLI command bindings. The command is skipped when the device does not
advertise block-mode support.

Usage:
  sudo bin/nandworks script tests/scripts/block_mode.lua [block]
]]

local block = tonumber(arg[1]) or 0

print(string.rep("-", 60))
print("Micron block-mode script")
print(string.format("Target block: %d", block))
print(string.rep("-", 60))

nandworks.with_session(function(cmd)
  if not cmd.block_mode then
    print("block-mode command not available in this build; skipping")
    return
  end

  local summary_status = cmd.block_mode({ list = true, block = block, refresh = true, meta = { allow_failure = true } })
  if summary_status ~= 0 then
    print("Device does not expose block-mode support; skipping")
    return
  end

  cmd.block_mode({ block = block, mode = "slc", force = true, no_verify = false, meta = { allow_failure = true } })
  cmd.block_mode({ block = block, mode = "mlc", force = true, no_verify = false, meta = { allow_failure = true } })
  print("Completed block-mode round trip on block " .. block)
end)
