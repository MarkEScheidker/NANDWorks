--[[
Dump the ONFI or JEDEC parameter page to a file using nandworks.commands.

Usage:
  sudo bin/nandworks script examples/scripts/dump_parameters.lua [onfi|jedec] [output] [bytewise]

Arguments (all optional):
  mode    Either "onfi" (default) or "jedec".
  output  Destination file for the raw 256-byte page (default "parameter_page.bin").
  bytewise  Provide any value to request bytewise transfers.
]]

local mode = (arg[1] or "onfi"):lower()
local output_path = arg[2] or "parameter_page.bin"
local bytewise = arg[3] ~= nil

if mode ~= "onfi" and mode ~= "jedec" then
  error("First argument must be 'onfi' or 'jedec', received: " .. tostring(arg[1]))
end

print(string.rep("=", 60))
print("Dumping parameter page (" .. mode .. ") to " .. output_path)
print("Bytewise transfer: " .. tostring(bytewise))
print(string.rep("=", 60))

nandworks.with_session(function(cmd)
  cmd.parameters({
    jedec = (mode == "jedec"),
    bytewise = bytewise,
    raw = true,
    output = output_path,
  })
end)

print("Saved parameter page to " .. output_path)
