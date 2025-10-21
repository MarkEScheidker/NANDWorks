# NANDWorks

NANDWorks is a C++ toolkit that lets Raspberry Pi single-board computers exercise ONFI-compliant NAND flash directly from Linux user space. It combines a high-performance hardware abstraction layer, a full ONFI protocol stack, and a collection of command-line utilities, tests, and examples so you can characterize, program, and validate NAND parts without auxiliary firmware or FPGA bridges.

## Highlights
- **End-to-end toolchain** – A single `make` discovers every application, test, and example, emits binaries under `bin/`, and (optionally) refreshes Doxygen docs.
- **Full ONFI coverage** – Handles bring-up, identification, parameter-page parsing, read/program/erase flows, bad-block checks, and verification routines validated against Micron ONFI 4.0 NAND.
- **Micron block-mode tooling** – Toggle supported Micron blocks between SLC and MLC, cache state, and automate workflows via CLI and Lua bindings.
- **Deterministic GPIO control** – Uses memory-mapped GPIO via `libbcm2835` for cycle-level timing, including busy-wait helpers for margin experiments.
- **Configurable instrumentation** – Compile-time knobs control logging verbosity and timing capture with zero overhead when disabled.
- **Documentation driven** – Source-level comments feed Doxygen (`docs/html`) for browsing the public API.

## Hardware Overview
- **Target platform:** Raspberry Pi 3 or 4 running 64-bit Raspberry Pi OS (or a comparable Debian derivative).
- **NAND devices:** Any ONFI-compliant part; geometry is discovered at runtime through the ONFI parameter page.
- **GPIO wiring:** Defaults live in `include/hardware_locations.hpp`. Update the table if your wiring differs.

| Signal | BCM GPIO | Notes |
| --- | --- | --- |
| `DQ0…DQ7` | 21, 20, 16, 12, 25, 24, 23, 18 | 8-bit bidirectional data bus |
| `WP#` | 26 | Write Protect (active low) |
| `WE#` | 19 | Write Enable (active low) |
| `ALE` | 13 | Address Latch Enable |
| `CLE` | 11 | Command Latch Enable |
| `CE#` | 22 | Chip Enable (active low) |
| `RE#` | 27 | Read Enable (active low) |
| `R/B#` | 17 | Ready/Busy indicator from the NAND |

## Prerequisites
- **Build toolchain:** `g++`, `make`, and standard libraries (install via `sudo apt install build-essential`).
- **Optional:** `doxygen` if you want HTML API docs (`sudo apt install doxygen`).
- **Permissions:** Anything that touches `/dev/mem` (all runtime tools) must run with root privileges (`sudo`).
- **Vendor library:** The repository already includes `libbcm2835` in `lib/bcm2835_install`, so no additional installation is required on the Pi.
- **Submodules:** LuaJIT is tracked as a submodule; after cloning run `git submodule update --init --recursive` (or clone with `--recurse-submodules`).

## Quick Start
```bash
# Build the static library, utilities, tests, examples, and docs
make

# Inspect the unified driver and available commands
bin/nandworks --help

# Probe the attached NAND (requires sudo on the Pi)
sudo bin/nandworks probe
```

To clean generated artifacts under `build/`, `bin/`, and `docs/html`:
```bash
make clean
```

> **Note:** `make` invokes `make docs`, which only runs Doxygen if it is present. When missing, the build prints “Doxygen not found; skipping docs” and continues.

## LuaJIT Submodule
LuaJIT lives under `lib/LuaJIT` and is tracked as a Git submodule. Common tasks:

```bash
# Populate submodules after cloning (if you skipped --recurse-submodules)
git submodule update --init --recursive

# Pull the main repo and refresh submodules to the recorded commits
git pull --rebase
git submodule update --init --recursive

# Move LuaJIT to the latest upstream commit on its default branch
git submodule update --remote lib/LuaJIT

# Inspect or pin a specific LuaJIT release tag
git -C lib/LuaJIT checkout v2.1-20230410
```

After changing the LuaJIT revision, commit both the `lib/LuaJIT` submodule pointer and `.gitmodules` (if it changed) so collaborators stay aligned.

## Lua Scripting
LuaJIT integration is enabled by default. Build normally with `make`, or disable it when toolchain support is unavailable:

```bash
# Standard build (LuaJIT on)
make

# Opt out of Lua support
make WITH_LUAJIT=0
```

Run scripts with the new `script` command:

```bash
sudo bin/nandworks script examples/scripts/probe.lua

# Forward arguments to the Lua script
sudo bin/nandworks script automate.lua -- my-block 42
```

Within Lua you now get a `nandworks` helper module (the legacy globals remain for compatibility):
- `nandworks.commands.<name>(opts)` (alias `commands.<name>`) mirrors every CLI command; hyphenated names map to underscores (`cmd.read_page{ block = 0, page = 0 }`). Boolean fields add flags, arrays feed repeatable options, and failures raise Lua errors unless you set `meta = { allow_failure = true }`.
- `nandworks.with_session(function(cmd, nw) ... end [, "onfi" | "jedec"])` ensures the driver is started before your code runs and shuts it down afterwards. The callback receives the commands table and the module context, so you can still reach `nw.driver` if you need advanced control.
- `nandworks.exec(...)` (alias `exec`) invokes a command manually and returns its status code.
- `nandworks.driver` (alias `driver`) exposes the lower-level `start_session`, `shutdown`, and `is_active` helpers.
- CLI arguments remain in the global `arg` table (`arg[0]` is the script path).

Typical automation script:

```lua
nandworks.with_session(function(cmd)
    cmd.probe()
    cmd.parameters({ bytewise = true })
    cmd.read_page({
        block = 0,
        page = 0,
        include_spare = true,
        output = "page0.bin",
    })
end)
```

By default only safe standard libraries are exposed. Pass `--allow-unsafe` to enable the Lua `os` and `io` libraries when you need filesystem access:

```bash
sudo bin/nandworks script --allow-unsafe tools/dump.lua
```

See `examples/scripts/probe.lua`, `examples/scripts/read_page.lua`, and `examples/scripts/program_page.lua` for walkthroughs. Additional automation smoke tests live under `tests/scripts/`, including `smoke.lua`, `bad_block_scan.lua`, and `random_cycle.lua`.

More sample workflows:
- `examples/scripts/dump_parameters.lua` – save the ONFI or JEDEC parameter page to disk.
- `examples/scripts/scan_bad_blocks.lua` – list factory-marked bad blocks (optionally targeting a single block).
- `examples/scripts/status_watch.lua` – poll the ONFI status register a few times to inspect ready/busy transitions.


## Unified CLI Driver
The `bin/nandworks` binary is now the primary front-end for NANDWorks. It consolidates device bring-up, data-path operations, verification helpers, and raw transport primitives:

- `bin/nandworks --help` lists every registered command with a short summary.
- `bin/nandworks help <command>` prints command-specific syntax, options, and safety notes.
- Commands that touch the hardware must be run with `sudo`; destructive flows (`program-*`, `erase-*`, raw command/address/data helpers) also require `--force` before they will execute.

### Identification & maintenance
| Command | Capability |
| --- | --- |
| `probe` | Initialise the ONFI stack, parse the parameter page, and print manufacturer/model/geometry details. |
| `read-id` | Execute READ-ID/UNIQUE-ID and emit the identifier in ASCII and hex form. |
| `status` (`--raw`) | Issue `0x70` and report ready/pass/write-protect bits (optionally the raw bitmap). |
| `parameters` (`--jedec`, `--bytewise`, `--raw`, `--output`) | Refresh the ONFI/Jedec parameter page, update cached geometry, and optionally dump the 256-byte payload. |
| `scan-bad-blocks` (`--block`) | Report factory bad blocks across the device or for a single block. |
| `reset-device`, `device-init`, `wait-ready` | Expose the HAL maintenance helpers for scripted workflows. |

### Read / inspect
| Command | Capability |
| --- | --- |
| `read-page` (`--include-spare`, `--bytewise`, `--output`) | Capture a single page to stdout or a file. |
| `read-block` (`--pages`, `--include-spare`, `--bytewise`, `--output`) | Stream a full block or page subset to a file or printable hexdump. |
| `raw-change-column` (`--column`), `raw-read-data` (`--count`) | Adjust the read pointer and pull arbitrary bytes from the bus. |

### Program & erase (require `--force`)
| Command | Capability |
| --- | --- |
| `program-page` (`--input`, `--include-spare`, `--pad`, `--verify`) | Program a single page from a buffer and optionally verify it. |
| `program-block` (`--pages`, `--input`, `--random`, `--verify`) | Program a block or list of pages using supplied or random data. |
| `erase-block`, `erase-chip` | Erase individual blocks or contiguous ranges. |
| `set-feature`, `raw-command`, `raw-address`, `raw-send-data` | Drive ONFI command/address/data cycles directly. |

### Verification & diagnostics
| Command | Capability |
| --- | --- |
| `verify-page`, `verify-block` | Compare flash contents against reference data and report byte/bit error counts. |
| `test-leds` | Pulse the indicator LEDs to confirm GPIO wiring. |

### Automation
| Command | Capability |
| --- | --- |
| `script` (`--allow-unsafe`) | Execute a Lua script with access to `exec()` for CLI commands and driver helpers; pass extra args after `--`. |

Example flows:
```bash
# Identify the attached device
sudo bin/nandworks probe

# Dump a page (including spare bytes) to disk
sudo bin/nandworks read-page --block 10 --page 4 --include-spare --output page10_4.bin

# Program and verify a page (explicit opt-in)
sudo bin/nandworks program-page --block 10 --page 4 --input payload.bin --verify --force

# Scan for factory-marked bad blocks
sudo bin/nandworks scan-bad-blocks
```

Legacy helper binaries under `bin/apps/` remain available for transitional workflows, but new automation should target `bin/nandworks` so behaviour stays centralised.

## Build Configuration
The top-level `Makefile` auto-discovers sources and produces a static core library plus per-target binaries:
- Applications: `apps/*.cpp` → `bin/apps/<name>`
- Tests: `tests/*.cpp` → `bin/tests/<name>`
- Examples: `examples/*.cpp` → `bin/examples/<name>`

Core knobs you can override on the command line (defaults shown):
- `LOG_ONFI_LEVEL=0` – Verbosity for protocol-layer logs (0–5)
- `LOG_HAL_LEVEL=0` – Verbosity for hardware abstraction logs (0–5)
- `PROFILE_TIME=0` – Enable timing capture for select operations
- `WITH_LUAJIT=1` – Build and link the embedded LuaJIT runtime (set to 0 to disable)

Convenience targets layer on common setups:
```bash
make debug     # Adds -g -O2 and enables INFO-level ONFI/HAL logs plus profiling
make trace     # Same as debug but raises ONFI/HAL to TRACE level
make profile   # Keeps default logs but enables PROFILE_TIME

# Manual override example
make LOG_ONFI_LEVEL=3 LOG_HAL_LEVEL=4 PROFILE_TIME=1
```

If you need to point at a different `libbcm2835`, adjust the include/library paths at the top of the Makefile.

## Runtime Tools
| Binary | Location | Description |
| --- | --- | --- |
| `nandworks` | `bin/nandworks` | Unified CLI covering identification, read/program/erase flows, feature access, and raw transport helpers. |
| `benchmark` | `bin/apps/benchmark` | Measures GPIO toggle rates for a range of busy-wait loop counts. |
| `erase_chip` | `bin/apps/erase_chip` | Iterates through every block and issues a full-chip erase (destructive). |
| `profiler` | `bin/apps/profiler` | Runs representative ONFI operations while streaming timing data when profiling is enabled. |
| `gpio_test` | `bin/apps/gpio_test` | Interactive harness for verifying each GPIO line and observing state changes. |
| `tester` | `bin/tests/tester` | Comprehensive regression covering erase/program/read/verify paths with randomized data. |
| `example_*` | `bin/examples/…` | Minimal programs that demonstrate embedding the ONFI API in other applications. |

Generated artifacts:
- `time_info_file.txt` – Created when binaries are built with `PROFILE_TIME=1`; records per-operation timing data.

### Timing-focused CLI commands
The main driver also exposes timing-specific variants that reuse the new low-level helpers to measure the exact busy interval reported on R/B#:

- `measure-erase --block <index> --force` – Issues a block erase and prints the nanosecond/microsecond busy duration along with the resulting status byte.
- `measure-program --block <index> --page <index> --force [--input file] [--include-spare] [--pad]` – Programs a single page (defaulting to an 0xFF fill) and reports how long the device remained busy.
- `measure-read --block <index> --page <index> [--include-spare] [--output <path>]` – Reads a page, printing it to stdout by default or writing to a file when `--output` is provided, and reports the array-to-cache transfer time.

## Library Architecture
- **Hardware Abstraction Layer (HAL):** `include/microprocessor_interface.hpp` / `src/microprocessor_interface.cpp` manage GPIO modes, signal timing, and register access using `libbcm2835`.
- **ONFI Protocol Layer:** `include/onfi_interface.hpp` and `src/onfi/*.cpp` implement reset, identification, feature access, block/page I/O, verification helpers, and higher-level utilities (controllers, data sinks, geometry helpers).
- **Timing Utilities:** `include/timing.hpp` / `src/timing.cpp` expose cycle-accurate busy waits and timestamp helpers leveraged by benchmarking and profiling tools.

The Doxygen configuration under `docs/` parses these headers to produce browsable API documentation.

## Repository Layout
```
.
├── Makefile                # Auto-discovers sources and orchestrates builds/docs
├── apps/                   # Command-line utilities (output in bin/apps)
├── bin/                    # Generated binaries grouped by category
├── build/                  # Object files and intermediate static libraries
├── docs/                   # Doxygen config; HTML output lives in docs/html
├── examples/               # Sample programs that show API usage patterns
│   └── scripts/            # Lua automation examples
├── include/                # Public headers (HAL, ONFI API, logging, timing)
├── lib/                    # Vendored dependencies
│   ├── bcm2835_install/    # Vendored bcm2835 headers and static library
│   └── LuaJIT/             # LuaJIT runtime tracked as a submodule
├── src/                    # Core library implementation files
├── tests/                  # Functional test entry points (bin/tests)
│   └── scripts/            # Lua automation smoke tests
└── README.md               # Project overview (this file)
```

## Documentation
- `docs/driver_cli.md` – Narrative guide to the unified `nandworks` CLI, its safety rails, and example workflows.

Regenerate the HTML reference under `docs/html` with:
```bash
make docs
# Then open docs/html/index.html in your browser
```

`docs/html/` is ignored by git, so you can generate and delete docs locally without affecting commits.

## Contributing
Contributions are welcome—file an issue or open a pull request:
- Match the existing formatting style (brace layout, concise inline comments).
- When changing behavior, add or extend tests under `tests/` where practical.
- Leave generated artifacts (`build/`, `bin/`, `docs/html/`) out of commits; `.gitignore` already covers them.

For hardware bring-up questions, feature requests (e.g., multi-die awareness, DMA-based transfers), or general discussion, start a thread in the issue tracker.

---
NANDWorks gives you full-stack control over ONFI NAND from Linux—automate lab workflows, run characterization experiments, or integrate the library into your own applications with confidence.
