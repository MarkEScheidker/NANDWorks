# MastersThesis

This project contains firmware and example code for interfacing ONFI compliant NAND flash devices from a DE1-SoC development board.  All source code lives under `src/` with public headers in `include/`.  Detailed documentation generated with Doxygen can be found in `docs/`.

## Building

A simple `Makefile` is provided at the repository root.  Build the example application with:

```bash
make
```

Artifacts are placed in the `build/` directory and the resulting executable is named `main`.

Clean build outputs with:

```bash
make clean
```

See `docs/readme.md` for more extensive usage information.
