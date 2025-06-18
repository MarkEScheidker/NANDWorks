/*! \mainpage
File: main.cpp

Description: This source file is to test the basic functionality of the interface designed.
                        .. This file tests the connection as well as the major functions like
                        .. .. erase, program etc
*/

// onfi_head.h has the necessary functionalities defined
#include "onfi_head.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>    // std::sort
#include <unordered_map>
#include <time.h>

using namespace std;

/**
these are the page indices that will be used for analysis for SLC
*/
uint16_t slc_page_indices[20] = {0,2,4,6,
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
// ---------------------------------------------------------------------------
// Helper routines implementing individual test cases.  Each returns true on
// success so the caller can tally overall pass/fail statistics.
// ---------------------------------------------------------------------------

static bool test_leds(onfi_interface &onfi_instance, bool verbose)
{
        if(verbose)
                cout << "Running LED test" << endl;
        onfi_instance.test_onfi_leds(verbose);
        return true;        // test_onfi_leds prints errors itself
}

static bool test_block_erase(onfi_interface &onfi_instance, bool verbose)
{
        unsigned int block = 0x01;

        if(verbose)
                cout << "Erasing block " << block << endl;
        onfi_instance.erase_block(block, verbose);
        return onfi_instance.verify_block_erase(block, true, nullptr, 0, verbose);
}

static bool test_single_page_program(onfi_interface &onfi_instance, bool verbose)
{
        unsigned int block = 0x01;
        unsigned int page  = 0;

        size_t page_size = onfi_instance.num_bytes_in_page +
                           onfi_instance.num_spare_bytes_in_page;
        uint8_t *pattern = new uint8_t[page_size];
        for(size_t i = 0; i < page_size; ++i)
                pattern[i] = static_cast<uint8_t>(i & 0xff);

        onfi_instance.program_page(block, page, pattern, true, verbose);
        bool ok = onfi_instance.verify_program_page(block, page, pattern, verbose);
        delete[] pattern;
        return ok;
}

static bool test_multi_page_program(onfi_interface &onfi_instance, bool verbose)
{
        unsigned int block = 0x01;

        onfi_instance.program_pages_in_a_block(block, false, false,
                                               slc_page_indices, 20, verbose);
        return onfi_instance.verify_program_pages_in_a_block(block, false,
                                                             slc_page_indices,
                                                             20, verbose);
}

static bool test_page_reads(onfi_interface &onfi_instance, bool verbose)
{
        unsigned int block = 0x01;

        onfi_instance.read_and_spit_page(block, 0, true, verbose);
        onfi_instance.read_and_spit_page(block, 1, true, verbose);
        return true;        // failures print directly
}

// Convenience wrapper to print PASS/FAIL for each test case
static bool run_test(const char *name,
                     bool (*func)(onfi_interface &, bool),
                     onfi_interface &onfi_instance,
                     bool verbose)
{
        cout << name << "... ";
        bool ok = func(onfi_instance, verbose);
        cout << (ok ? "PASS" : "FAIL") << endl;
        return ok;
}

static void usage(const char *prog)
{
        cout << "Usage: " << prog << " [-v]" << endl;
}

int main(int argc, char **argv)
{
        bool verbose = false;
        for(int i = 1; i < argc; ++i)
        {
                if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
                        verbose = true;
                else
                {
                        usage(argv[0]);
                        return 1;
                }
        }

        // create the interface object and initialize everything
        onfi_interface onfi_instance;
        onfi_instance.get_started();

        int pass = 0;
        int fail = 0;

        if(run_test("LED test", test_leds, onfi_instance, verbose))
                ++pass;
        else
                ++fail;

        if(run_test("Block erase", test_block_erase, onfi_instance, verbose))
                ++pass;
        else
                ++fail;

        if(run_test("Single page program", test_single_page_program, onfi_instance, verbose))
                ++pass;
        else
                ++fail;

        if(run_test("Multiple page program", test_multi_page_program, onfi_instance, verbose))
                ++pass;
        else
                ++fail;

        if(run_test("Read pages", test_page_reads, onfi_instance, verbose))
                ++pass;
        else
                ++fail;

        cout << pass << " tests passed, " << fail << " failed." << endl;

        return fail == 0 ? 0 : 1;
}
