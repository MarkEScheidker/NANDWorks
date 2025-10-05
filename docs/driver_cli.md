# NANDWorks Driver CLI

The `nandworks` binary consolidates every ONFI-facing workflow into a single command-line tool. This page summarises the command surface, how to run it safely, and how it maps to the underlying library functionality.

## Running the driver

```bash
# Show global help and the list of registered commands
bin/nandworks --help

# Show detailed usage for a particular command
bin/nandworks help read-page
```

Most commands interact with `/dev/mem`, so they must run with elevated privileges:

```bash
sudo bin/nandworks probe
```

Potentially destructive actions – any command that can modify flash contents or drive arbitrary bus transactions – are gated behind `--force` (or `-f`). The driver refuses to proceed until the flag is present, even when run as root.

## Command groups

The registry exposes the following functional groups. Every command automatically accepts `--help` / `-h` to display its usage.


### Discovery & diagnostics

| Command | Description | Example |
| --- | --- | --- |
| `probe` | Brings up the interface, parses the ONFI parameter page, and prints manufacturer/model/geometry. | `sudo bin/nandworks probe` |
| `read-id` | Executes the READ-ID/UNIQUE-ID sequences and prints both ASCII and hex representations. | `sudo bin/nandworks read-id` |
| `status` (`--raw`) | Issues `0x70` and reports ready/pass/write-protect bits, optionally the exact bit pattern. | `sudo bin/nandworks status --raw` |
| `scan-bad-blocks` (`--block`) | Checks factory bad-block markers for every block or a specific block. | `sudo bin/nandworks scan-bad-blocks --block 42` |
| `parameters` (`--jedec`, `--bytewise`, `--raw`, `--output`) | Re-reads the ONFI/Jedec parameter page, updates cached geometry, and optionally dumps the raw 256 bytes. | `sudo bin/nandworks parameters --output onfi.bin` |
| `wait-ready` | Blocks until R/B# indicates ready using the HAL loop – useful in scripts that chain raw commands. | `sudo bin/nandworks wait-ready` |

### Read paths

| Command | Description | Example |
| --- | --- | --- |
| `read-page` (`--block`, `--page`, `--include-spare`, `--bytewise`, `--output`) | Captures a single page via READ and writes to stdout or a file. | `sudo bin/nandworks read-page --block 10 --page 4 --include-spare --output page.bin` |
| `read-block` (`--block`, `--pages`, `--include-spare`, `--bytewise`, `--output`) | Streams an entire block or selected pages to a sink (file or hexdump). | `sudo bin/nandworks read-block --block 10 --pages 0-3 --output block10.bin` |
| `raw-read-data` (`--count`) | Reads an arbitrary number of bytes from the data bus into a hex table. | `sudo bin/nandworks raw-read-data --count 32` |
| `raw-change-column` (`--column`) | Sends the CHANGE READ COLUMN sequence to reposition the read pointer. | `sudo bin/nandworks raw-change-column --column 0x1A0` |

### Program & erase (require `--force`)

| Command | Description | Example |
| --- | --- | --- |
| `program-page` (`--block`, `--page`, `--input`, `--include-spare`, `--pad`, `--verify`) | Programs a page from a provided buffer, optionally pads to length and verifies the result. | `sudo bin/nandworks program-page --block 10 --page 4 --input payload.bin --verify --force` |
| `program-block` (`--block`, `--pages`, `--input`, `--include-spare`, `--pad`, `--verify`, `--random`) | Programs a block or list of pages with a static buffer or random data. | `sudo bin/nandworks program-block --block 10 --pages 0-3 --random --force` |
| `erase-block` (`--block`) | Erases a single block and waits for completion. | `sudo bin/nandworks erase-block --block 10 --force` |
| `erase-chip` (`--start`, `--count`) | Performs sequential block erases across the device (dangerous). | `sudo bin/nandworks erase-chip --start 0 --count 2048 --force` |
| `set-feature` (`--address`, `--data`) | Issues SET FEATURES with four byte payload. | `sudo bin/nandworks set-feature --address 0x01 --data 0x04,0x00,0x00,0x00 --force` |
| `raw-command` (`--value`) | Sends an arbitrary command byte. | `sudo bin/nandworks raw-command --value 0x90 --force` |
| `raw-address` (`--bytes`) | Sends one or more address cycles. | `sudo bin/nandworks raw-address --bytes 0x00,0x00,0x00 --force` |
| `raw-send-data` (`--bytes`) | Drives data bytes onto the bus. | `sudo bin/nandworks raw-send-data --bytes 0xAA,0x55 --force` |

### Timing metrics

| Command | Description | Example |
| --- | --- | --- |
| `measure-erase` (`--block`, `--force`) | Issues a block erase and reports the busy interval observed on R/B#. | `sudo bin/nandworks measure-erase --block 10 --force` |
| `measure-program` (`--block`, `--page`, `--include-spare`, `--input`, `--pad`, `--force`) | Programs a page (default 0xFF fill) and reports how long the device remained busy. | `sudo bin/nandworks measure-program --block 10 --page 4 --force` |
| `measure-read` (`--block`, `--page`, `--include-spare`, `--output <file>`) | Reads a page, printing it to stdout unless an output path is provided, and reports the array-to-cache transfer time. | `sudo bin/nandworks measure-read --block 10 --page 4 --output read.bin` |

### Verification & maintenance

| Command | Description | Example |
| --- | --- | --- |
| `verify-page` (`--block`, `--page`, `--include-spare`, `--input`) | Reads back a page and compares it against optional reference data, printing byte/bit error counts. | `sudo bin/nandworks verify-page --block 10 --page 4 --input payload.bin` |
| `verify-block` (`--block`, `--pages`, `--include-spare`, `--input`) | Verifies an entire block or subset of pages. | `sudo bin/nandworks verify-block --block 10 --pages 0-3 --input payload.bin` |
| `reset-device` | Issues the ONFI reset command and waits for ready. | `sudo bin/nandworks reset-device` |
| `device-init` | Runs the power-on initialisation helper (`device_initialization`). | `sudo bin/nandworks device-init` |
| `wait-ready` | Exposes the HAL wait loop as a first-class command. | `sudo bin/nandworks wait-ready` |


## Patterns & tips

- **Root guard** – The driver checks `geteuid()` before commanding the hardware. Unprivileged invocations print an explanatory error and exit with status 5.
- **Force guard** – Destructive commands throw an argument error when `--force` is omitted. This keeps exploratory work safe, especially when running scripts.
- **Uniform parsing** – Options accept both long (`--block`) and short (`-b`) forms. Values can be specified inline (`--value=0x90`) or as separate tokens. Lists (`--pages 0,4,9-12`) accept comma and dash notation.
- **Help everywhere** – Use `--help` or `-h` after any command to print its usage, option descriptions, and the force requirement if applicable.
- **Legacy tools** – The original apps (`bin/apps/*`) are still built for compatibility, but they reuse the same underlying library. New automation should favour the CLI so behaviour stays consistent and scriptable.

## Troubleshooting

| Message | Likely cause | Next steps |
| --- | --- | --- |
| `Command '<name>' requires root privileges.` | Command interacts with `/dev/mem` but was run without `sudo`. | Re-run via `sudo` or adjust permissions to grant the user raw memory access. |
| `Command requires --force to proceed` | Destructive command run without confirmation. | Re-run with `--force` once you are certain the operation is intended. |
| `Unknown option` / `Missing required option` | CLI argument typo. | Check `bin/nandworks help <command>` for the accepted flags and expected formats. |
| `GPIO initialisation failed` | Most often due to missing root privileges, but can also indicate the GPIO block is already in use. | Ensure `sudo` is used, stop conflicting software, and verify wiring. |

## Extending the CLI

To add a new command:
1. Declare the metadata in `src/driver/commands/onfi_commands.cpp` (or a new module) using `Command` initialisers.
2. Implement the handler with the shared `CommandContext`, reusing helpers such as `DriverContext::require_onfi_started()` and the parsing utilities in `command_arguments.hpp`.
3. Gate destructive operations behind `CommandSafety::RequiresForce` so they automatically respect the `--force` policy.
4. Add parser/unit coverage under `tests/` to lock in the new behaviour.

With these patterns, the CLI remains the single, authoritative interface for both manual operation and scripted automation.
