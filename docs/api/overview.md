# ONFI NAND Interface API

This reference concentrates on the `onfi_interface` façade and the supporting
classes that power the NAND flash tooling in this repository.  Use the
navigation pane to explore the high-level operations (erase, program, verify)
and the lower-level helpers that back them.

## Quick Start

1. Call `onfi_interface::get_started()` to power-up the hardware abstraction
   layer and parse the ONFI parameter page.
2. Inspect geometry fields such as `num_bytes_in_page` or
   `num_pages_in_block` to size your buffers.
3. Use the program/read/verify helpers to drive higher-level workflows or to
   compose your own NAND test routines.

## Interface Hierarchy

- `onfi_interface` (main entry point)
  - derives from `interface` (hardware abstraction for the GPIO bridge)
  - collaborates with `onfi::OnfiController` and `onfi::NandDevice` to execute
    protocol sequences

Each class page links to collaboration diagrams showing how data flows from
application code, through the controller helpers, and down to the HAL.

## Additional Resources

- See the `docs/` directory for hardware setup details.
- Example command-line tools live under `apps/` and `examples/` and rely on the
  same API documented here.

