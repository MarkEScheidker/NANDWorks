# NANDWorks

NANDWorks is a C++ toolkit that lets Raspberry Pi single-board computers exercise ONFI-compliant NAND flash directly from Linux user space. It combines a high-performance hardware abstraction layer, a full ONFI protocol stack, and a collection of command-line utilities, tests, and examples so you can characterize, program, and validate NAND parts without auxiliary firmware or FPGA bridges.

## Highlights
- **End-to-end toolchain** – A single `make` discovers every application, test, and example, emits binaries under `bin/`, and (optionally) refreshes Doxygen docs.
- **Full ONFI coverage** – Handles bring-up, identification, parameter-page parsing, read/program/erase flows, bad-block checks, and verification routines validated against Micron ONFI 4.0 NAND.
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

## Quick Start
```bash
# Build the static library, utilities, tests, examples, and docs
make

# Discover available binaries and categories
make help

# Run the GPIO benchmark (requires sudo on the Pi)
sudo bin/apps/benchmark
```

To clean generated artifacts under `build/`, `bin/`, and `docs/html`:
```bash
make clean
```

> **Note:** `make` invokes `make docs`, which only runs Doxygen if it is present. When missing, the build prints “Doxygen not found; skipping docs” and continues.

## Build Configuration
The top-level `Makefile` auto-discovers sources and produces a static core library plus per-target binaries:
- Applications: `apps/*.cpp` → `bin/apps/<name>`
- Tests: `tests/*.cpp` → `bin/tests/<name>`
- Examples: `examples/*.cpp` → `bin/examples/<name>`

Core knobs you can override on the command line (defaults shown):
- `LOG_ONFI_LEVEL=0` – Verbosity for protocol-layer logs (0–5)
- `LOG_HAL_LEVEL=0` – Verbosity for hardware abstraction logs (0–5)
- `PROFILE_TIME=0` – Enable timing capture for select operations

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
| `benchmark` | `bin/apps/benchmark` | Measures GPIO toggle rates for a range of busy-wait loop counts. |
| `erase_chip` | `bin/apps/erase_chip` | Iterates through every block and issues a full-chip erase (destructive). |
| `profiler` | `bin/apps/profiler` | Runs representative ONFI operations while streaming timing data when profiling is enabled. |
| `gpio_test` | `bin/apps/gpio_test` | Interactive harness for verifying each GPIO line and observing state changes. |
| `tester` | `bin/tests/tester` | Comprehensive regression covering erase/program/read/verify paths with randomized data. |
| `example_*` | `bin/examples/…` | Minimal programs that demonstrate embedding the ONFI API in other applications. |

Generated artifacts:
- `time_info_file.txt` – Created when binaries are built with `PROFILE_TIME=1`; records per-operation timing data.

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
├── include/                # Public headers (HAL, ONFI API, logging, timing)
├── lib/bcm2835_install/    # Vendored bcm2835 headers and static library
├── src/                    # Core library implementation files
├── tests/                  # Functional test entry points (bin/tests)
└── README.md               # Project overview (this file)
```

## Documentation
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
