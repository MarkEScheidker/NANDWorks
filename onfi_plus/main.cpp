/*! \mainpage
File: main.cpp

Description: This source file is to test the basic functionality of the interface designed.
			.. This file tests the connection as well as the major functions like
			.. .. erase, program etc
*/

// onfi_head.h has the necessary functinalities defined
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

// just the main function
int main()
{
	// let us make an object for onfi_interface
	// ..all the erase and program are defined inside the onfi interface class
	onfi_interface onfi_instance;
	onfi_instance.get_started();

	// following are user defined numbers
	unsigned int my_block_number = 0x01; // this is the block idx in the flash chip
	unsigned int my_page_number = 393; // this is the page idx inside the block above

	// onfi_instance.erase_block(my_block_number);
	// // onfi_instance.verify_block_erase_sample(my_block_number);

	// uint8_t* data_to_program = (uint8_t*)malloc(sizeof(uint8_t)*(onfi_instance.num_bytes_in_page+onfi_instance.num_spare_bytes_in_page));
	// for(int i = 0; i < onfi_instance.num_bytes_in_page+onfi_instance.num_spare_bytes_in_page; i++)
	// {
	// 	data_to_program[i] = i%256;
	// }

	// onfi_instance.program_block_tlc_micron(my_block_number, data_to_program,true,true);
	onfi_instance.read_and_spit_page(my_block_number, my_page_number, true);
	my_page_number = 538; // this is the page idx inside the block above
	onfi_instance.read_and_spit_page(my_block_number, my_page_number, true);

	return 0;
}
