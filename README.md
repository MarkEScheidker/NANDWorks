# ONFI NAND Flash Interface for Raspberry Pi

This project provides a comprehensive C++ library and a suite of command-line tools for interfacing with ONFI-compliant NAND flash memory on a Raspberry Pi. The software is designed to run on the board's ARM processor under Linux, using memory-mapped I/O for high-performance, low-level control of the GPIO pins connected to the NAND flash.

This implementation has been validated with a Micron 3D NAND part (ONFI 4.0 compliant), but its modular design makes it adaptable for other similar NAND devices.

## Table of Contents

- [Features](#features)
- [Hardware Setup](#hardware-setup)
  - [Pin Connections](#pin-connections)
  - [FPGA Bridge](#fpga-bridge)
- [Software Architecture](#software-architecture)
  - [Hardware Abstraction Layer (HAL)](#hardware-abstraction-layer-hal)
  - [ONFI Protocol Layer](#onfi-protocol-layer)
- [Building the Project](#building-the-project)
- [Available Tools & Usage](#available-tools--usage)
  - [`main`](#main)
  - [`erase_chip`](#erase_chip)
  - [`benchmark`](#benchmark)
  - [`profiler`](#profiler)
- [Public API Overview](#public-api-overview)
  - [Initialization and Setup](#initialization-and-setup)
  - [Device Information](#device-information)
  - [Core Operations](#core-operations)
  - [Verification](#verification)
  - [Utilities](#utilities)
- [Repository Structure](#repository-structure)
 - [Debugging & Logging](#debugging--logging)
 - [Build Options](#build-options)

## Features

*   **Low-Level NAND Control:** Direct interaction with the NAND flash for fundamental operations like Reset, Read ID, and Read Parameter Page.
*   **Core Flash Operations:** Robust and well-tested functions for page programming, page reading, and block erasing.
*   **Advanced Functionality:** Support for partial page programming/erasing, bad block detection, and SLC/MLC mode conversion.
*   **Performance Utilities:** Includes tools to benchmark GPIO toggle speeds and profile the timing of various NAND operations.
*   **Hardware Abstraction:** A dedicated hardware abstraction layer (HAL) for the DE1-SoC simplifies the interaction with GPIO pins, making the code more portable.
*   **Detailed Documentation:** Doxygen-generated API documentation is available in the `docs/html` directory for easy browsing.

## Hardware Setup

The code is specifically designed for the Raspberry Pi. The following sections detail the required hardware connections.

### Pin Connections

The following table outlines the expected GPIO connections between the DE1-SoC and the NAND flash device. For a definitive reference, please consult `include/hardware_locations.h`.

| Signal | GPIO Pin (BCM) | Description                               |
| :--- | :--- | :---------------------------------------- |
| `DQ0-DQ7` | 21, 20, 16, 12, 25, 24, 23, 18 | 8-bit bidirectional data bus.             |
| `WP#` | 26 | Write Protect (Active Low).               |
| `WE#` | 19 | Write Enable (Active Low).                |
| `ALE` | 13 | Address Latch Enable (Active High).       |
| `CLE` | 11 | Command Latch Enable (Active High).       |
| `CE#` | 22 | Chip Enable (Active Low).                 |
| `RE#` | 27 | Read Enable (Active Low).                 |
| `R/B#` | 17 | Ready/Busy status (Output from NAND).     |

## Software Architecture

The project is divided into two main layers: the Hardware Abstraction Layer (HAL) and the ONFI Protocol Layer.

### Hardware Abstraction Layer (HAL)

The HAL, primarily defined in `include/microprocessor_interface.h` and implemented in `src/microprocessor_interface.cpp`, is responsible for all direct interaction with the hardware. Its key responsibilities include:

*   **GPIO Control:** Setting pin directions (input/output), and driving pins high or low.
*   **Memory Mapping:** Accessing the physical memory of the DE1-SoC to control GPIO pins.
*   **Low-Level Data Transfer:** Providing primitive functions like `send_command`, `send_addresses`, and `send_data` that handle the raw signaling required to communicate with the NAND flash.

### ONFI Protocol Layer

Built on top of the HAL, the ONFI Protocol Layer, defined in `include/onfi_interface.h` and implemented in `src/onfi/`, implements the logic of the ONFI specification. This layer provides a user-friendly API for performing high-level NAND operations. Key functionalities include:

*   **Device Initialization:** Handling the reset sequence and reading the ONFI parameter page to configure the driver.
*   **Command Abstraction:** Translating high-level requests (e.g., "erase block 5") into the corresponding sequence of low-level ONFI commands.
*   **Status Management:** Checking the device's status to ensure operations complete successfully.

## Building the Project

To build all the executables, navigate to the project's root directory and run the `make` command.

```bash
make
```

This will compile the source code and place the executables (`main`, `erase_chip`, `benchmark`, `profiler`) in the root directory.

To clean the build artifacts, run:
```bash
make clean
```

## Available Tools & Usage

The project builds four main executables. **Note:** All tools require root privileges to access `/dev/mem` for direct hardware control.

### `main`

A general-purpose testing application that performs a basic set of sanity checks, including initialization, erase, program, and read operations. It's a great starting point to verify that your hardware is set up correctly and the library is functioning.

```bash
sudo ./main [-v]
```
*   `-v`: Enable verbose output for more detailed logging.

### `erase_chip`

A utility to perform a full erase of the connected NAND flash chip. **Use with extreme caution, as this will permanently delete all data on the device.**

```bash
sudo ./erase_chip
```

### `benchmark`

A tool to measure the raw GPIO toggle frequency. This is useful for assessing the performance of the underlying hardware interface and ensuring that the timing requirements of the NAND flash can be met.

```bash
sudo ./benchmark
```

### `profiler`

A utility to measure and profile the execution time of various NAND flash commands (e.g., program, erase, read). The results, including mean, median, and standard deviation, are printed to the console, providing valuable insights into the performance characteristics of the NAND device.

```bash
sudo ./profiler
```

## Public API Overview

The primary entry point for using this library is the `onfi_interface` class. The following is a high-level overview of its most important functions. For complete details, please refer to the Doxygen documentation.

### Initialization and Setup

*   `get_started()`: Initializes the entire ONFI interface, including the HAL and the NAND device itself.
*   `device_initialization()`: Performs the initial power-on reset and handshake with the NAND flash.
*   `reset_device()`: Issues a software reset to the NAND device.

### Device Information

*   `read_parameters()`: Reads the ONFI parameter page, which contains critical information about the device's geometry and capabilities.
*   `read_id()`: Reads the manufacturer and device ID.
*   `is_bad_block()`: Checks if a given block is marked as a bad block from the factory.

### Core Operations

*   `erase_block()`: Erases an entire block.
*   `program_page()`: Programs a single page with the provided data.
*   `read_page()`: Reads a single page into a buffer.

### Verification

*   `verify_block_erase()`: Verifies that a block has been successfully erased (i.e., all bits are 1).
*   `verify_program_page()`: Verifies that a page has been programmed correctly by comparing its contents with the original data.

### Utilities

*   `set_features()` / `get_features()`: Low-level functions for setting and getting ONFI features.
*   `convert_to_slc()` / `revert_to_mlc()`: Functions to switch a block between SLC and MLC modes.

## Repository Structure

```
.
├── Makefile              # Build script
├── README.md             # This file
├── build/                # Build artifacts (created by make)
├── docs/                 # Doxygen documentation
├── examples/             # Example code snippets
├── include/              # Header files for the library
│   ├── onfi_interface.h
│   ├── microprocessor_interface.h
│   └── hardware_locations.h
├── lib/                  # Included libraries (bcm2835)
└── src/                  # Source code for the library and tools
    ├── main.cpp
    ├── erase_chip.cpp
    ├── benchmark.cpp
    ├── profiler.cpp
    └── onfi/
        ├── erase.cpp
        ├── identify.cpp
        ├── init.cpp
        ├── program.cpp
        ├── read.cpp
        └── util.cpp
```

## Debugging & Logging

A lightweight, header‑only logging system provides structured logs with zero overhead when disabled. Logs include timestamps, levels, and a component tag (ONFI/HAL).

- Components: `onfi` (protocol API), `hal` (low‑level GPIO operations).
- Levels: `0=NONE`, `1=ERROR`, `2=WARN`, `3=INFO`, `4=DEBUG`, `5=TRACE`.
- Example macros:
  - `LOG_ONFI_INFO("Erased block %u", block)`
  - `LOG_ONFI_INFO_IF(verbose, "Erased block %u", block)`
  - `LOG_HAL_DEBUG("Sending %u address bytes", n)`

Enable logs at compile time by defining per‑component levels:

```bash
# ONFI at INFO, HAL at DEBUG
make CXXFLAGS+=" -DLOG_ONFI_LEVEL=3 -DLOG_HAL_LEVEL=4 "

# Very verbose
make CXXFLAGS+=" -DLOG_ONFI_LEVEL=5 -DLOG_HAL_LEVEL=5 "
```

Most tools also accept `-v` to show user‑oriented messages. Internally, log calls use `*_IF(verbose, ...)` forms so the runtime condition is inside the macro and compiles away entirely when logging is disabled.

Timing/profiling I/O is disabled by default to keep hot paths clean. To record per‑operation timings to `time_info_file.txt`:

```bash
make CXXFLAGS+=" -DPROFILE_TIME=1 "
```

## Build Options

Suggested configurations:

- Development diagnostics:
  - `make CXXFLAGS+=" -g -O2 -DLOG_ONFI_LEVEL=4 -DLOG_HAL_LEVEL=4 -DPROFILE_TIME=1 "`

- Maximum performance (no logs, no profiling):
  - `make CXXFLAGS+=" -O3 "`

- High‑level ONFI logs only:
  - `make CXXFLAGS+=" -O3 -DLOG_ONFI_LEVEL=3 "`

Run tools with `-v` to enable verbose output where supported:

```bash
sudo ./main -v
sudo ./erase_chip -v
```
Convenience Makefile targets (shortcuts):

- `make debug`: Enables ONFI=INFO and HAL=DEBUG logs, turns on profiling I/O, and adds `-g -O2` for easier debugging.
- `make trace`: Enables ONFI/HAL at TRACE level (very verbose), turns on profiling I/O, and adds `-g -O2`.
- `make profile`: Turns on profiling I/O only; logs remain at defaults.
- `make help`: Prints a brief summary of available variables and targets.

Make‑time variables (can be overridden on the command line):

- `LOG_ONFI_LEVEL`: 0..5 (default 0)
- `LOG_HAL_LEVEL`: 0..5 (default 0)
- `PROFILE_TIME`: 0 or 1 (default 0)

Examples:

```bash
# ONFI at INFO, HAL at DEBUG
make LOG_ONFI_LEVEL=3 LOG_HAL_LEVEL=4

# Enable profiling I/O only
make PROFILE_TIME=1

# Trace everything (be verbose!)
make LOG_ONFI_LEVEL=5 LOG_HAL_LEVEL=5 PROFILE_TIME=1
```

