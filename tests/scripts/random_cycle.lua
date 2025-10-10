--[[
Exercise a program/read/erase cycle using nandworks.with_session.

Usage:
  sudo bin/nandworks script --allow-unsafe tests/scripts/random_cycle.lua [block] [page] [length] [seed]
]]

if not os or not io then
  error("This script requires '--allow-unsafe' so it can create a temporary payload.")
end

local block = tonumber(arg[1]) or 0
local page = tonumber(arg[2]) or 0
local length = tonumber(arg[3]) or 4096
local seed = tonumber(arg[4]) or os.time()

if length <= 0 then
  error("Length must be positive, received: " .. tostring(length))
end

math.randomseed(seed)

print(string.rep("-", 60))
print("Random program/read/erase cycle via nandworks.commands")
print(string.format("Block=%d Page=%d Length=%d Seed=%d", block, page, length, seed))
print(string.rep("-", 60))

local tmp_path = os.tmpname()
local payload = io.open(tmp_path, "wb") or error("Unable to open temporary payload at " .. tmp_path)
for _ = 1, length do
  payload:write(string.char(math.random(0, 255)))
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
    cmd.verify_block({ block = block })
  end)
end)

cleanup()

if not ok then
  error(err)
end

print("Random cycle completed successfully.")
