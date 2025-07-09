# ONFI NAND Interface



This repository contains firmware and example code for interfacing ONFI-compliant NAND flash devices from a DE1‑SoC development board.  The implementation has been validated with a Micron MT29F256G08CBCBB 3D NAND part (ONFI&nbsp;4.0 compliant).  Although the code targets that device, most features should apply to similar NAND memories.

The software runs under Linux on the DE1‑SoC ARM processor.  It accesses the board peripherals directly through `/dev/mem`, so administrator privileges are required.  The interface abstracts low‑level pin manipulation and exposes convenient APIs for erase, program and read operations.

Comprehensive API documentation generated with Doxygen is included in the `html/` directory.  Additional working examples can be found in `examples`.


## Repository Layout

- `main.cpp` – Minimal demonstration application used to verify connectivity and exercise basic commands.
- `onfi/*.cpp` / `onfi_interface.h` – Implementation of the ONFI protocol, including block erase, page program, page read and various helper routines.
- `microprocessor_interface.cpp` / `microprocessor_interface.h` – Hardware abstraction layer for the DE1‑SoC.  Provides memory‑mapped I/O access, LED helpers and pin configuration utilities.
- `hardware_locations.h` – Pin assignments, timing macros and configuration values for the DE1‑SoC board.
- `examples` – Extensive usage examples that showcase more advanced flows.
- `html/` – Pre‑built Doxygen output describing all classes and functions.
- `Makefile` – Simple build script for compiling the example program.

## Building

The project is built using the provided `Makefile`.  From the repository root directory simply run:

```bash
make
```

This produces an executable named `main`.  The program must be executed as root (or with equivalent permissions) because it accesses `/dev/mem` for hardware control.

Clean up intermediate files with:

```bash
make clean
```

## Basic Usage

Running the program without arguments executes a small battery of
sanity tests that exercise erase, program and read operations.  Pass
`-v` to enable verbose output:

```bash
./main            # run basic tests
./main -v         # run tests with additional logging
```

For more complex scenarios see `examples`.  It illustrates how to:

- Initialize the hardware and the NAND device
- Erase blocks and verify that an erase completed
- Program individual pages or entire blocks
- Perform partial program/erase cycles
- Retrieve device parameters and status information

The helper functions print progress to standard output.  When `DEBUG_ONFI` or `DEBUG_INTERFACE` are enabled, additional logs are written to `onfi_debug_log.txt` and `interface_debug_log.txt` respectively.  If the `PROFILE_TIME` flag in `hardware_locations.h` is set to `true` the code records timing information in `time_info_file.txt`.

## Hardware Setup

The firmware expects the DE1‑SoC board's GPIO pins to be wired to the NAND device as described in `hardware_locations.h`.  Key control and data lines include:

| Signal | GPIO Bit |
| ------ | -------- |
| `DQ[7:0]` | `io0`-`io7` (21, 20, 16, 12, 25, 24, 23, 18) |
| `WP#` | 26 |
| `WE#` | 19 |
| `ALE` | 13 |
| `CLE` | 11 |
| `CE#` | 22 |
| `RE#` | 27 |
| `R/B#` | 17 |

Timing parameters are implemented using simple software delays; refer to `hardware_locations.h` for the exact macros.  The board must expose its lightweight HPS‑to‑FPGA bridge at address `0xFF200000` so that the driver can map it via `/dev/mem`.

## Features

The API offers fine grained control over a wide range of NAND operations:

- Device initialization and reset
- Reading the ONFI parameter page and decoding device geometry
- Full and partial block erase routines
- Page program with optional spare area writes
- TLC specific helpers (e.g. `program_page_tlc_toshiba`)
- SLC/MLC conversion utilities using ONFI set feature commands
- Timing profiling for performance analysis
- Direct page and block read functions capable of dumping data to a file

These primitives can be combined to implement custom experiments or production tools for NAND characterization.

## Documentation

Full API documentation generated with Doxygen is available under `html/index.html`.  Open that file in a browser to explore function descriptions and class diagrams.

Additional background and historical information can be found in the original [Nios NAND Interface documentation](https://sickranchez-c137.github.io/nios_NAND_interface/).  That project targets a bare‑metal environment; this repository adapts the same concepts for Linux.

## Notes

Sample output images referenced in the original document are not included in this repository.  Timing numbers related to performance profiling have been redacted for confidentiality.

