--[[
Program a page with a deterministic pattern, verify it, and clean up.

Usage:
  sudo bin/nandworks script --allow-unsafe examples/scripts/program_page.lua [block] [page] [length]
]]

if not os or not io then
  error("This script requires '--allow-unsafe' so it can write a temporary payload file.")
end

local block = tonumber(arg[1]) or 0
local page = tonumber(arg[2]) or 0
local length = tonumber(arg[3]) or 4096

if length <= 0 then
  error("Length must be positive, received: " .. tostring(length))
end

print(string.rep("=", 60))
print("Program / verify example via nandworks.commands")
print(string.format("Block=%d Page=%d Length=%d bytes", block, page, length))
print(string.rep("=", 60))

-- Build a simple deterministic payload so verification is meaningful.
local tmp_path = os.tmpname()
local payload = io.open(tmp_path, "wb") or error("Unable to create payload at " .. tmp_path)
for i = 0, length - 1 do
  payload:write(string.char((i * 7 + 31) % 256))
end
payload:close()

local function cleanup()
  os.remove(tmp_path)
end

local ok, err = pcall(function()
  nandworks.with_session(function(cmd)
    cmd.erase_block({ block = block, force = true })
    cmd.program_page({
      block = block,
      page = page,
      input = tmp_path,
      include_spare = true,
      pad = true,
      force = true,
    })
    cmd.verify_page({
      block = block,
      page = page,
      input = tmp_path,
      include_spare = true,
    })
    cmd.erase_block({ block = block, force = true })
  end)
end)

cleanup()

if not ok then
  error(err)
end

print("Program cycle completed successfully.")
