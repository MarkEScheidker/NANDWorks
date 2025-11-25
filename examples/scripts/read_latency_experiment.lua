--[=[
Measure ONFI page read latency (tR) across a region using nandworks.timed bindings.

Usage:
  sudo bin/nandworks script --allow-unsafe examples/scripts/read_latency_experiment.lua \
       [--blocks start[:count]] [--pages start[:count]] [--output file.csv] [--include-spare]

Defaults to the entire device if no ranges are provided. CSV is written to stdout unless
--output is set (requires --allow-unsafe to enable file I/O).
]=]

local function usage()
  print("Usage: sudo bin/nandworks script --allow-unsafe examples/scripts/read_latency_experiment.lua \\")
  print("         [--blocks start[:count]] [--pages start[:count]] [--output file.csv] [--include-spare]")
end

local function parse_range(text)
  local sep = string.find(text, "[:,]")
  local first = sep and string.sub(text, 1, sep - 1) or text
  local second = sep and string.sub(text, sep + 1) or nil

  local start = tonumber(first)
  local count = second and tonumber(second) or 1
  if not start or start < 0 then
    return nil, "invalid start in range: " .. text
  end
  if not count or count <= 0 then
    return nil, "invalid count in range: " .. text
  end
  return { start = start, count = count }
end

local function describe_error(timing)
  if not timing.busy_detected then
    return "rb_not_asserted"
  end
  if timing.timed_out then
    return "rb_timeout"
  end
  if not timing.ready then
    return "status_not_ready"
  end
  if not timing.pass then
    return "status_fail"
  end
  return ""
end

local function make_writer(path)
  if path then
    if not io or not io.open then
      error("File output requires --allow-unsafe to enable io.open")
    end
    local fh, err = io.open(path, "w")
    if not fh then
      error("Failed to open output '" .. tostring(path) .. "': " .. tostring(err))
    end
    return function(line) fh:write(line, "\n") end, function() fh:close() end
  end
  return function(line) print(line) end, function() end
end

local function parse_args()
  local cfg = {
    include_spare = false,
  }

  local i = 1
  while i <= #arg do
    local token = arg[i]
    if token == "--blocks" then
      i = i + 1
      if i > #arg then error("--blocks expects a value") end
      local range, err = parse_range(arg[i])
      if not range then error(err) end
      cfg.blocks = range
    elseif token == "--pages" then
      i = i + 1
      if i > #arg then error("--pages expects a value") end
      local range, err = parse_range(arg[i])
      if not range then error(err) end
      cfg.pages = range
    elseif token == "--output" then
      i = i + 1
      if i > #arg then error("--output expects a path") end
      cfg.output = arg[i]
    elseif token == "--include-spare" or token == "--include_spare" then
      cfg.include_spare = true
    elseif token == "--help" or token == "-h" then
      cfg.help = true
      break
    else
      error("Unknown argument: " .. tostring(token))
    end
    i = i + 1
  end

  return cfg
end

local function ensure_range(range, max_value, label)
  if range.start >= max_value then
    error(string.format("%s start %d exceeds geometry (%d)", label, range.start, max_value))
  end
  if range.start + range.count > max_value then
    error(string.format("%s span (%d+%d) exceeds geometry (%d)", label, range.start, range.count, max_value))
  end
end

local config = parse_args()
if config.help then
  usage()
  return
end
local include_spare = config.include_spare

nandworks.with_session(function(cmd, nw)
  local geom = nw.geometry()
  if not config.blocks then
    config.blocks = { start = 0, count = geom.blocks }
  end
  if not config.pages then
    config.pages = { start = 0, count = geom.pages_per_block }
  end

  ensure_range(config.blocks, geom.blocks, "Block")
  ensure_range(config.pages, geom.pages_per_block, "Page")

  local write_line, close_writer = make_writer(config.output)
  write_line("block,page,t_read_us,status,error")

  local rows = 0
  local successes = 0

  for b = 0, config.blocks.count - 1 do
    local block = config.blocks.start + b
    for p = 0, config.pages.count - 1 do
      local page = config.pages.start + p

      local status = 0
      local t_read_us = nil
      local error_text = ""

      local ok, timing = pcall(nw.timed.read_page, {
        block = block,
        page = page,
        include_spare = include_spare,
        fetch_data = false,
        verbose = false,
      })

      if ok then
        status = timing.status or 0
        error_text = describe_error(timing)
        if error_text == "" then
          t_read_us = (timing.duration_ns or 0) / 1000.0
          successes = successes + 1
        end
      else
        error_text = timing
      end

      if error_text ~= "" or not t_read_us then
        write_line(string.format("%d,%d,,0x%02X,%s", block, page, status, error_text))
      else
        write_line(string.format("%d,%d,%.3f,0x%02X,%s", block, page, t_read_us, status, error_text))
      end

      rows = rows + 1
    end
  end

  close_writer()
  print(string.format("Measured %d pages (%d succeeded)", rows, successes))
end)
