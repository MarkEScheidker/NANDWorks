#include "onfi_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <algorithm>
using namespace std;

bool onfi_interface::verify_program_page(unsigned int my_block_number, unsigned int my_page_number,uint8_t* data_to_program,bool verbose)
{
	bool return_value = true;
	//uint16_t num_bytes_to_test = num_bytes_in_page+num_spare_bytes_in_page;
	uint16_t num_bytes_to_test = num_bytes_in_page;

	uint8_t* data_read_from_page = (uint8_t*)malloc(sizeof(uint8_t)*(num_bytes_to_test));

	// let us first reset all the values in the local variables to 0x00
        memset(data_read_from_page,0xff,num_bytes_to_test);
	//first let us get the data from the page to the cache memory
	read_page(my_block_number, my_page_number, 5);
	// now let us get the values from the cache memory to our local variable
	get_data(data_read_from_page,num_bytes_to_test);

	//now iterate through each of them to see if they are correct
	uint16_t byte_id = 0;
	uint16_t fail_count = 0;
	for(byte_id = 0;byte_id<(num_bytes_to_test);byte_id++)
	{
		if(data_read_from_page[byte_id]!=data_to_program[byte_id])
		{
			fail_count++;
			return_value = false;

			if(verbose)
			{
				#if DEBUG_ONFI
					if(onfi_debug_file)
					{
						onfi_debug_file<<"P:"<<std::hex<<byte_id<<","<<std::hex<<0<<","<<std::hex<<data_read_from_page[byte_id]<<endl;
					}
					else fprintf(stdout,"P:%x,%x,%x\n",byte_id,0,data_read_from_page[byte_id]);
				#else
					fprintf(stdout,"P:%x,%x,%x\n",byte_id,0,data_read_from_page[byte_id]);
				#endif
			}
		}
	}
        if(fail_count)
            cout<<"For page "<< my_page_number<<" of block "<< my_block_number
                <<", program operation failed at "<<std::dec<<fail_count<<" bytes ("
                <<std::fixed<<std::setprecision(2)
                <<(static_cast<double>(fail_count)*100.0/num_bytes_to_test)
                <<"% of page)."<<endl;
        else
            cout<<"For page "<< my_page_number<<" of block "<< my_block_number
                <<", program operation did not fail."<<endl;

	fflush(stdout);
	free(data_read_from_page);

	return return_value;
}

// this function only programs a single page as indicated by the address provided
// for the arguments,
// .. page_address is the address of the page 5-byte address
// .. data_to_progra is the data to write
// .. including_spare is to indicate if you want to program the spare section as well
// .. .. if including spare  = 1, then length of data_to_program should be (num of bytes in pages + num of spare bytes)
// .. verbose is for priting messages
void onfi_interface::program_page(unsigned int my_block_number, unsigned int my_page_number, uint8_t* data_to_program, bool including_spare, bool verbose)
{
	uint8_t page_address[5] = {0,0,0,0,0};
	// following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
	convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, page_address);

	enable_erase();

#if DEBUG_ONFI
	if(onfi_debug_file)
	{
		char my_temp[100];
		sprintf(my_temp,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
		onfi_debug_file<<my_temp<<endl;
	}
	else
		fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#else
	if(verbose) fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#endif

	send_command(0x80);
	send_addresses(page_address,5);

	tADL;

	send_data(data_to_program,including_spare?(num_bytes_in_page+num_spare_bytes_in_page):(num_bytes_in_page));

#if PROFILE_TIME
	time_info_file<<"program page: ";
	START_TIME;
#endif
	send_command(0x10);

	tWB;
		// check if it is out of Busy cycle
	while((*jumper_address & hw::RB_mask)==0);
#if PROFILE_TIME
	END_TIME;
	PRINT_TIME;
#endif

	uint8_t status;
	read_status(&status);
	// .. use  the commended code for multi-plane die
	// read_status_enhanced(&status_value,(address+2));
	if(status&0x20)
	{
		if(status&0x01)
		{
			fprintf(stdout,"Failed Program Operation: %d,%d,%d\n",page_address[2],page_address[3],page_address[4]);
		}
		else
		{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation Completed"<<endl;
	else fprintf(stdout,"Program Operation Completed\n");
#else
	if(verbose) fprintf(stdout,"Program Operation Completed\n");
#endif
		}
	}
	else
	{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation, should not be here"<<endl;
	else fprintf(stdout,"Program Operation, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Program Operation, should not be here\n");
#endif
	}

	disable_erase();
}

// provide the subpage number to the page of TLC subpage to program i3 subpages independently
// my subpage number should be eihter 1, 2 or 3
// program all three subpages sequentially, meaning, subpage1, then subpage2 and then subpage3
void onfi_interface::program_page_tlc_toshiba_subpage(unsigned int my_block_number,unsigned int my_page_number, unsigned int my_subpage_number, uint8_t* data_to_program,bool including_spare,bool verbose)
{
	if(flash_chip != toshiba_tlc_toggle)
	{
		fprintf(stdout,"E: Trying to programm using TOSHIBA TLC function. Not allowed.\n");
		exit(-1);
	}
	

	uint8_t page_address[5] = {0,0,0,0,0};
	// following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
	convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, page_address);

	// write program routine here
	enable_erase();

#if DEBUG_ONFI
	if(onfi_debug_file)
	{
		char my_temp[100];
		sprintf(my_temp,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
		onfi_debug_file<<my_temp<<endl;
	}
	else
		fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#else
	if(verbose) fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#endif
	// for first subpage
	send_command((uint8_t)my_subpage_number);
	send_command(0x80);
	send_addresses(page_address,5);

	tADL;

	send_data(data_to_program,including_spare?(num_bytes_in_page+num_spare_bytes_in_page):(num_bytes_in_page));

#if PROFILE_TIME
	time_info_file<<"program page: ";
	START_TIME;
#endif
	if(my_subpage_number<3) send_command(0x1a);
	else send_command(0x10);

	tWB;
		// check if it is out of Busy cycle
	while((*jumper_address & hw::RB_mask)==0);
#if PROFILE_TIME
	END_TIME;
	PRINT_TIME;
#endif

	uint8_t status;
	read_status(&status);
	// .. use  the commended code for multi-plane die
	// read_status_enhanced(&status_value,(address+2));
	if(status&0x20)
	{
		if(status&0x01)
		{
fprintf(stdout,
        "Failed Program Operation of %u subpage: %d,%d,%d\n",
        my_subpage_number,
        page_address[2], page_address[3], page_address[4]);
		}
		else
		{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation Completed of "<<my_subpage_number<<" subpage"<<endl;
        else fprintf(stdout,"Program Operation Completed of %u subpage\n", my_subpage_number);
#else
        if(verbose) fprintf(stdout,"Program Operation Completed of %u subpage\n", my_subpage_number);
#endif
		}
	}
	else
	{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation of "<<my_subpage_number<<" subpage, should not be here"<<endl;
        else fprintf(stdout,"Program Operation of %u subpage, should not be here\n", my_subpage_number);
#else
        if(verbose) fprintf(stdout,"Program Operation of %u subpage, should not be here\n", my_subpage_number);
#endif
	}

	if(my_subpage_number>=3)
	{
		disable_erase();
	}

}

void onfi_interface::program_page_tlc_toshiba(unsigned int my_block_number,unsigned int my_page_number, uint8_t* data_to_program,bool including_spare,bool verbose)
{
	if(flash_chip != toshiba_tlc_toggle)
	{
		fprintf(stdout,"E: Trying to programm using TOSHIBA TLC function. Not allowed.\n");
		exit(-1);
	}
	

	uint8_t page_address[5] = {0,0,0,0,0};
	// following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
	convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, page_address);

	// write program routine here
	enable_erase();

#if DEBUG_ONFI
	if(onfi_debug_file)
	{
		char my_temp[100];
		sprintf(my_temp,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
		onfi_debug_file<<my_temp<<endl;
	}
	else
		fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#else
	if(verbose) fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#endif
	// for first subpage
	send_command(0x01);
	send_command(0x80);
	send_addresses(page_address,5);

	tADL;

	send_data(data_to_program,including_spare?(num_bytes_in_page+num_spare_bytes_in_page):(num_bytes_in_page));

#if PROFILE_TIME
	time_info_file<<"program page: ";
	START_TIME;
#endif
	send_command(0x1a);

	tWB;
		// check if it is out of Busy cycle
	while((*jumper_address & hw::RB_mask)==0);
#if PROFILE_TIME
	END_TIME;
	PRINT_TIME;
#endif

	uint8_t status;
	read_status(&status);
	// .. use  the commended code for multi-plane die
	// read_status_enhanced(&status_value,(address+2));
	if(status&0x20)
	{
		if(status&0x01)
		{
			fprintf(stdout,"Failed Program Operation of first subpage: %d,%d,%d\n",page_address[2],page_address[3],page_address[4]);
		}
		else
		{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation Completed of first subpage"<<endl;
	else fprintf(stdout,"Program Operation Completed of first subpage\n");
#else
	if(verbose) fprintf(stdout,"Program Operation Completed of first subpage\n");
#endif
		}
	}
	else
	{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation of first subpage, should not be here"<<endl;
	else fprintf(stdout,"Program Operation of first subpage, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Program Operation of first subpage, should not be here\n");
#endif
	}

	// for second subpage
	send_command(0x02);
	send_command(0x80);
	send_addresses(page_address,5);

	tADL;

	send_data(data_to_program,including_spare?(num_bytes_in_page+num_spare_bytes_in_page):(num_bytes_in_page));

#if PROFILE_TIME
	time_info_file<<"program page of second subpage: ";
	START_TIME;
#endif
	send_command(0x1a);

	tWB;
		// check if it is out of Busy cycle
	while((*jumper_address & hw::RB_mask)==0);
#if PROFILE_TIME
	END_TIME;
	PRINT_TIME;
#endif

	read_status(&status);
	// .. use  the commended code for multi-plane die
	// read_status_enhanced(&status_value,(address+2));
	if(status&0x20)
	{
		if(status&0x01)
		{
			fprintf(stdout,"Failed Program Operation of second subpage: %d,%d,%d\n",page_address[2],page_address[3],page_address[4]);
		}
		else
		{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation Completed of second subpage"<<endl;
	else fprintf(stdout,"Program Operation Completed of second subpage\n");
#else
	if(verbose) fprintf(stdout,"Program Operation Completed of second subpage\n");
#endif
		}
	}
	else
	{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation of second subpage, should not be here"<<endl;
	else fprintf(stdout,"Program Operation of second subpage, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Program Operation of second subpage, should not be here\n");
#endif
	}

	// for third subpage
	send_command(0x03);
	send_command(0x80);
	send_addresses(page_address,5);

	tADL;

	send_data(data_to_program,including_spare?(num_bytes_in_page+num_spare_bytes_in_page):(num_bytes_in_page));

#if PROFILE_TIME
	time_info_file<<"program page of third subpage: ";
	START_TIME;
#endif
	send_command(0x10);

	tWB;
		// check if it is out of Busy cycle
	while((*jumper_address & hw::RB_mask)==0);
#if PROFILE_TIME
	END_TIME;
	PRINT_TIME;
#endif

	read_status(&status);
	// .. use  the commended code for multi-plane die
	// read_status_enhanced(&status_value,(address+2));
	if(status&0x20)
	{
		if(status&0x01)
		{
			fprintf(stdout,"Failed Program Operation of third subpage: %d,%d,%d\n",page_address[2],page_address[3],page_address[4]);
		}
		else
		{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation Completed of third subpage"<<endl;
	else fprintf(stdout,"Program Operation Completed of third subpage\n");
#else
	if(verbose) fprintf(stdout,"Program Operation Completed of third subpage\n");
#endif
		}
	}
	else
	{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation of third subpage, should not be here"<<endl;
	else fprintf(stdout,"Program Operation of third subpage, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Program Operation of third subpage, should not be here\n");
#endif
	}
	disable_erase();

}
void onfi_interface::partial_program_page(unsigned int my_block_number, unsigned int my_page_number, uint32_t loop_count,uint8_t* data_to_program,bool including_spare,bool verbose)
{
	uint8_t page_address[5] = {0,0,0,0,0};
	// following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
	convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, page_address);

	enable_erase();

#if DEBUG_ONFI
	if(onfi_debug_file)
	{
		char my_temp[100];
		sprintf(my_temp,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
		onfi_debug_file<<my_temp<<endl;
	}
	else
		fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#else
	if(verbose) fprintf(stdout,"Inside Program Fn: Address is: %02x,%02x,%02x,%02x,%02x.",page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#endif

	send_command(0x80);
	send_addresses(page_address,5);

	tADL;

	send_data(data_to_program,including_spare?(num_bytes_in_page+num_spare_bytes_in_page):(num_bytes_in_page));

#if PROFILE_TIME
	time_info_file<<"program page: ";
	START_TIME;
#endif
	send_command(0x10);

	// tWB;
	// let us introduce a delay
	delay_function(loop_count);
		
	// let us issue reset command here
	send_command(0xff);

#if PROFILE_TIME
	END_TIME;
	PRINT_TIME;
#endif
	// check if it is out of Busy cycle
	while((*jumper_address & hw::RB_mask)==0);

	uint8_t status;
	read_status(&status);
	// .. use  the commended code for multi-plane die
	// read_status_enhanced(&status_value,(address+2));
	if(status&0x20)
	{
		if(status&0x01)
		{
			fprintf(stdout,"Failed Program Operation\n");
		}
		else
		{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation Completed"<<endl;
	else fprintf(stdout,"Program Operation Completed\n");
#else
	if(verbose) fprintf(stdout,"Program Operation Completed\n");
#endif
		}
	}
	else
	{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Program Operation, should not be here"<<endl;
	else fprintf(stdout,"Program Operation, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Program Operation, should not be here\n");
#endif
	}

	disable_erase();
}

// let us program 512 pages in the block with all 0s
// the paramters are:
// .. my_test_block_address is the address of the block (starting address)
// .. num_pages is the number of pages in the page_indices array
// .. verbose is for printing
void onfi_interface::program_pages_in_a_block_slc(unsigned int my_block_number, uint16_t num_pages,bool verbose)
{
        uint8_t* data_to_program = (uint8_t*) malloc(sizeof(uint8_t)*(num_bytes_in_page+num_spare_bytes_in_page));
        memset(data_to_program,0x00,num_bytes_in_page+num_spare_bytes_in_page);

	// let us program all the pages in the block
	uint16_t page_idx = 0;
	for(page_idx = 0;page_idx<num_pages;page_idx++)
	{
		// (uint8_t* page_address,uint8_t* data_to_program,bool including_spare,bool verbose)
		program_page(my_block_number, page_idx, data_to_program,true,verbose);
	}
	free(data_to_program);
}

// this function will program the pages in a block with an array provided by user
// .. if the user wants to give an array
// .. .. it is indicated through bool array_provided
// .. .. the actual array is supposed to be in uint8_t* provided_array (lenght should be total number of bytes in a page)
void onfi_interface::program_n_pages_in_a_block(unsigned int my_block_number, uint16_t num_pages,bool array_provided, uint8_t* provided_array,bool verbose)
{
	uint8_t* data_to_program;
	if(array_provided)
	{
		data_to_program = provided_array;
	}else
	{
        data_to_program = (uint8_t*) malloc(sizeof(uint8_t)*(num_bytes_in_page+num_spare_bytes_in_page));
        memset(data_to_program,0x00,num_bytes_in_page+num_spare_bytes_in_page);
	}

	// let us program all the pages in the block
	uint16_t page_idx = 0;
	for(page_idx = 0;page_idx<num_pages;page_idx++)
	{
		// (uint8_t* page_address,uint8_t* data_to_program,bool including_spare,bool verbose)
		program_page(my_block_number,page_idx,data_to_program,true,verbose);
	}
	if(!array_provided)
		free(data_to_program);
}

// let us program pages in the block with all uint8_t* provided_data
// the paramters are:
// .. my_test_block_address is the address of the block (starting address)
// .. complete_block if this is true all the pages in the block will be programmed
// .. page_indices is an array of indices inside the block that we want to program
// .. num_pages is the number of pages in the page_indices array
// .. verbose is for printing
void onfi_interface::program_pages_in_a_block_data(unsigned int my_block_number, uint8_t* provided_data,bool complete_block,uint16_t* page_indices,uint16_t num_pages,bool verbose)
{
	uint8_t* data_to_program = provided_data;

	if(complete_block)
	{
		// let us program all the pages in the block
		uint16_t page_idx = 0;
		for(page_idx = 0;page_idx<num_pages_in_block;page_idx++)
		{
			// (uint8_t* page_address,uint8_t* data_to_program,bool including_spare,bool verbose)
			program_page(my_block_number, page_idx, data_to_program,true,verbose);
		}
	}else
	{
		uint16_t curr_page_index = 0;

		// let us sort the page indices here
		uint16_t idx = 0;
		uint16_t* page_indices_sorted = new uint16_t[num_pages];
		for(idx = 0; idx < num_pages; idx++)
			page_indices_sorted[idx] = page_indices[idx];

		sort(page_indices_sorted,page_indices_sorted+num_pages);

		for(idx = 0;idx<num_pages;idx++)
		{
			// let us grab the index from the array
			// curr_page_index = page_indices[idx];
			curr_page_index = page_indices_sorted[idx];

			program_page(my_block_number, curr_page_index,data_to_program,true,verbose);
		}

		delete[] page_indices_sorted;

	}
}
// let us program pages in the block with all 0s
// the paramters are:
// .. my_test_block_address is the address of the block (starting address)
// .. complete_block if this is true all the pages in the block will be programmed
// .. page_indices is an array of indices inside the block that we want to program
// .. num_pages is the number of pages in the page_indices array
// .. verbose is for printing
void onfi_interface::program_pages_in_a_block(unsigned int my_block_number, bool complete_block,bool data_random,uint16_t* page_indices,uint16_t num_pages, bool verbose)
{
	uint8_t* data_to_program = (uint8_t*) malloc(sizeof(uint8_t)*(num_bytes_in_page+num_spare_bytes_in_page));
	srand(time(0));
	if(data_random)
	{
		for(uint32_t b_idx=0;b_idx<(num_bytes_in_page+num_spare_bytes_in_page);b_idx++)
		{
			data_to_program[b_idx] = (uint8_t)(rand()%255);
		}
	}else
	{
                memset(data_to_program,0x00,num_bytes_in_page+num_spare_bytes_in_page);
	}
	// memset(data_to_program+1000,0x00,sizeof(uint8_t)*1000);
	std::fstream input_data_file;
	input_data_file.open("raw_data_file.txt",std::fstream::out);
	for(int idx = 0;idx < num_bytes_in_page+num_spare_bytes_in_page; idx++)
	{
		input_data_file<<data_to_program[idx];
	}
	input_data_file.close();

	if(complete_block)
	{
		// let us program all the pages in the block
		uint16_t page_idx = 0;
		for(page_idx = 0;page_idx<num_pages_in_block;page_idx++)
		{
			// (uint8_t* page_address,uint8_t* data_to_program,bool including_spare,bool verbose)
			program_page(my_block_number, page_idx,data_to_program,true,verbose);
		}
	}else
	{
		uint16_t curr_page_index = 0;

		// let us sort the page indices here
		uint16_t* page_indices_sorted = new uint16_t[num_pages];
		uint16_t idx = 0;
		for(idx = 0; idx < num_pages; idx++)
			page_indices_sorted[idx] = page_indices[idx];

		sort(page_indices_sorted,page_indices_sorted+num_pages);

		for(idx = 0;idx<num_pages;idx++)
		{
			// let us grab the index from the array
			// curr_page_index = page_indices[idx];
			curr_page_index = page_indices_sorted[idx];

			// (uint8_t* page_address,uint8_t* data_to_program,bool including_spare,bool verbose)
			program_page(my_block_number, curr_page_index, data_to_program, true, verbose);
		}

		delete[] page_indices_sorted;

	}

	free(data_to_program);
}

void onfi_interface::partial_program_pages_in_a_block(unsigned int my_block_number,uint32_t loop_count,bool complete_block,uint16_t* page_indices,uint16_t num_pages,bool verbose)
{
        uint8_t* data_to_program = (uint8_t*) malloc(sizeof(uint8_t)*(num_bytes_in_page+num_spare_bytes_in_page));
        memset(data_to_program,0x00,num_bytes_in_page+num_spare_bytes_in_page);

	if(complete_block)
	{
		// let us program all the pages in the block
		uint16_t page_idx = 0;
		for(page_idx = 0;page_idx<num_pages_in_block;page_idx++)
		{
			// (uint8_t* page_address,uint8_t* data_to_program,bool including_spare,bool verbose)
			partial_program_page(my_block_number, page_idx,loop_count, data_to_program,true,verbose);
		}
	}else
	{
		uint16_t curr_page_index = 0;

		// let us sort the page indices here
		uint16_t* page_indices_sorted = new uint16_t[num_pages];
		uint16_t idx = 0;
		for(idx = 0; idx < num_pages; idx++)
			page_indices_sorted[idx] = page_indices[idx];

		sort(page_indices_sorted,page_indices_sorted+num_pages);

		for(idx = 0;idx<num_pages;idx++)
		{
			// let us grab the index from the array
			// curr_page_index = page_indices[idx];
			curr_page_index = page_indices_sorted[idx];

			partial_program_page(my_block_number, curr_page_index, loop_count,data_to_program,true,verbose);
		}
		delete[] page_indices_sorted;
	}

	free(data_to_program);
}

// let us verify program pages in the block 
// the paramters are:
// .. my_test_block_address is the address of the block (starting address)
// .. complete_block if this is true all the pages in the block will be verified
// .. page_indices is an array of indices inside the block that we want to verify
// .. num_pages is the number of pages in the page_indices array
// .. verbose is for printing
bool onfi_interface::verify_program_pages_in_a_block(unsigned int my_block_number,bool complete_block,uint16_t* page_indices,uint16_t num_pages,bool verbose)
{
	bool return_value = true;
	//uint16_t num_bytes_to_test = num_bytes_in_page+num_spare_bytes_in_page;
	uint16_t num_bytes_to_test = num_bytes_in_page;

        uint8_t* data_to_program = (uint8_t*) malloc(sizeof(uint8_t)*(num_bytes_to_test));
        memset(data_to_program,0x00,num_bytes_to_test);

	if(complete_block)
	{
		// let us program all the pages in the block
		uint16_t page_idx = 0;
		for(page_idx = 0;page_idx<num_pages_in_block;page_idx++)
		{
			// verify_program_page(uint8_t* page_address,uint8_t* data_to_program,bool verbose)
			if(!verify_program_page(my_block_number, page_idx, data_to_program,verbose))
				return_value = false;
		}
	}else
	{
		uint16_t curr_page_index = 0;
		uint16_t idx = 0;
		cout<<"Verifying program operation on "<<std::dec<<num_pages<<" pages"<<endl;
		for(idx = 0;idx<num_pages;idx++)
		{
			// let us grab the index from the array
			curr_page_index = page_indices[idx];

			// verify_program_page(uint8_t* page_address,uint8_t* data_to_program,bool verbose)
			if(!verify_program_page(my_block_number, curr_page_index, data_to_program,verbose))
				return_value = false;
		}

		// delete[] page_indices_sorted;

	}

	free(data_to_program);
	return return_value;
}

// this function goes through the block and verifies program values in all of 
// .. 512 pages in a block
bool onfi_interface::verify_program_pages_in_a_block_slc(unsigned int my_block_number, bool verbose)
{
	bool return_value = true;

        uint8_t* data_to_program = (uint8_t*) malloc(sizeof(uint8_t)*(num_bytes_in_page+num_spare_bytes_in_page));
        memset(data_to_program,0x00,num_bytes_in_page+num_spare_bytes_in_page);

	// let us program all the pages in the block
	uint16_t page_idx = 0;
	for(page_idx = 0;page_idx<(num_pages_in_block/2);page_idx++)
	{
		// verify_program_page(uint8_t* page_address,uint8_t* data_to_program,bool verbose)
		if(!verify_program_page(my_block_number, page_idx, data_to_program,verbose))
			return_value = false;
	}

	free(data_to_program);
	return return_value;
}

// this function reads the single page address provided
// each value is hex while the sequence is terminated by newline character

