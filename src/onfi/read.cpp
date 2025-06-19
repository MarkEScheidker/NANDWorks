#include "onfi_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <algorithm>
using namespace std;

void onfi_interface::read_page(unsigned int my_block_number, unsigned int my_page_number,uint8_t address_length,bool verbose)
{
	if(address_length != num_column_cycles+num_row_cycles)
	{
		fprintf(stdout,"E: Address Length Mismatch.");
	}
	uint8_t address[address_length];
	convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, address);
	// make sure none of the LUNs are busy
	while((*jumper_address & hw::RB_mask)==0);

	if(flash_chip==toshiba_tlc_toggle)
		send_command(0x0);

	send_command(0x00);
	send_addresses(address,address_length);

#if PROFILE_TIME
	time_info_file<<"Reading page:..";
	START_TIME;
#endif

	send_command(0x30);

	// just a delay
	tWB;
	// // this is added just for read-retry operation
	// for(uint8_t vv=0;vv<255;vv++)
	// {
	// 	asm("nop");
	// }

	// check for RDY signal
	while((*jumper_address & hw::RB_mask)==0);
#if PROFILE_TIME
	END_TIME;
	if(verbose) fprintf(stdout,"Read page completed \n");
	PRINT_TIME;
#endif	
	// tRR = 40ns
	asm("NOP");
}

// this function opens a file named time_info_file.txt
// .. this file will log all the duration from the timing operations as necessary
void onfi_interface::change_read_column(uint8_t* col_address)
{
	tRHW;	// tRHW = 200ns

	send_command(0x05);
	send_addresses(col_address,2);
	send_command(0xe0);

	tCCS;	//tCCS = 200ns
}

// function to disable Program and Erase operation
// .. when WP is low, program and erase operation are disabled
// .. when WP is high, program and erase operation are enabled
void onfi_interface::read_and_spit_page(unsigned int my_block_number, unsigned int my_page_number, bool bytewise, bool verbose)
{
	uint8_t* data_read_from_page = (uint8_t*)malloc(sizeof(uint8_t)*(num_bytes_in_page+num_spare_bytes_in_page));

	// let us first reset all the values in the local variables to 0x00
        memset(data_read_from_page,0xff,num_bytes_in_page+num_spare_bytes_in_page);
	//first let us get the data from the page to the cache memory
	read_page(my_block_number, my_page_number, 5);
	// now let us get the values from the cache memory to our local variable
	if(bytewise)
	{
		uint8_t col_address[2] = {0,0};		
		for(uint32_t b_idx = 0;b_idx< num_bytes_in_page+num_spare_bytes_in_page;b_idx++)
		{
			col_address[1] = b_idx/256;
			col_address[0] = b_idx%256;
			change_read_column(col_address);
			asm("NOP");
			asm("NOP");
			get_data(data_read_from_page+b_idx,1);
		}
	}else
	{
		get_data(data_read_from_page,num_bytes_in_page+num_spare_bytes_in_page);
	}

	if(verbose)
	{
		#if DEBUG_ONFI
			if(onfi_debug_file)
			{
				onfi_debug_file<<"Reading page ("<<my_page_number<<" of block "<< my_block_number<<" ):"<<endl;
			}
			else fprintf(stdout,"Reading page (%d of block %d):\n",my_page_number, my_block_number);
		#else
			fprintf(stdout,"Reading page (%d of block %d):\n",my_page_number, my_block_number);
		#endif
	}

	//now iterate through each of them to see if they are correct
	uint16_t byte_id = 0;
	for(byte_id = 0;byte_id<(num_bytes_in_page+num_spare_bytes_in_page);byte_id++)
	{
		//we will spit each byte out from here in hex
		if(onfi_data_file)
		{
			onfi_data_file<<data_read_from_page[byte_id];
		}else
		{
			cout<<data_read_from_page[byte_id];
		}
	}

	// just terminate the sequence with a newline
	if(onfi_data_file)
	{
		onfi_data_file<<endl;
	}else
	{
		cout<<endl;
	}

	if(verbose)
	{
		#if DEBUG_ONFI
			if(onfi_debug_file)
			{
				onfi_debug_file<<".. Reading page completed"<<endl;
			}
			else fprintf(stdout,".. Reading page completed\n");
		#else
			fprintf(stdout,".. Reading page completed\n");
		#endif
	}
	fflush(stdout);
	free(data_read_from_page);
}

void onfi_interface::read_and_spit_page_tlc_toshiba(unsigned int my_block_number, unsigned int my_page_number, bool verbose)
{

	if(flash_chip != toshiba_tlc_toggle)
	{
		fprintf(stdout, "Read Function for TOSHIBA toggle chip attempted use of different flash chip. NOT ALLOWED.\n");
		exit(-1);
	}

	uint8_t* data_read_from_page = (uint8_t*)malloc(sizeof(uint8_t)*(num_bytes_in_page+num_spare_bytes_in_page));

	// let us first reset all the values in the local variables to 0xff
        memset(data_read_from_page,0xff,num_bytes_in_page+num_spare_bytes_in_page);

	// for LSB page
	send_command(0x01);
	//first let us get the data from the page to the cache memory
	read_page(my_block_number, my_page_number, 5);
	// now let us get the values from the cache memory to our local variable
	get_data(data_read_from_page,num_bytes_in_page+num_spare_bytes_in_page);

	if(verbose)
	{
		#if DEBUG_ONFI
			if(onfi_debug_file)
			{
				onfi_debug_file<<"Reading LSB subpage of page ("<<my_page_number<<" of block "<< my_block_number<<" ):"<<endl;
			}
			else fprintf(stdout,"Reading LSB subpage of page (%d of block %d):\n",my_page_number, my_block_number);
		#else
			fprintf(stdout,"Reading LSB subpage of page (%d of block %d):\n",my_page_number, my_block_number);
		#endif
	}

	//now iterate through each of them to see if they are correct
	uint16_t byte_id = 0;
	for(byte_id = 0;byte_id<(num_bytes_in_page+num_spare_bytes_in_page);byte_id++)
	{
		//we will spit each byte out from here in hex
		if(onfi_data_file)
		{
			onfi_data_file<<data_read_from_page[byte_id];
		}else
		{
			cout<<data_read_from_page[byte_id];
		}
	}

	// just terminate the sequence with a newline
	if(onfi_data_file)
	{
		onfi_data_file<<endl;
	}else
	{
		cout<<endl;
	}

	if(verbose)
	{
		#if DEBUG_ONFI
			if(onfi_debug_file)
			{
				onfi_debug_file<<".. Reading  LSB subpage of page completed"<<endl;
			}
			else fprintf(stdout,".. Reading  LSB subpage of page completed\n");
		#else
			fprintf(stdout,".. Reading  LSB subpage of page completed\n");
		#endif
	}
	fflush(stdout);

//////////////////////
	// let us first reset all the values in the local variables to 0xff
        memset(data_read_from_page,0xff,num_bytes_in_page+num_spare_bytes_in_page);
	// for CSB page
	send_command(0x02);
	//first let us get the data from the page to the cache memory
	read_page(my_block_number, my_page_number, 5);
	// now let us get the values from the cache memory to our local variable
	get_data(data_read_from_page,num_bytes_in_page+num_spare_bytes_in_page);

	if(verbose)
	{
		#if DEBUG_ONFI
			if(onfi_debug_file)
			{
				onfi_debug_file<<"Reading CSB subpage of page ("<<my_page_number<<" of block "<< my_block_number<<" ):"<<endl;
			}
			else fprintf(stdout,"Reading CSB subpage of page (%d of block %d):\n",my_page_number, my_block_number);
		#else
			fprintf(stdout,"Reading CSB subpage of page (%d of block %d):\n",my_page_number, my_block_number);
		#endif
	}

	//now iterate through each of them to see if they are correct
	for(byte_id = 0;byte_id<(num_bytes_in_page+num_spare_bytes_in_page);byte_id++)
	{
		//we will spit each byte out from here in hex
		if(onfi_data_file)
		{
			onfi_data_file<<data_read_from_page[byte_id];
		}else
		{
			cout<<data_read_from_page[byte_id];
		}
	}

	// just terminate the sequence with a newline
	if(onfi_data_file)
	{
		onfi_data_file<<endl;
	}else
	{
		cout<<endl;
	}

	if(verbose)
	{
		#if DEBUG_ONFI
			if(onfi_debug_file)
			{
				onfi_debug_file<<".. Reading  CSB subpage of page completed"<<endl;
			}
			else fprintf(stdout,".. Reading CSB subpage of page completed\n");
		#else
			fprintf(stdout,".. Reading CSB subpage of page completed\n");
		#endif
	}
	fflush(stdout);
/////////////////////////////////
	// let us first reset all the values in the local variables to 0xff
        memset(data_read_from_page,0xff,num_bytes_in_page+num_spare_bytes_in_page);
	// for MSB page
	send_command(0x03);
	//first let us get the data from the page to the cache memory
	read_page(my_block_number, my_page_number, 5);
	// now let us get the values from the cache memory to our local variable
	get_data(data_read_from_page,num_bytes_in_page+num_spare_bytes_in_page);

	if(verbose)
	{
		#if DEBUG_ONFI
			if(onfi_debug_file)
			{
				onfi_debug_file<<"Reading MSB subpage of page ("<<my_page_number<<" of block "<< my_block_number<<" ):"<<endl;
			}
			else fprintf(stdout,"Reading MSB subpage of page (%d of block %d):\n",my_page_number, my_block_number);
		#else
			fprintf(stdout,"Reading MSB subpage of page (%d of block %d):\n",my_page_number, my_block_number);
		#endif
	}

	//now iterate through each of them to see if they are correct
	for(byte_id = 0;byte_id<(num_bytes_in_page+num_spare_bytes_in_page);byte_id++)
	{
		//we will spit each byte out from here in hex
		if(onfi_data_file)
		{
			onfi_data_file<<data_read_from_page[byte_id];
		}else
		{
			cout<<data_read_from_page[byte_id];
		}
	}

	// just terminate the sequence with a newline
	if(onfi_data_file)
	{
		onfi_data_file<<endl;
	}else
	{
		cout<<endl;
	}

	if(verbose)
	{
		#if DEBUG_ONFI
			if(onfi_debug_file)
			{
				onfi_debug_file<<".. Reading  MSB subpage of page completed"<<endl;
			}
			else fprintf(stdout,".. Reading  MSB subpage of page completed\n");
		#else
			fprintf(stdout,".. Reading  MSB subpage of page completed\n");
		#endif
	}
	fflush(stdout);

	free(data_read_from_page);
}
// this function reads the pages in the block
// .. since the complete data from the block might be too much data, the parameter num_pages can be used to limit the data
// .. the num_pages should indicate the number of pages in block starting from beginning
// .. verbose indicates the debug messages to be printed
void onfi_interface::read_block_data_n_pages(unsigned int my_block_number, uint16_t num_pages,bool bytewise, bool verbose)
{
	uint16_t page_idx = 0;
	for(page_idx = 0;page_idx<num_pages;page_idx++)
	{
		// read_and_spit_page(uint8_t* page_address,uint8_t* data_to_program,bool verbose)
		read_and_spit_page(my_block_number, page_idx, bytewise, verbose);
	}

}

// this function reads the pages in the block
// .. since the complete data from the block might be too much data, the parameter complete_block can be used to limit the data
// .. if complete_block is true, the compete block will be read
// .. if complete_block is false, the indices of the pages to be read cane be provided as an array
// .. if the indices of pages is used, the num_pages should indicate the numb er of pages listed in the array
// .. verbose indicates the debug messages to be printed
void onfi_interface::read_block_data(unsigned int my_block_number, unsigned int my_page_number, bool complete_block,uint16_t* page_indices,uint16_t num_pages,bool verbose)
{

        uint8_t* data_to_program = (uint8_t*) malloc(sizeof(uint8_t)*(num_bytes_in_page+num_spare_bytes_in_page));
        memset(data_to_program,0x00,num_bytes_in_page+num_spare_bytes_in_page);

	if(complete_block)
	{
		// let us program all the pages in the block
		uint16_t page_idx = 0;
		for(page_idx = 0;page_idx<num_pages_in_block;page_idx++)
		{
			// read_and_spit_page(uint8_t* page_address,uint8_t* data_to_program,bool verbose)
			read_and_spit_page(my_block_number, page_idx, verbose);
		}
	}else
	{
		uint16_t curr_page_index = 0;
		uint16_t idx = 0;
		for(idx = 0;idx<num_pages;idx++)
		{
			// let us grab the index from the array
			curr_page_index = page_indices[idx];

			// read_and_spit_page(uint8_t* page_address,uint8_t* data_to_program,bool verbose)
			read_and_spit_page(my_block_number, curr_page_index, verbose);
		}
	}

	free(data_to_program);
}

//  This function read the page specified by the index value in the 
// .. block and puts the read value into the array passed " return_value" as argument
void onfi_interface::read_page_and_return_value(unsigned int my_block_number, unsigned int my_page_number, uint16_t index,uint8_t* return_value,bool including_spare)
{
        memset(return_value,0xff,including_spare?(num_bytes_in_page+num_spare_bytes_in_page):num_bytes_in_page);

	// now read the page with local_address
	read_page(my_block_number, my_page_number, 5);
	// now let us get the values from the cache memory to our local variable
	get_data(return_value,including_spare?(num_bytes_in_page+num_spare_bytes_in_page):num_bytes_in_page);
}

//The SET FEATURES (EFh) command writes the subfeature parameters (P1-P4) to the
// .. specified feature address to enable or disable target-specific features. This command is
// .. accepted by the target only when all die (LUNs) on the target are idle.
// .. the parameters P1-P4 are in data_to_send argument

