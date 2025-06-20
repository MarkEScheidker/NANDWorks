# ONFI NAND Interface

This repository provides firmware and example code for interfacing ONFI-compliant NAND flash devices from an Altera DE1-SoC development board. The software runs under Linux on the ARM core and accesses board peripherals through `/dev/mem`. All source code resides in `src/` with public headers in `include/`. Pre-generated API documentation is available in `docs/html`.

## Building

Use the provided `Makefile` in the repository root:

```bash
make           # build into ./build and produce ./main
make clean     # remove build artifacts
```

The resulting `main` executable must be run with root privileges.

## Repository Layout

- `src/` – C++ sources for the driver and examples
- `include/` – public headers
- `docs/html` – Doxygen documentation
- `Makefile` – build script

## Basic Usage

Running `./main` without arguments performs a small set of sanity tests. Pass `-v` for verbose output. Extensive usage examples can be found in `examples/`.

## Hardware Setup

GPIO assignments and timing parameters are defined in `include/hardware_locations.h`. The DE1‑SoC GPIO pins must be wired to the NAND device as specified there. Timing is implemented with simple software delays.

## Features

The code supports a wide range of NAND operations:

- Device initialization and reset
- Reading the ONFI parameter page
- Full and partial block erase routines
- Page program with optional spare area writes
- TLC helpers (e.g. Toshiba sub‑page programming)
- Converting blocks between SLC and MLC
- Timing profiling utilities
- Direct page and block read functions

## Public API

The main user-facing class is `onfi_interface` defined in `include/onfi_interface.h`.  The
lists below group its most common functions by purpose and provide brief
descriptions.

### Setup & Initialization
- `get_started()` – prepare the GPIO interface and internal state.
- `initialize_onfi()` / `deinitialize_onfi()` – allocate or release resources.
- `open_onfi_debug_file()` / `open_onfi_data_file()` – create log files for debug output.
- `open_time_profile_file()` – enable timing logs.
- `device_initialization()` / `reset_device()` – perform ONFI resets and handshakes.
- `test_onfi_leds()` – flash LEDs to verify the connection.

### Status and Configuration
- `read_parameters()` / `read_id()` – query device geometry and identification.
- `decode_ONFI_version()` – parse the ONFI version field.
- `set_page_size()` / `set_page_size_spare()` – configure page and spare area sizes.
- `set_block_size()` / `set_lun_size()` – adjust geometry settings.
- `check_status()` / `wait_on_status()` – poll the chip status register.
- `read_status()` – read the current state directly.

### Block Operations
- `erase_block()` / `partial_erase_block()` – erase pages within a block.
- `verify_block_erase()` / `verify_block_erase_sample()` – confirm an erase completed.
- `is_bad_block()` – check if a block is marked bad.
- `disable_erase()` / `enable_erase()` – gate erase functionality.

### Page Program and Read
- `program_page()` / `partial_program_page()` – write data to a page.
- `program_page_tlc_toshiba()` / `program_page_tlc_toshiba_subpage()` – helpers for Toshiba TLC devices.
- `verify_program_page()` – verify that a page was written correctly.
- `read_page()` – read a page into a buffer.
- `read_and_spit_page()` / `read_and_spit_page_tlc_toshiba()` – dump a page to stdout.
- `read_block_data()` / `read_block_data_n_pages()` – sequential page reads.
- `read_page_and_return_value()` – fetch a page and return it as a string.
- `program_pages_in_a_block()` / `program_pages_in_a_block_data()` – write an entire block.
- `program_n_pages_in_a_block()` / `program_pages_in_a_block_slc()` – program a subset of pages.
- `partial_program_pages_in_a_block()` – partial writes within a block.
- `verify_program_pages_in_a_block()` / `verify_program_pages_in_a_block_slc()` – bulk verification.
- `change_read_column()` – shift the read pointer before issuing a read command.
- `convert_to_slc()` / `revert_to_mlc()` – switch cell mode using standard commands.
- `convert_to_slc_set_features()` / `revert_to_mlc_set_features()` – alternative SLC/MLC toggles.

### Utilities
- `get_data()` – convert a buffer into a printable string.
- `set_features()` / `get_features()` – issue ONFI feature commands.
- `delay_function()` / `profile_time()` – simple timing helpers.
- `convert_pagenumber_to_columnrow_address()` – translate a page number to an address tuple.

### Hardware Interface Primitives
The `interface` base class in `include/microprocessor_interface.h` provides the
low-level helpers used by `onfi_interface`:

- `open_physical()` / `close_physical()` – handle `/dev/mem` access.
- `map_physical()` / `unmap_physical()` / `get_base_bridge()` – map the HPS–FPGA bridge.
- `convert_peripheral_address()` – compute a virtual address for a peripheral.
- `open_interface_debug_file()` / `close_interface_debug_file()` – log hardware transactions.
- `turn_leds_on()` / `turn_leds_off()` / `test_leds_increment()` – simple LED helpers.
- `set_ce_low()` / `set_default_pin_values()` – manage the chip enable and default pin states.
- `set_datalines_direction_input()` / `set_datalines_direction_default()` – switch GPIO direction.
- `set_pin_direction_inactive()` – tri-state the bus.
- `send_command()` / `send_addresses()` / `send_data()` – raw NAND transactions.

Refer to the Doxygen HTML for complete parameter descriptions.

## Documentation

Open `docs/html/index.html` in a browser to read the full API reference.
