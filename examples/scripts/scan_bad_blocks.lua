--[[
Run the factory bad-block scan using nandworks.with_session.

Usage:
  sudo bin/nandworks script examples/scripts/scan_bad_blocks.lua [block]

Provide an optional numeric block index to inspect a single block.
]]

local block = arg[1] and tonumber(arg[1]) or nil
if arg[1] and not block then
  error("Expected numeric block index, received: " .. tostring(arg[1]))
end

print(string.rep("-", 50))
if block then
  print("Scanning block " .. block .. " for factory markings")
else
  print("Scanning entire device for factory-marked bad blocks")
end
print(string.rep("-", 50))

nandworks.with_session(function(cmd)
  if block then
    cmd.scan_bad_blocks({ block = block })
  else
    cmd.scan_bad_blocks()
  end
end)

print("Bad-block scan complete.")
