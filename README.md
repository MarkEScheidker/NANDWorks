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
| `CLE` | 11 | Command Latch Enable (Active High).       |
| `ALE` | 13 | Address Latch Enable (Active High).       |
| `RE#` | 27 | Read Enable (Active Low).                 |
| `WE#` | 19 | Write Enable (Active Low).                |
| `CE#` | 22 | Chip Enable (Active Low).                 |
| `R/B#` | 17 | Ready/Busy status (Output from NAND).     |

### FPGA Bridge

The lightweight HPS-to-FPGA bridge is expected to be available at the physical address `0xFF200000`. The software uses this bridge to access the GPIO controllers.

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
