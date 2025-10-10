--[[
Read a NAND page (including spare bytes) and let the CLI format the output.

Usage:
  sudo bin/nandworks script examples/scripts/read_page.lua [block] [page] [bytewise]

Pass any non-nil third argument (e.g. `true`) to request bytewise transfers.
]]

local block = tonumber(arg[1]) or 0
local page = tonumber(arg[2]) or 0
local bytewise = arg[3] ~= nil

print(string.rep("=", 60))
print("Read-page example via nandworks.commands")
print(string.format("Block=%d Page=%d Bytewise=%s", block, page, tostring(bytewise)))
print(string.rep("=", 60))

nandworks.with_session(function(cmd)
  cmd.read_page({
    block = block,
    page = page,
    include_spare = true,
    bytewise = bytewise,
  })
end)

print("Read complete.")
