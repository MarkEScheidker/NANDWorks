/*! \mainpage
File: main.cpp

Description: This source file is to test the basic functionality of the interface designed.
                        .. This file tests the connection as well as the major functions like
                        .. .. erase, program etc
*/

// onfi_interface.h has the necessary functionalities defined
#include "onfi_interface.h"

#include <vector>
#include <cstdlib>
#include <cstring>
#include <algorithm>    // std::sort
#include <unordered_map>
#include <time.h>

// Define a maximum allowed error threshold for programming tests
const int MAX_ALLOWED_ERRORS = 50; // Example: Allow up to 50 bit errors per page

using namespace std;

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
    unsigned int block = rand() % onfi_instance.num_blocks;

    if (verbose)
        cout << "Erasing block " << block << endl;
    onfi_instance.erase_block(block, verbose);
    return onfi_instance.verify_block_erase(block, true, nullptr, 0, verbose);
}

static bool test_single_page_program(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = rand() % onfi_instance.num_blocks;
    unsigned int page = 0;

    size_t page_size = onfi_instance.num_bytes_in_page +
                       onfi_instance.num_spare_bytes_in_page;
    uint8_t *pattern = new uint8_t[page_size];
    for (size_t i = 0; i < page_size; ++i)
        pattern[i] = static_cast<uint8_t>(i & 0xff);
    // Ensure the first byte of the spare area is not 0x00 to avoid marking as bad block
    pattern[onfi_instance.num_bytes_in_page] = 0xFF;

    onfi_instance.program_page(block, page, pattern, true, verbose);
    bool ok = onfi_instance.verify_program_page(block, page, pattern, verbose, MAX_ALLOWED_ERRORS);
    delete[] pattern;
    return ok;
}

static bool test_multi_page_program(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = rand() % onfi_instance.num_blocks;

    onfi_instance.program_pages_in_a_block(block, false, false, slc_page_indices, 20, verbose);
    return onfi_instance.verify_program_pages_in_a_block(block, false,
                                                         slc_page_indices,
                                                         20, verbose, MAX_ALLOWED_ERRORS);
}

static bool test_page_reads(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = rand() % onfi_instance.num_blocks;

    onfi_instance.read_and_spit_page(block, 0, true, verbose);
    onfi_instance.read_and_spit_page(block, 1, true, verbose);
    return true; // failures print directly
}

static bool test_error_analysis(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = rand() % onfi_instance.num_blocks;
    unsigned int page = 0;
    if (verbose) {
        cout << "Performing error analysis on block " << block << " page " << page << endl;
    }

    size_t page_size = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    uint8_t *pattern = new uint8_t[page_size];
    for (size_t i = 0; i < page_size; ++i) {
        pattern[i] = static_cast<uint8_t>(i & 0xff);
    }
    // Ensure the first byte of the spare area is not 0x00 to avoid marking as bad block
    pattern[onfi_instance.num_bytes_in_page] = 0xFF;

    onfi_instance.erase_block(block, verbose);
    onfi_instance.program_page(block, page, pattern, true, verbose);

    uint8_t *read_data = new uint8_t[page_size];
    onfi_instance.read_page_and_return_value(block, page, 0, read_data, true);

    int errors = 0;
    for (size_t i = 0; i < onfi_instance.num_bytes_in_page; ++i) {
        if (read_data[i] != pattern[i]) {
            errors++;
        }
    }

    cout << "Found " << errors << " errors in " << onfi_instance.num_bytes_in_page << " bytes ("
         << (static_cast<double>(errors) / onfi_instance.num_bytes_in_page) * 100 << "%)" << endl;

    delete[] pattern;
    delete[] read_data;
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
    unsigned int block = rand() % onfi_instance.num_blocks;
    unsigned int page = rand() % onfi_instance.num_pages_in_block;

    size_t page_size = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    uint8_t *pattern = new uint8_t[page_size];
    for (size_t i = 0; i < page_size; ++i) {
        pattern[i] = static_cast<uint8_t>(rand() % 256); // Random data
    }
    // Ensure the first byte of the spare area is not 0x00 to avoid marking as bad block
    pattern[onfi_instance.num_bytes_in_page] = 0xFF;

    if (verbose) {
        cout << "Testing random program/read/verify/erase on block " << block << " page " << page << endl;
    }

    // Erase the block before programming
    onfi_instance.erase_block(block, verbose);

    // Program the page with random data
    onfi_instance.program_page(block, page, pattern, true, verbose);

    // Verify the programmed page
    bool program_ok = onfi_instance.verify_program_page(block, page, pattern, verbose, MAX_ALLOWED_ERRORS);

    // Erase the block after verification
    onfi_instance.erase_block(block, verbose);
    bool erase_ok = onfi_instance.verify_block_erase(block, true, nullptr, 0, verbose);

    delete[] pattern;
    return program_ok && erase_ok;
}

static bool test_spare_area_io(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = rand() % onfi_instance.num_blocks;
    unsigned int page = 0;
    size_t page_size = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    if (verbose) {
        cout << "Testing spare area I/O on block " << block << " page " << page << endl;
        cout << "Page size (main + spare): " << page_size << " bytes" << endl;
    }
    uint8_t *pattern = new uint8_t[page_size];
    // Fill main data area with one pattern
    for (size_t i = 0; i < onfi_instance.num_bytes_in_page; ++i) {
        pattern[i] = 0xAA;
    }
    // Fill spare area with another pattern
    for (size_t i = onfi_instance.num_bytes_in_page; i < page_size; ++i) {
        pattern[i] = 0x55;
    }

    onfi_instance.erase_block(block, verbose);
    onfi_instance.program_page(block, page, pattern, true, verbose);

    uint8_t *read_data = new uint8_t[page_size];
    onfi_instance.read_page_and_return_value(block, page, 0, read_data, true);

    bool ok = true;
    // Only verify the spare area for this test
    for (size_t i = onfi_instance.num_bytes_in_page; i < page_size; ++i) {
        if (read_data[i] != 0x55) {
            ok = false;
            if (verbose) {
                cout << "Mismatch in spare area at byte " << i << ": Expected 0x55, Got 0x" << hex << (int)read_data[i] << dec << endl;
            }
            break;
        }
    }

    if (verbose) {
        cout << "Pattern (last 16 bytes of main + first 16 bytes of spare):" << endl;
        for (size_t i = onfi_instance.num_bytes_in_page - 16; i < page_size && i < onfi_instance.num_bytes_in_page + 16; ++i) {
            cout << hex << (int)pattern[i] << " " << dec;
        }
        cout << endl;
        cout << "Read Data (last 16 bytes of main + first 16 bytes of spare):" << endl;
        for (size_t i = onfi_instance.num_bytes_in_page - 16; i < page_size && i < onfi_instance.num_bytes_in_page + 16; ++i) {
            cout << hex << (int)read_data[i] << " " << dec;
        }
        cout << endl;
    }

    delete[] pattern;
    delete[] read_data;
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

static bool test_onfi_id_parameters(onfi_interface &onfi_instance, bool verbose) {
    if (verbose) {
        cout << "Testing ONFI ID and parameter reading" << endl;
    }
    onfi_instance.read_id();
    onfi_instance.read_parameters(ONFI, false, verbose); // Assuming ONFI and not bytewise
    return true;
}

static bool test_partial_erase(onfi_interface &onfi_instance, bool verbose) {
    unsigned int block = rand() % onfi_instance.num_blocks;
    unsigned int page = rand() % onfi_instance.num_pages_in_block; // Page number is still needed for partial_erase_block signature

    size_t page_size = onfi_instance.num_bytes_in_page + onfi_instance.num_spare_bytes_in_page;
    uint8_t *pattern_program = new uint8_t[page_size];
    memset(pattern_program, 0xAA, page_size); // Pattern to program before partial erase

    if (verbose) {
        cout << "Testing partial erase on block " << block << ", using page " << page << " for address." << endl;
    }

    // Erase the block first to ensure a clean state
    onfi_instance.erase_block(block, verbose);

    // Program all pages in the block with a known pattern
    for (unsigned int p = 0; p < onfi_instance.num_pages_in_block; ++p) {
        onfi_instance.program_page(block, p, pattern_program, true, verbose);
    }

    // Perform partial erase on the block (using a page address for the command)
    onfi_instance.partial_erase_block(block, page, 30000, verbose);

    // Verify that the entire block is erased (all 0xFFs)
    bool block_erased_ok = onfi_instance.verify_block_erase(block, true, nullptr, 0, verbose);

    delete[] pattern_program;

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
    cout << "Usage: " << prog << " [-v]" << endl;
}

int main(int argc, char **argv) {
    cout << "\n--- NAND Flash Interface Tester ---" << endl;
    srand(time(NULL)); // Seed the random number generator
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            verbose = true;
        else {
            usage(argv[0]);
            return 1;
        }
    }

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
