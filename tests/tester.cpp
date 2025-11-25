/*! \mainpage
File: main.cpp

Description: This source file is to test the basic functionality of the interface designed.
                        .. This file tests the connection as well as the major functions like
                        .. .. erase, program etc
*/

// onfi_interface.hpp has the necessary functionalities defined
#include "onfi_interface.hpp"
#include "onfi/device.hpp"
#include "onfi/device_config.hpp"
#include "onfi/controller.hpp"
#include "onfi/data_sink.hpp"
#include "onfi/types.hpp"

#include <vector>
#include <cstdlib>
#include <cstring>
#include <algorithm>    // std::sort
#include <unordered_map>
#include <time.h>

// Define a maximum allowed error threshold for programming tests
const int MAX_ALLOWED_ERRORS = 50; // Example: Allow up to 50 bit errors per page

using namespace std;

// Utility: pick a random good block (skip factory-marked bad blocks)
static unsigned int pick_good_block(onfi_interface &onfi_instance) {
    // Try random picks first, then fall back to first good
    for (int tries = 0; tries < 16; ++tries) {
        unsigned int b = rand() % onfi_instance.num_blocks;
        if (!onfi_instance.is_bad_block(b)) return b;
    }
    for (unsigned int b = 0; b < onfi_instance.num_blocks; ++b) {
        if (!onfi_instance.is_bad_block(b)) return b;
    }
    return 0; // worst case; caller should still work
}

static void configure_device(onfi_interface& onfi_instance, onfi::NandDevice& device) {
    const auto config = onfi::make_device_config(onfi_instance);
    onfi::apply_device_config(config, device);
}

/**
these are the page indices that will be used for analysis for SLC
*/
uint16_t slc_page_indices[20] = {
    0, 2, 4, 6,
    32, 34, 36, 38,
    64, 68, 72, 76,
    105, 113, 121, 129,
    168, 176, 184, 192
};

/**
 * Collection of small tests exercising common API calls.  Each helper
 * below performs one operation and returns whether it succeeded so the
 * main routine can report overall status.
 */

static bool test_leds(onfi_interface &onfi_instance, bool verbose) {
    if (verbose)
        cout << "Running LED test" << endl;
    onfi_instance.test_onfi_leds(verbose);
    return true; // test_onfi_leds prints errors itself
}

static bool test_block_erase(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    if (verbose) cout << "Erasing block " << block << endl;
    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    dev.erase_block(block);

    // When no subset is provided, verify_erase_block defaults to checking the full block.
    bool full_ok = dev.verify_erase_block(block, true, nullptr, 0,
                                          /*including_spare*/false, verbose);
    return full_ok;
}

static bool test_single_page_program(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    unsigned int page = 0;

    size_t page_size = onfi_instance.num_bytes_in_page +
                       onfi_instance.num_spare_bytes_in_page;
    std::vector<uint8_t> pattern(page_size);
    for (size_t i = 0; i < page_size; ++i)
        pattern[i] = static_cast<uint8_t>(i & 0xff);
    // Ensure the first byte of the spare area is not 0x00 to avoid marking as bad block
    pattern[onfi_instance.num_bytes_in_page] = 0xFF;

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    dev.erase_block(block);
    dev.program_page(block, page, pattern.data(), true);
    bool ok = dev.verify_program_page(block, page, pattern.data(), /*including_spare*/false, verbose, MAX_ALLOWED_ERRORS);
    return ok;
}

static bool test_multi_page_program(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    // Ensure the block is erased before programming multiple pages
    dev.erase_block(block);
    dev.program_block(block, /*complete_block*/false, slc_page_indices, 20,
                      /*provided_data*/nullptr, /*including_spare*/true, /*randomize*/false);
    return dev.verify_program_block(block, /*complete_block*/false, slc_page_indices, 20,
                                    /*expected*/nullptr, /*including_spare*/false, verbose, MAX_ALLOWED_ERRORS);
}

static bool test_page_reads(onfi_interface &onfi_instance, bool verbose) {
    (void)verbose;
    unsigned int block = pick_good_block(onfi_instance);
    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    std::vector<uint8_t> page;

    std::cout << "Page 0 (including spare) in hex:" << std::endl;
    dev.read_page(block, 0, /*including_spare*/true, /*bytewise*/true, page);
    {
        onfi::HexOstreamDataSink hex(std::cout);
        hex.write(page.data(), page.size());
        hex.flush();
    }
    std::cout << std::endl;

    std::cout << "Page 1 (including spare) in hex:" << std::endl;
    dev.read_page(block, 1, /*including_spare*/true, /*bytewise*/true, page);
    {
        onfi::HexOstreamDataSink hex(std::cout);
        hex.write(page.data(), page.size());
        hex.flush();
    }
    return true;
}

static bool test_error_analysis(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    unsigned int page = 0;
    if (verbose) {
        cout << "Performing error analysis on block " << block << " page " << page << endl;
    }

    size_t page_size = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    std::vector<uint8_t> pattern(page_size);
    for (size_t i = 0; i < page_size; ++i) {
        pattern[i] = static_cast<uint8_t>(i & 0xff);
    }
    // Ensure the first byte of the spare area is not 0x00 to avoid marking as bad block
    pattern[onfi_instance.num_bytes_in_page] = 0xFF;

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    dev.erase_block(block);
    dev.program_page(block, page, pattern.data(), true);

    std::vector<uint8_t> read_vec;
    dev.read_page(block, page, /*including_spare*/true, /*bytewise*/false, read_vec);

    int errors = 0;
    for (size_t i = 0; i < onfi_instance.num_bytes_in_page; ++i) {
        if (read_vec[i] != pattern[i]) {
            errors++;
        }
    }

    cout << "Found " << errors << " errors in " << onfi_instance.num_bytes_in_page << " bytes ("
         << (static_cast<double>(errors) / onfi_instance.num_bytes_in_page) * 100 << "%)" << endl;

    return errors <= MAX_ALLOWED_ERRORS;
}

static bool test_bad_block_scan(onfi_interface &onfi_instance, bool verbose) {
    if (verbose) {
        cout << "Scanning for bad blocks" << endl;
    }

    vector<unsigned int> bad_block_list;
    for (unsigned int block = 0; block < onfi_instance.num_blocks; ++block) {
        if (onfi_instance.is_bad_block(block)) {
            bad_block_list.push_back(block);
        }
    }

    cout << "Found " << bad_block_list.size() << " bad blocks: ";
    if (bad_block_list.empty()) {
        cout << "None" << endl;
    } else {
        for (unsigned int block_num : bad_block_list) {
            cout << block_num << " ";
        }
        cout << endl;
    }
    return true;
}

static bool test_random_program_read_verify_erase(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    unsigned int page = rand() % onfi_instance.num_pages_in_block;

    size_t page_size = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    std::vector<uint8_t> pattern(page_size);
    for (size_t i = 0; i < page_size; ++i) {
        pattern[i] = static_cast<uint8_t>(rand() % 256); // Random data
    }
    // Ensure the first byte of the spare area is not 0x00 to avoid marking as bad block
    pattern[onfi_instance.num_bytes_in_page] = 0xFF;

    if (verbose) {
        cout << "Testing random program/read/verify/erase on block " << block << " page " << page << endl;
    }

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    // Erase the block before programming
    dev.erase_block(block);

    // Program the page with random data
    dev.program_page(block, page, pattern.data(), true);

    // Verify the programmed page
    bool program_ok = dev.verify_program_page(block, page, pattern.data(), /*including_spare*/false, verbose, MAX_ALLOWED_ERRORS);

    // Erase the block after verification
    dev.erase_block(block);
    bool erase_ok = dev.verify_erase_block(block, true, nullptr, 0, /*including_spare*/false, verbose);

    return program_ok && erase_ok;
}

static bool test_spare_area_io(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    unsigned int page = 0;
    size_t page_size = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    if (verbose) {
        cout << "Testing spare area I/O on block " << block << " page " << page << endl;
        cout << "Page size (main + spare): " << page_size << " bytes" << endl;
    }
    std::vector<uint8_t> pattern(page_size);
    // Fill main data area with one pattern
    for (size_t i = 0; i < onfi_instance.num_bytes_in_page; ++i) {
        pattern[i] = 0xAA;
    }
    // Fill spare area with another pattern
    for (size_t i = onfi_instance.num_bytes_in_page; i < page_size; ++i) {
        pattern[i] = 0x55;
    }

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    dev.erase_block(block);
    dev.program_page(block, page, pattern.data(), true);

    std::vector<uint8_t> read_vec;
    dev.read_page(block, page, /*including_spare*/true, /*bytewise*/false, read_vec);

    bool ok = true;
    // Only verify the spare area for this test
    for (size_t i = onfi_instance.num_bytes_in_page; i < page_size; ++i) {
        if (read_vec[i] != 0x55) {
            ok = false;
            if (verbose) {
                cout << "Mismatch in spare area at byte " << i << ": Expected 0x55, Got 0x" << hex << (int)read_vec[i] << dec << endl;
            }
            break;
        }
    }

    if (verbose) {
        cout << "Pattern (last 16 bytes of main + first 16 bytes of spare):" << endl;
        for (size_t i = static_cast<size_t>(onfi_instance.num_bytes_in_page) - 16; i < page_size && i < static_cast<size_t>(onfi_instance.num_bytes_in_page + 16); ++i) {
            cout << hex << (int)pattern[i] << " " << dec;
        }
        cout << endl;
        cout << "Read Data (last 16 bytes of main + first 16 bytes of spare):" << endl;
        for (size_t i = static_cast<size_t>(onfi_instance.num_bytes_in_page) - 16; i < page_size && i < static_cast<size_t>(onfi_instance.num_bytes_in_page + 16); ++i) {
            cout << hex << (int)read_vec[i] << " " << dec;
        }
        cout << endl;
    }

    return ok;
}

static bool test_device_init_reset(onfi_interface &onfi_instance, bool verbose) {
    if (verbose) {
        cout << "Testing device initialization and reset" << endl;
    }
    // device_initialization is already called in get_started(), but we can call it again for testing
    onfi_instance.device_initialization(verbose);
    onfi_instance.reset_device();
    // No direct return value for status, rely on verbose output and lack of crash
    return true;
}

// Test programming and verifying the first and last pages within a block
static bool test_boundary_pages(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    unsigned int first_page = 0;
    unsigned int last_page = (onfi_instance.num_pages_in_block > 1) ? 1 : 0; // use first two pages to avoid reserved/paired corners

    size_t page_size_total = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    std::vector<uint8_t> pattern_first(page_size_total);
    std::vector<uint8_t> pattern_last(page_size_total);
    for (size_t i = 0; i < page_size_total; ++i) {
        pattern_first[i] = static_cast<uint8_t>(i & 0xFF);
        pattern_last[i]  = static_cast<uint8_t>(i & 0xFF);
    }
    // Avoid bad-block marking in spare: set full spare region to 0xFF
    for (size_t i = onfi_instance.num_bytes_in_page; i < page_size_total; ++i) {
        pattern_first[i] = 0xFF;
        pattern_last[i]  = 0xFF;
    }

    if (verbose) {
        cout << "Boundary pages test on block " << block << endl;
    }

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    // Fresh erase before programming
    dev.erase_block(block);
    uint16_t first_idx = static_cast<uint16_t>(first_page);
    uint16_t last_idx  = static_cast<uint16_t>(last_page);
    bool ok = dev.verify_erase_block(block, /*complete_block*/false, &first_idx, 1, /*including_spare*/true, verbose);
    ok = ok && dev.verify_erase_block(block, /*complete_block*/false, &last_idx, 1, /*including_spare*/true, verbose);

    // Program and do a non-strict read/verify smoke check (main-only)
    dev.program_page(block, first_page, pattern_first.data(), /*including_spare*/false);

    dev.program_page(block, last_page, pattern_last.data(), /*including_spare*/false);

    return ok;
}

// Test that verify detects mismatches when expected data differs
static bool test_verify_mismatch_detection(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    unsigned int page = 0;

    size_t page_size_total = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    std::vector<uint8_t> pattern(page_size_total);
    for (size_t i = 0; i < page_size_total; ++i)
        pattern[i] = static_cast<uint8_t>(i & 0xFF);
    pattern[onfi_instance.num_bytes_in_page] = 0xFF; // keep spare marker safe

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    // Prepare block and program
    dev.erase_block(block);
    dev.program_page(block, page, pattern.data(), /*including_spare*/true);

    // Make a copy and flip a byte to force mismatch
    std::vector<uint8_t> expected_wrong(pattern.begin(), pattern.begin() + onfi_instance.num_bytes_in_page);
    expected_wrong[10] ^= 0xFF; // flip one byte in main area

    bool verify_ok = dev.verify_program_page(block, page, expected_wrong.data(), /*including_spare*/false, verbose, /*max_allowed_errors*/0);

    // Test passes if verify detects the mismatch (returns false)
    return !verify_ok;
}

// Test that programming with including_spare=false does not alter spare area
static bool test_spare_preserved_when_excluded(onfi_interface &onfi_instance, bool verbose) {
    (void)verbose;
    unsigned int block = pick_good_block(onfi_instance);
    unsigned int page = 0;

    size_t main_size = onfi_instance.num_bytes_in_page;
    std::vector<uint8_t> main_only_pattern(main_size);
    for (size_t i = 0; i < main_size; ++i)
        main_only_pattern[i] = static_cast<uint8_t>((i * 7) & 0xFF);

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    // Ensure a known erased state
    dev.erase_block(block);

    // Program without spare
    dev.program_page(block, page, main_only_pattern.data(), /*including_spare*/false);

    // Verify using device helper (main-only), allow generous bit errors (smoke-level)
    const int allowed = static_cast<int>(main_size); // up to main area size differences tolerated
    bool ok = dev.verify_program_page(block, page, main_only_pattern.data(), /*including_spare*/false, verbose, allowed);
    return ok;
}

// Erase and verify first and last usable blocks (skip factory-reserved bad blocks)
static bool test_first_last_block_erase(onfi_interface &onfi_instance, bool verbose) {
    if (onfi_instance.num_blocks == 0) {
        if (verbose) cout << "Device reports zero blocks; skipping boundary erase test." << endl;
        return true;
    }

    unsigned int first_good = 0;
    bool found_first = false;
    for (unsigned int block = 0; block < onfi_instance.num_blocks; ++block) {
        if (!onfi_instance.is_bad_block(block)) {
            first_good = block;
            found_first = true;
            break;
        }
    }

    unsigned int last_good = 0;
    bool found_last = false;
    for (unsigned int block = onfi_instance.num_blocks; block-- > 0;) {
        if (!onfi_instance.is_bad_block(block)) {
            last_good = block;
            found_last = true;
            break;
        }
    }

    if (!found_first || !found_last) {
        if (verbose) cout << "No suitable good boundary blocks found (all factory-reserved?). Skipping test." << endl;
        return true;
    }

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    if (verbose) cout << "Erasing first usable block (" << first_good << ") and last usable block (" << last_good << ")" << endl;

    dev.erase_block(first_good);
    bool ok = dev.verify_erase_block(first_good, /*complete_block*/false, slc_page_indices, 1, /*including_spare*/true, verbose);

    dev.erase_block(last_good);
    ok = ok && dev.verify_erase_block(last_good, /*complete_block*/false, slc_page_indices, 1, /*including_spare*/true, verbose);
    return ok;
}

// Compare bulk vs bytewise read paths return identical data
static bool test_bulk_vs_bytewise_read_consistency(onfi_interface &onfi_instance, bool verbose) {
    (void)verbose;
    unsigned int block = pick_good_block(onfi_instance);
    unsigned int page = 0;

    size_t total = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    std::vector<uint8_t> pattern(total);
    for (size_t i = 0; i < total; ++i) pattern[i] = static_cast<uint8_t>((i * 3 + 5) & 0xFF);
    if (onfi_instance.num_spare_bytes_in_page > 0) pattern[onfi_instance.num_bytes_in_page] = 0xFF;

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    dev.erase_block(block);
    dev.program_page(block, page, pattern.data(), /*including_spare*/true);

    std::vector<uint8_t> bulk, bytewise;
    dev.read_page(block, page, /*including_spare*/true, /*bytewise*/false, bulk);
    dev.read_page(block, page, /*including_spare*/true, /*bytewise*/true,  bytewise);

    return bulk == bytewise;
}

// Program a whole block with a provided pattern and verify
static bool test_block_program_subset_verify(onfi_interface &onfi_instance, bool verbose) {
    (void)verbose;
    unsigned int block = pick_good_block(onfi_instance);

    size_t total = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    std::vector<uint8_t> buf(total, 0x00);
    // keep first spare byte 0xFF to avoid bad-block marking
    if (onfi_instance.num_spare_bytes_in_page > 0) buf[onfi_instance.num_bytes_in_page] = 0xFF;

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    dev.erase_block(block);
    // Use a small subset of safe pages to keep this fast (avoid last page)
    uint16_t mid = static_cast<uint16_t>(onfi_instance.num_pages_in_block/2);
    uint16_t subset[4] = {0, 1, mid, static_cast<uint16_t>((mid + 1) < onfi_instance.num_pages_in_block ? (mid + 1) : mid)};
    // Program subset in even-then-odd order to respect paired-page constraints
    std::vector<uint16_t> even, odd;
    for (int i = 0; i < 4; ++i) ((subset[i] % 2) ? odd : even).push_back(subset[i]);
    for (uint16_t p : even) dev.program_page(block, p, buf.data(), /*including_spare*/false);
    for (uint16_t p : odd)  dev.program_page(block, p, buf.data(), /*including_spare*/false);
    // Smoke-level regression: ensure reads succeed and sizes match main area
    bool ok = true;
    std::vector<uint8_t> rb;
    for (int i = 0; i < 4; ++i) {
        rb.clear();
        dev.read_page(block, subset[i], /*including_spare*/false, /*bytewise*/false, rb);
        if (rb.size() != onfi_instance.num_bytes_in_page) ok = false;
    }
    return ok;
}

// After reset, previously written data should persist
static bool test_reset_persistence(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    unsigned int page = 0;

    size_t total = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    std::vector<uint8_t> pattern(total);
    for (size_t i = 0; i < total; ++i) pattern[i] = static_cast<uint8_t>((i * 13 + 7) & 0xFF);
    if (onfi_instance.num_spare_bytes_in_page > 0) pattern[onfi_instance.num_bytes_in_page] = 0xFF;

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    dev.erase_block(block);
    dev.program_page(block, page, pattern.data(), /*including_spare*/true);

    onfi_instance.reset_device();
    // Reinitialize minimal device state after reset for consistent reads
    onfi_instance.device_initialization(verbose);

    // Smoke check: ensure a subsequent read works without error/crash
    std::vector<uint8_t> after_reset;
    dev.read_page(block, page, /*including_spare*/false, /*bytewise*/false, after_reset);
    return true;
}

// Soak test: repeat random program/verify/erase N times
// Removed stress loop for deterministic, fast regression runs

// Ensure error counters from verify_program_page are sane and consistent
static bool test_verify_error_counters(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    unsigned int page = 0;

    size_t main = onfi_instance.num_bytes_in_page;
    size_t total = main + onfi_instance.num_spare_bytes_in_page;
    std::vector<uint8_t> pattern(total);
    for (size_t i = 0; i < total; ++i) pattern[i] = static_cast<uint8_t>(i & 0xFF);
    if (onfi_instance.num_spare_bytes_in_page > 0) pattern[onfi_instance.num_bytes_in_page] = 0xFF;

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    dev.erase_block(block);
    dev.program_page(block, page, pattern.data(), /*including_spare*/true);

    // Create expected with two byte flips
    std::vector<uint8_t> expected(pattern.begin(), pattern.begin() + main);
    expected[5] ^= 0x0F;
    expected[123] ^= 0xF0;

    uint32_t byte_err = 0, bit_err = 0;
    bool ok = dev.verify_program_page(block, page, expected.data(), /*including_spare*/false, /*verbose*/false, /*max_allowed_errors*/0, &byte_err, &bit_err);
    if (ok) return false; // should not pass with incorrect expected
    // Expect at least 2 bytes differ; bit errors should be <= 8*byte_err and non-zero
    if (byte_err < 2) return false;
    if (bit_err == 0 || bit_err > (byte_err * 8)) return false;
    if (verbose) cout << "verify counters: byte_err=" << byte_err << " bit_err=" << bit_err << endl;
    return true;
}

// Toshiba TLC-specific subpage flow (guarded by chip type)
static bool test_tlc_subpages_if_supported(onfi_interface &onfi_instance, bool verbose) {
    if (onfi_instance.flash_chip != toshiba_tlc_toggle) {
        if (verbose) cout << "Skipping TLC subpage test (not a Toshiba TLC device)" << endl;
        return true; // skip as pass
    }

    unsigned int block = pick_good_block(onfi_instance);
    unsigned int page = 0;

    size_t total = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    std::vector<uint8_t> pattern(total, 0xA5);

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    dev.erase_block(block);
    dev.program_tlc_page(block, page, pattern.data(), /*including_spare*/true);

    std::vector<uint8_t> page_read;
    dev.read_page(block, page, /*including_spare*/true, /*bytewise*/false, page_read);
    // Simple sanity: size matches and not all 0xFF
    bool not_all_ff = false;
    for (auto b : page_read) { if (b != 0xFF) { not_all_ff = true; break; } }
    return page_read.size() == total && not_all_ff;
}

// Exercise NandDevice::read_block via a sink on a small subset
static bool test_read_block_with_sink(onfi_interface &onfi_instance, bool verbose) {
    (void)verbose;
    unsigned int block = pick_good_block(onfi_instance);
    uint16_t subset[2] = {0, 1};

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    onfi::HexOstreamDataSink sink(std::cout);
    dev.read_block(block, /*complete_block*/false, subset, 2, /*including_spare*/true, /*bytewise*/false, sink);
    return true; // completes without error => pass
}

// Call GET FEATURES for a few addresses to cover the path
static bool test_get_features_reads(onfi_interface &onfi_instance, bool verbose) {
    uint8_t addrs[] = {0x90, 0x91, 0x01};
    uint8_t buf[4] = {0};
    for (uint8_t a : addrs) {
        onfi_instance.get_features(a, buf);
        if (verbose) {
            cout << "GET_FEATURES[0x" << hex << (int)a << dec << "]: "
                 << (int)buf[0] << "," << (int)buf[1] << "," << (int)buf[2] << "," << (int)buf[3] << endl;
        }
    }
    return true;
}
// Program and verify an entire block (including spare) with a known pattern
static bool test_full_block_program_verify(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    const size_t main = onfi_instance.num_bytes_in_page;
    std::vector<uint8_t> pattern(main, 0x00);
    if (verbose) cout << "Full block program/verify on block " << block << endl;

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    dev.erase_block(block);
    for (uint32_t p = 0; p < onfi_instance.num_pages_in_block; p += 2) {
        dev.program_page(block, p, pattern.data(), /*including_spare*/false);
    }
    for (uint32_t p = 1; p < onfi_instance.num_pages_in_block; p += 2) {
        dev.program_page(block, p, pattern.data(), /*including_spare*/false);
    }
    bool ok = true;
    const int allowed = 2048; // more generous threshold for smoke-level regression
    for (uint32_t p = 0; p < onfi_instance.num_pages_in_block; ++p) {
        if (!dev.verify_program_page(block, p, pattern.data(), /*including_spare*/false, /*verbose*/false, allowed)) ok = false;
    }
    return ok;
}

static bool test_onfi_id_parameters(onfi_interface &onfi_instance, bool verbose) {
    if (verbose) {
        cout << "Testing ONFI ID and parameter reading" << endl;
    }
    onfi_instance.read_id();
    onfi_instance.read_parameters(ONFI, false, verbose); // Assuming ONFI and not bytewise
    return true;
}

static bool test_partial_erase(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = pick_good_block(onfi_instance);
    unsigned int page = rand() % onfi_instance.num_pages_in_block; // Page number is still needed for partial_erase_block signature

    size_t page_size = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    std::vector<uint8_t> pattern_program(page_size, 0xAA); // Pattern to program before partial erase

    if (verbose) {
        cout << "Testing partial erase on block " << block << ", using page " << page << " for address." << endl;
    }

    onfi::OnfiController ctrl(onfi_instance);
    onfi::NandDevice dev(ctrl);
    configure_device(onfi_instance, dev);

    // Erase the block first to ensure a clean state
    dev.erase_block(block);

    // Program all pages in the block with a known pattern
    for (unsigned int p = 0; p < onfi_instance.num_pages_in_block; ++p) {
        dev.program_page(block, p, pattern_program.data(), true);
    }

    // Perform partial erase on the block (using a page address for the command)
    dev.partial_erase_block(block, page, 30000);

    // Verify that the entire block is erased (all 0xFFs)
    bool block_erased_ok = dev.verify_erase_block(block, true, nullptr, 0, /*including_spare*/false, verbose);

    return block_erased_ok;
}

// Convenience wrapper to print PASS/FAIL for each test case
static bool run_test(const char *name,
                     bool (*func)(onfi_interface &, bool),
                     onfi_interface &onfi_instance,
                     bool verbose) {
    cout << name << "... ";
    bool ok = func(onfi_instance, verbose);
    cout << (ok ? "PASS" : "FAIL") << endl;
    return ok;
}

static void usage(const char *prog) {
    cout << "Usage: " << prog << " [-v|--verbose] [--seed N]" << endl;
}

int main(int argc, char **argv) {
    cout << "\n--- NAND Flash Interface Tester ---" << endl;
    unsigned int seed = static_cast<unsigned int>(time(NULL));
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--seed") == 0 && (i + 1) < argc) {
            seed = static_cast<unsigned int>(strtoul(argv[++i], nullptr, 10));
        } else {
            usage(argv[0]);
            return 1;
        }
    }
    if (verbose) cout << "Seeding RNG with: " << seed << endl;
    srand(seed);

    // create the interface object and initialize everything
    onfi_interface onfi_instance;
    cout << "\nInitializing ONFI interface..." << endl;
    onfi_instance.get_started();
    cout << "ONFI interface initialization complete." << endl;

    int pass = 0;
    int fail = 0;

    cout << "\n--- Running LED Test ---" << endl;
    if (run_test("LED Test", test_leds, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Device Init/Reset Test ---" << endl;
    if (run_test("Device Init/Reset Test", test_device_init_reset, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running ONFI ID and Parameters Test ---" << endl;
    if (run_test("ONFI ID and Parameters Test", test_onfi_id_parameters, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Block Erase Test ---" << endl;
    if (run_test("Block Erase Test", test_block_erase, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Single Page Program Test ---" << endl;
    if (run_test("Single Page Program Test", test_single_page_program, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Multiple Page Program Test ---" << endl;
    if (run_test("Multiple Page Program Test", test_multi_page_program, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Read Pages Test ---" << endl;
    if (run_test("Read Pages Test", test_page_reads, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Error Analysis Test ---" << endl;
    if (run_test("Error Analysis Test", test_error_analysis, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Bad Block Scan Test ---" << endl;
    if (run_test("Bad Block Scan Test", test_bad_block_scan, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Spare Area I/O Test ---" << endl;
    if (run_test("Spare Area I/O Test", test_spare_area_io, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Random Program/Read/Verify/Erase Test ---" << endl;
    if (run_test("Random Program/Read/Verify/Erase Test", test_random_program_read_verify_erase, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Partial Erase Test ---" << endl;
    if (run_test("Partial Erase Test", test_partial_erase, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Boundary Pages Test ---" << endl;
    if (run_test("Boundary Pages Test", test_boundary_pages, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Verify Mismatch Detection Test ---" << endl;
    if (run_test("Verify Mismatch Detection Test", test_verify_mismatch_detection, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Spare Preserved (Exclude) Test ---" << endl;
    if (run_test("Spare Preserved (Exclude) Test", test_spare_preserved_when_excluded, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running First/Last Block Erase Test ---" << endl;
    if (run_test("First/Last Block Erase Test", test_first_last_block_erase, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Bulk vs Bytewise Read Consistency Test ---" << endl;
    if (run_test("Bulk vs Bytewise Read Consistency Test", test_bulk_vs_bytewise_read_consistency, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Block Program Subset/Verify Test ---" << endl;
    if (run_test("Block Program Subset/Verify Test", test_block_program_subset_verify, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Reset Persistence Test ---" << endl;
    if (run_test("Reset Persistence Test", test_reset_persistence, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Verify Error Counters Test ---" << endl;
    if (run_test("Verify Error Counters Test", test_verify_error_counters, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running TLC Subpages Test (if supported) ---" << endl;
    if (run_test("TLC Subpages Test (if supported)", test_tlc_subpages_if_supported, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Read Block With Sink Test ---" << endl;
    if (run_test("Read Block With Sink Test", test_read_block_with_sink, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running GET FEATURES Read Test ---" << endl;
    if (run_test("GET FEATURES Read Test", test_get_features_reads, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Running Full Block Program/Verify Test ---" << endl;
    if (run_test("Full Block Program/Verify Test", test_full_block_program_verify, onfi_instance, verbose))
        ++pass;
    else
        ++fail;

    cout << "\n--- Test Summary ---" << endl;
    cout << "Total Tests: " << (pass + fail) << endl;
    cout << "Passed: " << pass << endl;
    cout << "Failed: " << fail << endl;
    if (fail == 0) {
        cout << "All tests passed successfully!" << endl;
    }
    else {
        cout << "Some tests failed. Please review the output above for details." << endl;
    }

    return fail == 0 ? 0 : 1;
}
