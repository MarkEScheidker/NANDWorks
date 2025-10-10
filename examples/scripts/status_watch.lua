--[[
Poll the ONFI status register a few times to monitor ready/busy bits.

Usage:
  sudo bin/nandworks script examples/scripts/status_watch.lua [count] [raw]

Arguments:
  count  Number of polls to perform (default 3).
  raw    Provide any value to show the raw bit pattern alongside the decoded view.
]]

local count = tonumber(arg[1]) or 3
local raw = arg[2] ~= nil

if count < 1 then
  error("Count must be at least 1")
end

print("Polling ONFI status " .. count .. " time(s); raw output: " .. tostring(raw))

nandworks.with_session(function(cmd)
  for i = 1, count do
    print(string.format("\nStatus poll %d/%d", i, count))
    cmd.status({ raw = raw })
  end
end)

print("\nStatus polling finished.")
