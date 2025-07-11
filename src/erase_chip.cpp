#include "onfi_interface.h"
#include <iostream>
#include <string>
#include <cstring>

using namespace std;

static void usage(const char *prog) {
    cout << "Usage: " << prog << " [-v]" << endl;
}

int main(int argc, char **argv) {
    cout << "\n--- NAND Flash Chip Eraser ---" << endl;
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            verbose = true;
        else {
            usage(argv[0]);
            return 1;
        }
    }

    onfi_interface onfi_instance;
    cout << "Initializing ONFI interface..." << endl;
    onfi_instance.get_started();
    cout << "ONFI interface initialization complete." << endl;

    cout << "Starting full chip erase..." << endl;
    for (unsigned int block = 0; block < onfi_instance.num_blocks; ++block) {
        cout << "Erasing block " << block << "/" << onfi_instance.num_blocks - 1 << endl;
        onfi_instance.erase_block(block, verbose);
    }
    cout << "Full chip erase complete." << endl;

    return 0;
}
