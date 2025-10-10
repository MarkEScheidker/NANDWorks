--[[
Run the CLI bad-block scan using the new helper bindings.

Usage:
  sudo bin/nandworks script tests/scripts/bad_block_scan.lua [block]
]]

local block = arg[1] and tonumber(arg[1]) or nil
if arg[1] and not block then
  error("Expected numeric block index, received: " .. tostring(arg[1]))
end

print("Scanning for factory-marked bad blocks...")

nandworks.with_session(function(cmd)
  if block then
    cmd.scan_bad_blocks({ block = block })
  else
    cmd.scan_bad_blocks()
  end
end)

print("Bad-block scan complete.")
