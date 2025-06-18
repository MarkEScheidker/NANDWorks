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

Running `./main` without arguments performs a small set of sanity tests. Pass `-v` for verbose output. Extensive usage examples can be found in `src/help.cpp`.

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

The main user-facing class is `onfi_interface` defined in `include/onfi_head.h`. Key methods include:

- `get_started()`
- `initialize_onfi()` / `deinitialize_onfi()`
- `test_onfi_leds()`
- `open_onfi_debug_file()`
- `open_onfi_data_file()`
- `get_data()`
- `check_status()` and `wait_on_status()`
- `device_initialization()` and `reset_device()`
- `read_parameters()` and `read_id()`
- `decode_ONFI_version()`
- `set_page_size()` / `set_page_size_spare()`
- `set_block_size()` / `set_lun_size()`
- `is_bad_block()`
- `read_page()`
- `open_time_profile_file()`
- `change_read_column()`
- `erase_block()` / `partial_erase_block()`
- `read_status()`
- `verify_block_erase_sample()` / `verify_block_erase()`
- `disable_erase()` / `enable_erase()`
- `program_page()` / `partial_program_page()`
- `program_page_tlc_toshiba()` / `program_page_tlc_toshiba_subpage()`
- `verify_program_page()`
- `program_pages_in_a_block()` / `program_pages_in_a_block_data()`
- `program_n_pages_in_a_block()` / `program_pages_in_a_block_slc()`
- `partial_program_pages_in_a_block()`
- `verify_program_pages_in_a_block()` / `verify_program_pages_in_a_block_slc()`
- `read_and_spit_page()` / `read_and_spit_page_tlc_toshiba()`
- `read_block_data()` / `read_block_data_n_pages()`
- `read_page_and_return_value()`
- `set_features()` / `get_features()`
- `convert_to_slc_set_features()` / `revert_to_mlc_set_features()`
- `convert_to_slc()` / `revert_to_mlc()`
- `delay_function()` / `profile_time()`
- `convert_pagenumber_to_columnrow_address()`

The `interface` base class in `include/microprocessor_interface.h` exposes lower level helpers:

- `open_physical()` / `close_physical()`
- `map_physical()` / `unmap_physical()` / `get_base_bridge()`
- `convert_peripheral_address()`
- `open_interface_debug_file()` / `close_interface_debug_file()`
- `turn_leds_on()` / `turn_leds_off()` / `test_leds_increment()`
- `set_ce_low()` / `set_default_pin_values()`
- `set_datalines_direction_input()` / `set_datalines_direction_default()`
- `set_pin_direction_inactive()`
- `send_command()` / `send_addresses()` / `send_data()`

Refer to the Doxygen HTML for complete parameter descriptions.

## Documentation

Open `docs/html/index.html` in a browser to read the full API reference.