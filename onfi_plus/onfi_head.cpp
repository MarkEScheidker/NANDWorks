#include "onfi_head.h"

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <cstdint>
#include <iomanip>

#include <algorithm>    // std::sort

using namespace std;

/**
 finds the number of 1s in the number input
..  the algo iscalled Brain-Kernigham algo
*/
unsigned int find_num_1s(int input_number)
{
	unsigned int count = 0;
	while(input_number)
	{
		input_number &= (input_number-1);
		count++;
	}
	return count;
}

/**
Following is the list of carefull chosen page indices in any block
.. please read "Testing Latency on Shared Pages" in readme.md in "sources/Data" folder
*/
uint16_t num_pages_selected = 100;
uint16_t page_indices_selected[] = {0,2,4,6,8,10,12,14,16,18, //these are the pages at the beginning of the block (they are not shared)
						
						32, 34, 36, 38, 40, 42, 44, 46, 48, 50, // these are the pages at the beginning of shared location
						64, 68, 72, 76, 80, 84, 88, 92, 96, 100, // .. these are MSB pages to the pages above
						
						105, 113, 121, 129, 137, 145, 153, 161, 169, 177, // these are the pages that follow different relation for LSB and MSB pages
						168, 176, 184, 192, 200, 208, 216, 224, 232, 240, // .. these are MSB for above LSB pages
						
						601, 609, 617, 625, 633, 641, 649, 657, 665, 673, // these pages are somewhere in the middle of the block
						664, 672, 680, 688, 696, 704, 712, 720, 728, 736, // .. these pages are MSB to above LSB pages
						
						941, 943, 945, 947, 949, 951, 953, 955, 957, 959, // .. these are at the tail of shared pages
						1004, 1006, 1008, 1010, 1012, 1014, 1016, 1018, 1020, 1022, // .. these are MSB to above LSB pages
						
						1003, 1005, 1009, 1011, 1013, 1015, 1017, 1019, 1021, 1023}; // .. these are unshared pages at the tail of the block


void onfi_interface::get_started(param_type ONFI_OR_JEDEC)
{
	bool bytewise = true;
	initialize_onfi();

	/**now that the object is created
	.. the object should go and initialize itself by looking at the addresses and map to them
	.. let us test if the LEDs test will succeed
	*/
	test_onfi_leds();

#if PROFILE_TIME
	open_time_profile_file();
#endif

	/**for the NAND chip, let us initialize the chip
	.. this should follow a pattern of power cycle as mentioned in the datasheet
	.. then we will do a reset operation
	*/
	device_initialization();
	read_id();

	/**
	now let test the NAND chip and print its properties as read from the chip memory itself
	.. please make sure that the pin connections is appropriate as mentioned in hardware_locations.h
	.. this function call is also important to establish the parameters like:
	.. onfi_instance.num_bytes_in_page, onfi_instance.num_spare_bytes_in_page and num_pages_in_block
	*/
	if(flash_chip == toshiba_tlc_toggle)
	{
		ONFI_OR_JEDEC=JEDEC;
		bytewise = false;
	}

	read_parameters(ONFI_OR_JEDEC,bytewise,true);
}

/**
	this function opens the debug file 
	.. debug file is used to log the debug messages
	.. the file is only opened when DEBUG_ONFI is true
	.. unless otherwise, it is suggested to set DEBUG_ONFI to false
*/
void onfi_interface::open_onfi_debug_file()
{
#if DEBUG_ONFI
	onfi_debug_file.open("onfi_debug_log.txt",std::fstream::out);
#endif
}

// open the data file
void onfi_interface::open_onfi_data_file()
{
	onfi_data_file.open("data_file.txt",std::fstream::out);
}

// this function can be used to test the status of flash operation
void onfi_interface::check_status()
{	
	// 0x70 is the command for getting status register value
	send_command(0x70);
	uint8_t status;
	get_data(&status,1);

	if(status&0x01)
	{
		if(onfi_debug_file)
			onfi_debug_file<<"I: Last Operation failed"<<endl;
		else
			cout<<"I: Last Operation failed"<<endl;
	}
}

void onfi_interface::wait_on_status()
{	
	// 0x70 is the command for getting status register value
	send_command(0x70);
	uint8_t status;
	get_data(&status,1);

	// wait until the ARDY bit is 1
	while((status&0x20)==0x0)
	{

	}
	if(status&0x01)
	{
		if(onfi_debug_file)	onfi_debug_file<<"I: Last Operation failed"<<endl;
		else cout<<"I: Last Operation failed"<<endl;
	}
}

//let us create a function that performs initialization
// .. opens ONFI debug file
// .. this function gets the bridge based address
// .. converts the peripheral addresses
// .. tests the LEDs
void onfi_interface::initialize_onfi(bool verbose)
{
	open_interface_debug_file();
	//open the ONFI debug file
	open_onfi_debug_file();
	// open the data file
	open_onfi_data_file();

	//let us get the base address of the bridge
	fd = -1;
	bridge_base_virtual = get_base_bridge(&fd);
	if(!bridge_base_virtual)
	{
#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"E: Converting to virtual failed"<<endl;
	else
		cout<<"E: Converting to virtual failed"<<endl;
#else
	if(verbose) cout<<"E: Converting to virtual failed"<<endl;
#endif
	}else
	{
#if DEBUG_ONFI
        if(onfi_debug_file)
                onfi_debug_file<<"I: Converting to virtual successful. The base address is "<<std::hex<<reinterpret_cast<uintptr_t>(bridge_base_virtual)<<endl;
        else
                cout<<"I: Converting to virtual successful. The base address is "<<std::hex<<reinterpret_cast<uintptr_t>(bridge_base_virtual)<<endl;
#else
        if(verbose) cout<<"I: Converting to virtual successful. The base address is "<<std::hex<<reinterpret_cast<uintptr_t>(bridge_base_virtual)<<endl;
#endif
	}
	// let us convert the address of the peripherals
	convert_peripheral_address(bridge_base_virtual);

#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: Successful conversion of all the peripheral addresses"<<endl;
	else
		cout<<"I: Successful conversion of all the peripheral addresses"<<endl;
#else
	cout<<"I: Successful conversion of all the peripheral addresses"<<endl;
#endif
}

void onfi_interface::deinitialize_onfi(bool verbose)
{
	unmap_physical(bridge_base_virtual,LW_BRIDGE_SPAN,verbose);
	close_physical(fd,verbose);
}
// this function can be used to test the LEDs if they are properly set up
//  since it is called as a part of onfi_interface, if this works
// .. the interface is set up properly
void onfi_interface::test_onfi_leds(bool verbose)
{
#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: Testing LEDs"<<endl;
	else
		cout<<"I: Testing LEDs"<<endl;
#else
	if(verbose) cout<<"I: Testing LEDs"<<endl;
#endif
	//just a simple delay for LEDs to stay ON
	turn_leds_on();
	uint32_t delay_ = 20000;
	while(delay_--);
	turn_leds_off();

#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: Testing LEDs completed"<<endl;
	else
		cout<<"I: Testing LEDs completed"<<endl;
#else
	if(verbose) cout<<"I: Testing LEDs completed"<<endl;
#endif
}

// function to receive data from the NAND device
// .. data is output from the cache regsiter of selected die
// .. it is supported following a read operation of NAND array
void onfi_interface::get_data(uint8_t* data_received,uint16_t num_data)
{
	if (interface_type==asynchronous)
	{
		set_default_pin_values();
		set_datalines_direction_input();

		// .. data can be received when on ready state (RDY signal)
		// .. ensure RDY is high
		// .. .. just keep spinning here checking for ready signal
		while((*jumper_address & RB_mask)== 0x00);

		// .. data can be received following READ operation
		// .. the procedure should be as follows
		// .. .. CE should be low
		*jumper_address &= ~CE_mask;
		// .. make WE high
		// .. .. WE should be high from before
		// .. .. ALE and CLE should be low
		// .. .. they should be low from before
		uint16_t i=0;
		for( i=0;i<num_data;i++)
		{			
			// #if PROFILE_TIME

			// 	fprintf(stderr,"Obtaining data from cache Follows\n");
			// 	START_TIME;
			// #endif
			// set the RE to low for next cycle
			*jumper_address &= ~RE_mask;

			// tREA = 40ns
			SAMPLE_TIME;
			asm("NOP");
			asm("NOP");

			// read the data
			data_received[i] = *jumper_address & DQ_mask;

			// .. data is available at DQ pins on the rising edge of RE pin (RE is also input to NAND)
			*jumper_address |= RE_mask;
			
			// #if PROFILE_TIME
			// 	fprintf(stderr,".. time profile from get_data() one word: ");
			// 	END_TIME;
			// 	PRINT_TIME;
			// #endif	
			// tREH
			asm("NOP");
	 // same same 
		}

		// set the pins as output
		set_datalines_direction_default();
		//make sure to call set_default_pin_values()
		set_default_pin_values();
	}else if (interface_type==toggle)
	{
		if(num_data%2)
		{
			printf("E: In TOGGLE mode, num_data for data out cycle must be even number (currently is %d).\n",num_data);
			// printf(".. E: Program will terminate.\n");
			// exit(-1);
		}
		set_default_pin_values();
		set_datalines_direction_input();

		*jumper_address &= ~CE_mask;
		asm("nop");

		// now take RE# to low
		*jumper_address &= ~RE_mask;
		// now take DQS to low
		// set DQS as output
		*jumper_address |= DQS_mask;
		*jumper_direction |= (DQS_mask+DQSc_mask);
		*jumper_address &= ~DQS_mask;
		asm("nop");
		*jumper_address |= RE_mask;
		asm("nop");
		// now in a loop read the data
		uint16_t i=0;
		for( i=0;i<num_data;i+=1)
		{
			// read the data
			data_received[i] = *jumper_address & DQ_mask;

			*jumper_address ^= RE_mask;
			*jumper_address ^= DQS_mask;
		}
		*jumper_address |= RE_mask;

		// set the pins as output
		set_datalines_direction_default();
		//make sure to call set_default_pin_values()
		set_default_pin_values();
	}
	else
	{

	}
}

// function to initialize the NAND device
// .. following are the tasks to be performed for initialization of NAND device
// .. provide Vcc (ie ramp Vcc)
// .. host must wait for R/B to be valid (R/B should be low for certain duration and be high
// .. .. to indicate device is ready) R/B is output from the NAND device
// .. issue 0xFF command after R/B goes high (Reset is the first command to be issued)
// .. R/B should be monotired again after issuing 0XFF command
void onfi_interface::device_initialization(bool verbose)
{
#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: Initializing device with a reset cycle"<<endl;
	else
		cout<<"I: Initializing device with a reset cycle"<<endl;
#else
	if(verbose) cout<<"I: Initializing device with a reset cycle"<<endl;
#endif	
	set_pin_direction_inactive();
	set_default_pin_values();

	// we need to set CE to low for RESET to work
	set_ce_low();

	//insert delay here
	uint16_t i=0;
	for(i=0;i<90;i++);	//50 us max

	// wait for R/B signal to go high
	while((*jumper_address & RB_mask)==0);

	// now issue RESET command
#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: .. initiating a reset cycle"<<endl;
	else
		cout<<"I: .. initiating a reset cycle"<<endl;
#else
	if(verbose) cout<<"I: .. initiating a reset cycle"<<endl;
#endif	
	reset_device();
#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: .. .. reset cycle done"<<endl;
	else
		cout<<"I: .. .. reset cycle done"<<endl;
#else
	if(verbose) cout<<"I: .. .. reset cycle done"<<endl;
#endif
}

// function to reset the whole device
// .. issue a command 0xFF for reset operation
// .. following operations should be performed for this
// .. .. enable command cycle
// .. .. command to be sent is 0xFF
// .. .. check for R/B signal to be high after certain duration (should go low(busy) and go high (ready))
void onfi_interface::reset_device()
{
	// oxff is reset command
	send_command(0xff);
	// no address is expected for reset command
	// wait for busy signal again
	
	//insert delay here
	// .. not needed because the next polling statement will take care
	// .. polling the Ready/BUSY signal
	// .. but we should wait for tWB = 200ns before the RB signal is valid
	tWB;	// tWB = 200ns

	while((*jumper_address & RB_mask)==0);
}

// read id of the device
void onfi_interface::read_id()
{
	uint8_t num_bytes;
	uint8_t address_to_read;
	
	// let us read the unique ID programmed into the device
	num_bytes = 64;
	address_to_read = 0x0;
	send_command(0xED);
	send_addresses(&address_to_read);
	uint8_t* my_unique_id = (uint8_t*)(malloc(num_bytes*sizeof(uint8_t)));
	tRR;
	//wait for RB# signal to be high
	while((*jumper_address & RB_mask)==0);	
	tRR;

	get_data(my_unique_id,num_bytes);
	printf("-------------------------------------------------\n");
	printf("The Unique ID is: ");
	for(uint8_t idx = 0;idx<num_bytes;idx++)
	{
		printf("0x%x ,", my_unique_id[idx]);
	}
	printf("\n");
	free(my_unique_id);
	printf("-------------------------------------------------\n");

	// let us read the ID at address 00
	num_bytes = 8;
	address_to_read = 0x0;
	send_command(0x90);
	send_addresses(&address_to_read);
	uint8_t* my_00_address = (uint8_t*)(malloc(num_bytes*sizeof(uint8_t)));
	tWHR;
	get_data(my_00_address,num_bytes);
	printf("-------------------------------------------------\n");
	printf("The ID at 0x00 is: ");
	for(uint8_t idx = 0;idx<num_bytes;idx++)
	{
		if((my_00_address[idx]>='a' && my_00_address[idx]<='z') ||(my_00_address[idx]>='A' && my_00_address[idx]<='Z') )
			printf("%c ,", my_00_address[idx]);
		else
			printf("0x%x ,", my_00_address[idx]);
	}
	printf("\n");
	printf("-------------------------------------------------\n");

	// let us read the ID at address 20
	num_bytes = 4;
	address_to_read = 0x20;
	send_command(0x90);
	send_addresses(&address_to_read);
	uint8_t* my_20_address = (uint8_t*)(malloc(num_bytes*sizeof(uint8_t)));
	tWHR;
	get_data(my_20_address,num_bytes);
	printf("-------------------------------------------------\n");
	printf("The ID at 0x20 is: ");
	for(uint8_t idx = 0;idx<num_bytes;idx++)
	{
		if((my_20_address[idx]>='a' && my_20_address[idx]<='z') ||(my_20_address[idx]>='A' && my_20_address[idx]<='Z') )
			printf("%c ,", my_20_address[idx]);
		else
			printf("0x%x ,", my_20_address[idx]);
	}
	printf("\n");
	free(my_20_address);
	printf("-------------------------------------------------\n");

	// let us read the ID at address 40
	num_bytes = 6;
	address_to_read = 0x40;
	send_command(0x90);
	send_addresses(&address_to_read);
	uint8_t* my_40_address = (uint8_t*)(malloc(num_bytes*sizeof(uint8_t)));
	tWHR;
	get_data(my_40_address,num_bytes);
	printf("-------------------------------------------------\n");
	printf("The ID at 0x40 is: ");
	for(uint8_t idx = 0;idx<num_bytes;idx++)
	{
		if((my_40_address[idx]>='a' && my_40_address[idx]<='z') ||(my_40_address[idx]>='A' && my_40_address[idx]<='Z') )
			printf("%c ,", my_40_address[idx]);
		else
			printf("0x%x ,", my_40_address[idx]);
	}
	printf("\n");
	free(my_40_address);
	printf("-------------------------------------------------\n");

	//this is where we determine if the default interface is asynchronous or toggle
	// .. ths is for TOSHIBA toggle chips
	if(my_00_address[0]==0x98)	// this is for TOSHIBA chips
	{
		printf("Detected TOSHIBA");
		if((my_00_address[5]&0x80))	// this is for TOGGLE
		{
			printf(" TOGGLE");
			interface_type = toggle;	// this will affect DIN and DOUT cycles
			if(((my_00_address[2]>>2)&0x02)==0x02) // this is for TLC
			{
				printf(" TLC");
				flash_chip = toshiba_tlc_toggle;	// this will affect how we program pages
			}
		}
		printf(" Chip\n.");
	}else if (my_00_address[0]==0x2c)	// this is for Micron
	{
		printf("Detected MICRON");
		if(((my_00_address[2]>>2)&0x02)==0x02) // this is for TLC
		{
			printf(" TLC");
			flash_chip = micron_tlc;	// this will affect how we program pages
		}else if (((my_00_address[2]>>2)&0x01)==0x01) // this is for MLC
		{
			printf(" MLC");
			flash_chip = micron_mlc;	// this will affect how we program pages
		}else if (((my_00_address[2]>>2)&0x02)==0x00) // this is for SLC
		{
			// flash chip will be default type
			printf(" SLC");
		}
		printf(" Chip \n");
	}else
	{
		printf("Detected Asynchronous NAND Flash Chip.\n");
	}
	free(my_00_address);
}

// parameter page_number is the global page number in the chip
// my_test_block_address is an array [c1,c2,r1,r2,r3]
void onfi_interface::convert_pagenumber_to_columnrow_address(unsigned int my_block_number, unsigned int my_page_number, uint8_t* my_test_block_address)
{

#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: Converting block: "<<my_block_number<<" and page: "<<my_page_number<<" to {col,col,row,row,row} format."<<endl;
	else
		cout<<"I: Converting block: "<<my_block_number<<" and page: "<<my_page_number<<" to {col,col,row,row,row} format."<<endl;
#else
	if(verbose) cout<<"I: Converting block: "<<my_block_number<<" and page: "<<my_page_number<<" to {col,col,row,row,row} format."<<endl;
#endif

	unsigned int page_number = (my_block_number*num_pages_in_block+my_page_number);

	for(uint8_t i = 0;i<num_column_cycles; i++)
	{
		my_test_block_address[i] = 0;
	}
	for(uint8_t i = num_column_cycles; i<(num_column_cycles+num_row_cycles); i++)
	{
		my_test_block_address[i] = (page_number&0xff);
		page_number = page_number>>8;
	}
#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: .. converted to "<<(int)my_test_block_address[0]<<","<<(int)my_test_block_address[1]<<","<<(int)my_test_block_address[2]<<","<<(int)my_test_block_address[3]<<","<<(int)my_test_block_address[4]<<endl;
	else
		cout<<"I: .. converted to "<<(int)my_test_block_address[0]<<","<<(int)my_test_block_address[1]<<","<<(int)my_test_block_address[2]<<","<<(int)my_test_block_address[3]<<","<<(int)my_test_block_address[4]<<endl;
#else
	if(verbose) cout<<"I: .. converted to "<<(int)my_test_block_address[0]<<","<<(int)my_test_block_address[1]<<","<<(int)my_test_block_address[2]<<","<<(int)my_test_block_address[3]<<","<<(int)my_test_block_address[4]<<endl;
#endif
}

// following function reads the ONFI parameter page
// .. the ONFI parameter page consists of ONFI parameter data structure
// .. this data structure consists of critical information about the internal organization of memory
// .. the data structure is 256 bytes
// .. the data will be available for read with a command sequence of ECh followed by an address of 40h
void onfi_interface::read_parameters(param_type ONFI_OR_JEDEC, bool bytewise, bool verbose)
{

	// make sure none of the LUNs are busy
	while((*jumper_address & RB_mask)==0);

	uint8_t address_to_send = 0x00;
	std::string type_parameter = "ONFI";
	if(ONFI_OR_JEDEC==JEDEC)
	{
		address_to_send = 0x40;
		type_parameter = "JEDEC";
	}

#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: Reading "<<type_parameter<<" parameters"<<endl;
	else
		cout<<"I: Reading "<<type_parameter<<" parameters"<<endl;
#else
	if(verbose) cout<<"I: Reading "<<type_parameter<<" parameters"<<endl;
#endif

	// make sure none of the LUNs are busy
	while((*jumper_address & RB_mask)==0);

#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: .. sending command"<<endl;
	else
		cout<<"I: .. sending command"<<endl;
#else
	if(verbose) cout<<"I: .. sending command"<<endl;
#endif
	// read ID command
	send_command(0xEC);
	// send address 00

#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: .. sending address"<<endl;
	else
		cout<<"I: .. sending address"<<endl;
#else
	if(verbose) cout<<"I: .. sending address"<<endl;
#endif
	send_addresses(&address_to_send);

	//have some delay here and wait for busy signal again before reading the paramters
	asm("nop");
	// make sure none of the LUNs are busy
	while((*jumper_address & RB_mask)==0);

#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: .. acquiring "<<type_parameter<<" parameters"<<endl;
	else
		cout<<"I: .. acquiring "<<type_parameter<<" parameters"<<endl;
#else
	if(verbose) cout<<"I: .. acquiring "<<type_parameter<<" parameters"<<endl;
#endif
	// now read the 256-bytes of data
	uint16_t num_bytes_in_parameters = 256;
	uint8_t ONFI_parameters[num_bytes_in_parameters];
	
	if(not bytewise)
		get_data(ONFI_parameters,num_bytes_in_parameters);

	uint8_t col_address[2] = {0,0};
	printf("-------------------------------------------------\n");
	for(uint16_t idx = 0;idx<num_bytes_in_parameters;idx++)
	{
		if(bytewise)
		{
			col_address[1] = idx/256;
			col_address[0] = idx%256;
			change_read_column(col_address);
			get_data(ONFI_parameters+idx,1);
		}
#if DEBUG_ONFI
		if(idx%20==0)
			printf("\n");
		if((ONFI_parameters[idx]>='a' && ONFI_parameters[idx]<='z') ||(ONFI_parameters[idx]>='A' && ONFI_parameters[idx]<='Z') )
			printf("%c ,", ONFI_parameters[idx]);
		else
			printf("0x%x ,", ONFI_parameters[idx]);
#endif
	}
	printf("\n");
	printf("-------------------------------------------------\n");

#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: .. acquired "<<type_parameter<<" parameters"<<endl;
	else
		cout<<"I: .. acquired "<<type_parameter<<" parameters"<<endl;
#else
	if(verbose) cout<<"I: .. acquired "<<type_parameter<<" parameters"<<endl;
#endif

	uint8_t ret_whole,ret_decimal;

	// following function is irrelevant for JEDEC Page
	decode_ONFI_version(ONFI_parameters[4],ONFI_parameters[5],&ret_whole,&ret_decimal);

	set_page_size(ONFI_parameters[83],ONFI_parameters[82],ONFI_parameters[81],ONFI_parameters[80]);
	set_page_size_spare(ONFI_parameters[85],ONFI_parameters[84]);
	set_block_size(ONFI_parameters[95],ONFI_parameters[94],ONFI_parameters[93],ONFI_parameters[92]);
	set_lun_size(ONFI_parameters[99],ONFI_parameters[98],ONFI_parameters[97],ONFI_parameters[96]);
		
	num_column_cycles = (ONFI_parameters[101]&0xf0)>>4;
	num_row_cycles = (ONFI_parameters[101]&0x0f);


	// now that we have read the paramters, we must decode the read values
#if DEBUG_ONFI
	if(onfi_debug_file)
	{
		char my_temp[400];

		sprintf(my_temp,"Printing information from the %s paramters\n",type_parameter.c_str());
		onfi_debug_file<<my_temp;

		sprintf(my_temp,".. The signature obtained from first 4-bytes are %c%c%c%c\n", ONFI_parameters[0], ONFI_parameters[1], ONFI_parameters[2], ONFI_parameters[3]);
		onfi_debug_file<<my_temp;

		sprintf(my_temp,".. Bytes 4 and 5 indicate the %s version supported: The maximum supported version is %c.%c\n",type_parameter.c_str(),ret_whole,ret_decimal);
		onfi_debug_file<<my_temp;

		sprintf(my_temp,".. Bytes 32 to 43 gives the manufacturer information: \"%c%c%c%c%c%c%c%c%c%c%c%c\"\n",ONFI_parameters[32],ONFI_parameters[33],ONFI_parameters[34],ONFI_parameters[35],ONFI_parameters[36],ONFI_parameters[37],ONFI_parameters[38],ONFI_parameters[39],ONFI_parameters[40],ONFI_parameters[41],ONFI_parameters[42],ONFI_parameters[43]);
		onfi_debug_file<<my_temp;

		sprintf(my_temp,".. Bytes 44 to 63 gives the device model: \"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\"\n",ONFI_parameters[44],ONFI_parameters[45],ONFI_parameters[46],ONFI_parameters[47],ONFI_parameters[48],ONFI_parameters[49],ONFI_parameters[50],ONFI_parameters[51],ONFI_parameters[52],ONFI_parameters[53],ONFI_parameters[54],ONFI_parameters[55],ONFI_parameters[56],ONFI_parameters[57],ONFI_parameters[58],ONFI_parameters[59],ONFI_parameters[60],ONFI_parameters[61],ONFI_parameters[62],ONFI_parameters[63]);
		onfi_debug_file<<my_temp;

		sprintf(my_temp,".. Bytes 80-83 gives the number of bytes per page: 0x%02x%02x%02x%02x (%d bytes per page)\n",ONFI_parameters[83],ONFI_parameters[82],ONFI_parameters[81],ONFI_parameters[80],num_bytes_in_page);
		onfi_debug_file<<my_temp;

		sprintf(my_temp,".. Bytes 84-85 gives the number of spare bytes per page: 0x%02x%02x (%d spare bytes per page)\n",ONFI_parameters[85],ONFI_parameters[85],num_spare_bytes_in_page);
		onfi_debug_file<<my_temp;
		
		sprintf(my_temp,".. Bytes 92-95 gives the number of pages in a block: 0x%02x%02x%02x%02x (%d pages in a block)\n", ONFI_parameters[95], ONFI_parameters[94], ONFI_parameters[93], ONFI_parameters[92],num_pages_in_block);
		onfi_debug_file<<my_temp;

		sprintf(my_temp,".. Bytes 96-99 gives the number of blocks in a LUN: 0x%02x%02x%02x%02x (%d blocks in a LUN)\n",ONFI_parameters[99],ONFI_parameters[98],ONFI_parameters[97],ONFI_parameters[96], num_blocks);
		onfi_debug_file<<my_temp;

		sprintf(my_temp,".. Byte 100 gives the number of LUNs per chip enable: %d\n", ONFI_parameters[100]);
		onfi_debug_file<<my_temp;

		sprintf(my_temp,".. Byte 102 gives information on how many bits are per cell: %d bits per cell\n", ONFI_parameters[102]);
		onfi_debug_file<<my_temp;

		sprintf(my_temp,".. Byte 180 gives read-retry levels supported: %d levels\n", ONFI_parameters[180]&0x0f);
		onfi_debug_file<<my_temp;

		sprintf(my_temp,".. Number of Colum Cycles is: %d and number of Row Cycles is %d\n", num_column_cycles,num_row_cycles);
		onfi_debug_file<<my_temp;

		onfi_debug_file<<"***********************************************";
		onfi_debug_file<<endl;
	}else
	{
		fprintf(stdout,"Printing information from the %s paramters\n",type_parameter.c_str());
		fprintf(stdout,".. The signature obtained from first 4-bytes are %c%c%c%c\n", ONFI_parameters[0], ONFI_parameters[1], ONFI_parameters[2], ONFI_parameters[3]);
		fprintf(stdout,".. Bytes 4 and 5 indicate the %s version supported: The maximum supported version is %c.%c\n",type_parameter.c_str(),ret_whole,ret_decimal);

		fprintf(stdout,".. Bytes 32 to 43 gives the manufacturer information: \"%c%c%c%c%c%c%c%c%c%c%c%c\"\n",ONFI_parameters[32],ONFI_parameters[33],ONFI_parameters[34],ONFI_parameters[35],ONFI_parameters[36],ONFI_parameters[37],ONFI_parameters[38],ONFI_parameters[39],ONFI_parameters[40],ONFI_parameters[41],ONFI_parameters[42],ONFI_parameters[43]);
		fprintf(stdout,".. Bytes 44 to 63 gives the device model: \"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\"\n",ONFI_parameters[44],ONFI_parameters[45],ONFI_parameters[46],ONFI_parameters[47],ONFI_parameters[48],ONFI_parameters[49],ONFI_parameters[50],ONFI_parameters[51],ONFI_parameters[52],ONFI_parameters[53],ONFI_parameters[54],ONFI_parameters[55],ONFI_parameters[56],ONFI_parameters[57],ONFI_parameters[58],ONFI_parameters[59],ONFI_parameters[60],ONFI_parameters[61],ONFI_parameters[62],ONFI_parameters[63]);

		fprintf(stdout,".. Bytes 80-83 gives the number of bytes per page: 0x%02x%02x%02x%02x (%d bytes per page)\n",ONFI_parameters[83],ONFI_parameters[82],ONFI_parameters[81],ONFI_parameters[80],num_bytes_in_page);

		fprintf(stdout,".. Bytes 84-85 gives the number of spare bytes per page: 0x%02x%02x (%d spare bytes per page)\n",ONFI_parameters[85],ONFI_parameters[85],num_spare_bytes_in_page);
		
		fprintf(stdout,".. Bytes 92-95 gives the number of pages in a block: 0x%02x%02x%02x%02x (%d pages in a block)\n", ONFI_parameters[95], ONFI_parameters[94], ONFI_parameters[93], ONFI_parameters[92],num_pages_in_block);
		fprintf(stdout,".. Bytes 96-99 gives the number of blocks in a LUN: 0x%02x%02x%02x%02x\n",ONFI_parameters[99],ONFI_parameters[98],ONFI_parameters[97],ONFI_parameters[96]);
		fprintf(stdout,".. Byte 100 gives the number of LUNs per chip enable: %d\n", ONFI_parameters[100]);
		fprintf(stdout,".. Byte 180 gives read-retry levels supported: %d levels\n", ONFI_parameters[179]&0x0f);
		fprintf(stdout,".. Byte 181-184 gives available levels: %02x,%02x,%02x and %02x levels\n", ONFI_parameters[180],ONFI_parameters[181],ONFI_parameters[182],ONFI_parameters[183]);
		
		fprintf(stdout,".. Byte 102 gives information on how many bits are per cell: %d bits per cell\n", ONFI_parameters[102]);
		fprintf(stdout,".. Byte 180 gives read-retry levels supported: %d levels\n", ONFI_parameters[180]&0x0f);
		fprintf(stdout,".. Number of Colum Cycles is: %d and number of Row Cycles is %d\n", num_column_cycles,num_row_cycles);
		fprintf(stdout,"***********************************************\n");
	}
#else
if(verbose) 
{
		fprintf(stdout,"Printing information from the %s paramters\n",type_parameter.c_str());
		fprintf(stdout,".. The signature obtained from first 4-bytes are %c%c%c%c\n", ONFI_parameters[0], ONFI_parameters[1], ONFI_parameters[2], ONFI_parameters[3]);
		fprintf(stdout,".. Bytes 4 and 5 indicate the %s version supported: The maximum supported version is %c.%c\n",type_parameter.c_str(),ret_whole,ret_decimal);

		fprintf(stdout,".. Bytes 32 to 43 gives the manufacturer information: \"%c%c%c%c%c%c%c%c%c%c%c%c\"\n",ONFI_parameters[32],ONFI_parameters[33],ONFI_parameters[34],ONFI_parameters[35],ONFI_parameters[36],ONFI_parameters[37],ONFI_parameters[38],ONFI_parameters[39],ONFI_parameters[40],ONFI_parameters[41],ONFI_parameters[42],ONFI_parameters[43]);
		fprintf(stdout,".. Bytes 44 to 63 gives the device model: \"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\"\n",ONFI_parameters[44],ONFI_parameters[45],ONFI_parameters[46],ONFI_parameters[47],ONFI_parameters[48],ONFI_parameters[49],ONFI_parameters[50],ONFI_parameters[51],ONFI_parameters[52],ONFI_parameters[53],ONFI_parameters[54],ONFI_parameters[55],ONFI_parameters[56],ONFI_parameters[57],ONFI_parameters[58],ONFI_parameters[59],ONFI_parameters[60],ONFI_parameters[61],ONFI_parameters[62],ONFI_parameters[63]);

		fprintf(stdout,".. Bytes 80-83 gives the number of bytes per page: 0x%02x%02x%02x%02x (%d bytes per page)\n",ONFI_parameters[83],ONFI_parameters[82],ONFI_parameters[81],ONFI_parameters[80],num_bytes_in_page);

		fprintf(stdout,".. Bytes 84-85 gives the number of spare bytes per page: 0x%02x%02x (%d spare bytes per page)\n",ONFI_parameters[85],ONFI_parameters[85],num_spare_bytes_in_page);
		
		fprintf(stdout,".. Bytes 92-95 gives the number of pages in a block: 0x%02x%02x%02x%02x (%d pages in a block)\n", ONFI_parameters[95], ONFI_parameters[94], ONFI_parameters[93], ONFI_parameters[92],num_pages_in_block);
		fprintf(stdout,".. Bytes 96-99 gives the number of blocks in a LUN: 0x%02x%02x%02x%02x\n",ONFI_parameters[99],ONFI_parameters[98],ONFI_parameters[97],ONFI_parameters[96]);
		fprintf(stdout,".. Byte 100 gives the number of LUNs per chip enable: %d\n", ONFI_parameters[100]);
		fprintf(stdout,".. Byte 180 gives read-retry levels supported: %d levels\n", ONFI_parameters[179]&0x0f);
		fprintf(stdout,".. Byte 181-184 gives available levels: %02x,%02x,%02x and %02x levels\n", ONFI_parameters[180],ONFI_parameters[181],ONFI_parameters[182],ONFI_parameters[183]);
		fprintf(stdout,".. Byte 102 gives information on how many bits are per cell: %d bits per cell\n", ONFI_parameters[102]);
		fprintf(stdout,".. Byte 180 gives read-retry levels supported: %d levels\n", ONFI_parameters[180]&0x0f);
		fprintf(stdout,".. Number of Colum Cycles is: %d and number of Row Cycles is %d\n", num_column_cycles,num_row_cycles);
		fprintf(stdout,"***********************************************\n");
}
#endif
}

// based on the values read from the ONFI paramters, we will figure out the ONFI version
void onfi_interface::decode_ONFI_version(uint8_t byte_4,uint8_t byte_5,uint8_t* ret_whole,uint8_t* ret_decimal)
{
	if(byte_4&0x02)//test bit9 of [byte_4,byte_5]
	{
		*ret_whole = '4';
		*ret_decimal = '0';
		return;
	}else if(byte_4&0x01)//test bit8 of [byte_4,byte_5]
	{
		*ret_whole = '3';
		*ret_decimal = '2';
		return;
	}else if(byte_5&0x80)//test bit7 of [byte_4,byte_5]
	{
		*ret_whole = '3';
		*ret_decimal = '1';
		return;
	}else if(byte_5&0x40)//test bit6 of [byte_4,byte_5]
	{
		*ret_whole = '3';
		*ret_decimal = '0';
		return;
	}else if(byte_5&0x20)//test bit5 of [byte_4,byte_5]
	{
		*ret_whole = '2';
		*ret_decimal = '3';
		return;
	}else if(byte_5&0x10)//test bit4 of [byte_4,byte_5]
	{
		*ret_whole = '2';
		*ret_decimal = '2';
		return;
	}else if(byte_5&0x8)//test bit3 of [byte_4,byte_5]
	{
		*ret_whole = '2';
		*ret_decimal = '1';
		return;
	}else if(byte_5&0x04)//test bit2 of [byte_4,byte_5]
	{
		*ret_whole = '2';
		*ret_decimal = '0';
		return;
	}else if(byte_5&0x02)//test bit1 of [byte_4,byte_5]
	{
		*ret_whole = '1';
		*ret_decimal = '0';
		return;
	}else
	{
		*ret_whole = 'x';
		*ret_decimal = 'x';
		return;
	}
}


// following function will set the size of page based on value read 
void onfi_interface::set_page_size(uint8_t byte_83,uint8_t byte_82,uint8_t byte_81,uint8_t byte_80)
{
	num_bytes_in_page = (((byte_83*256+byte_82)*256+byte_81)*256+byte_80);
	// for error condition
	if((byte_83==0xff&&byte_82==0xff&&byte_81==0xff&&byte_80==0xff)||(byte_83==0x0&&byte_82==0x0&&byte_81==0x0&&byte_80==0x0))
		num_bytes_in_page = 2048;
}

// following function will set the size of spare bytes in a page based on value read 
void onfi_interface::set_page_size_spare(uint8_t byte_85,uint8_t byte_84)
{
	num_spare_bytes_in_page = (byte_85*256+byte_84);
	if((byte_85==0xff&&byte_84==0xff)||(byte_84==0x0&&byte_85==0x0))
		num_spare_bytes_in_page = 128;
}

// following function will set the number of pages in a block
void onfi_interface::set_block_size(uint8_t byte_95,uint8_t byte_94,uint8_t byte_93,uint8_t byte_92)
{
	num_pages_in_block = ((byte_95*256+byte_94)*256+byte_93)*256+byte_92;
	if((byte_95==0xff&&byte_94==0xff&&byte_93==0xff&&byte_92==0xff)||(byte_95==0x0&&byte_94==0x0&&byte_93==0x0&&byte_92==0x0))
		num_pages_in_block = 64;
}

void onfi_interface::set_lun_size(uint8_t byte_99,uint8_t byte_98,uint8_t byte_97,uint8_t byte_96)
{
	num_blocks = ((byte_99*256+byte_98)*256+byte_97)*256+byte_96;
	if((byte_96==0xff&&byte_97==0xff&&byte_98==0xff&&byte_99==0xff)||(byte_96==0x0&&byte_97==0x0&&byte_98==0x0&&byte_99==0x0))
		num_blocks = 64;
}

// following function tests if the block address sent was a bad block or not
// the ONFI requirement is that the first byte in the spare area of the first page
// .. should have 0x00 in it to indicate if the block is bad
// this function takes in a block address
// .. and reads the first spare byte of the first page to determine if it is a bad block
bool onfi_interface::is_bad_block(unsigned int my_block_number)
{	
	//let us read the first spare byte i.e. byte number 16384 in the first page
	read_page(my_block_number, 0, 5);
	
	//let us change the read column point to the first byte of spare region
	uint8_t new_col[2] = {(uint8_t)(num_bytes_in_page&0xff), (uint8_t)((num_bytes_in_page>>8)&0xff)};
	change_read_column(new_col);

	uint8_t is_valid_data = 0x55;
	get_data(&is_valid_data,1);

	//if the value read is 0x00, then it is a bad block
	if(is_valid_data==0xff)
	{
		//this is perfect 
		// .. so not a bad block
		return false;
	}else if(is_valid_data==0x00)
	{
		//this is a bad block
		return true;
	}else
	{
		fprintf(stderr,"E: Some data found while testing bad block: ");
		// may still not be a bad block
		return false;
	}

	// resinstate the read column address to the 0th byte in the page
	new_col[0] = 0x0;
	new_col[1] = 0x0;
	change_read_column(new_col);
}

// write a function to perform an read operation from NAND flash to cache register
// .. reads one page from the NAND to the cache register
// .. during the read, you can use change_read_column and change_row_address
void onfi_interface::read_page(unsigned int my_block_number, unsigned int my_page_number,uint8_t address_length,bool verbose)
{
	if(address_length != num_column_cycles+num_row_cycles)
	{
		fprintf(stdout,"E: Address Length Mismatch.");
	}
	uint8_t address[address_length];
	convert_pagenumber_to_columnrow_address(my_block_number, my_page_number, address);
	// make sure none of the LUNs are busy
	while((*jumper_address & RB_mask)==0);

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
	while((*jumper_address & RB_mask)==0);
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
void onfi_interface::open_time_profile_file()
{
#if PROFILE_TIME
	time_info_file.open("time_info_file.txt",std::fstream::out);
#endif
}

// follow the following function call by get_data() function call
// .. please change this if the device has multiple dies
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
void onfi_interface::disable_erase()
{
	// check to see if the device is busy
	// .. wait if busy
	while((*jumper_address&RB_mask)==0x00);

	// wp to low
	*jumper_address &= ~WP_mask;	
	
	//insert delay here
	uint8_t i=0;
	for(i=0;i<4;i++);
}

void onfi_interface::enable_erase()
{
	// check to see if the device is busy
	// .. wait if busy
	while((*jumper_address&RB_mask)==0x00);

	// wp to high
	*jumper_address |= WP_mask;
	
	//insert delay here
	tWW;
}

// following function erases the block address provided as the parameter to the function
// .. the address provided should be three 8-bit number array that corresponds to 
// .. .. R1,R2 and R3 for the block to be erased
void onfi_interface::erase_block(unsigned int my_block_number, bool verbose)
{	
	uint8_t my_test_block_address[5] = {0,0,0,0,0};
	// following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
	convert_pagenumber_to_columnrow_address(my_block_number, 0, my_test_block_address);
	uint8_t *row_address = my_test_block_address+2;

	enable_erase();
	*jumper_direction &= ~RB_mask;

	// check if it is out of Busy cycle
	while((*jumper_address & RB_mask)==0);

	send_command(0x60);
	send_addresses(row_address,3);
#if PROFILE_TIME
	time_info_file<<"Erasing block: ";
	START_TIME;
#endif
	send_command(0xd0);

	tWB;
	

	// check if it is out of Busy cycle
	while((*jumper_address & RB_mask)==0);

#if PROFILE_TIME
	END_TIME;
	PRINT_TIME;
#endif	

#if DEBUG_ONFI
	if(onfi_debug_file)
	{
		char my_temp[100];
		sprintf(my_temp,"Inside Erase Fn: Address is: %02x,%02x,%02x.",row_address[0],row_address[1],row_address[2]);
		onfi_debug_file<<my_temp<<endl;
	}
	else
		fprintf(stdout,"Inside Erase Fn: Address is: %02x,%02x,%02x.\n",row_address[0],row_address[1],row_address[2]);
#else
	if(verbose) fprintf(stdout,"Inside Erase Fn: Address is: %02x,%02x,%02x.\n",row_address[0],row_address[1],row_address[2]);
#endif

	// let us read the status register value
	uint8_t status;
	read_status(&status);
	if(status&0x20)
	{
		if(status&0x01)
		{
			fprintf(stdout,"Failed Erase Operation\n");
		}
		else
		{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Erase Operation Completed"<<endl;
	else fprintf(stdout,"Erase Operation Completed\n");
#else
	if(verbose) fprintf(stdout,"Erase Operation Completed\n");
#endif
		}
	}
	else
	{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Erase Operation, should not be here"<<endl;
	else fprintf(stdout,"Erase Operation, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Erase Operation, should not be here\n");
#endif
	}

	disable_erase();
}


// following function erases the block address provided as the parameter to the function
// .. the address provided should be three 8-bit number array that corresponds to 
// .. .. R1,R2 and R3 for the block to be erased
// .. loop_count is the partial erase times, a value of 10 will correspond to 1 ns delay
void onfi_interface::partial_erase_block(unsigned int my_block_number, unsigned int my_page_number, uint32_t loop_count,bool verbose)
{	
	uint8_t my_test_block_address[5];
	convert_pagenumber_to_columnrow_address(my_block_number,my_page_number,my_test_block_address);
	uint8_t *row_address = my_test_block_address+2;

	enable_erase();
	*jumper_direction &= ~RB_mask;

	// check if it is out of Busy cycle
	while((*jumper_address & RB_mask)==0);

	send_command(0x60);
	send_addresses(row_address,3);
#if PROFILE_TIME
	time_info_file<<"Partial Erasing block: ";
	START_TIME;
#endif
	send_command(0xd0);

	// tWB;
	
	delay_function(loop_count);
		
	// let us issue reset command here
	send_command(0xff);

#if PROFILE_TIME
	END_TIME;
	PRINT_TIME;
#endif	

	// check if it is out of Busy cycle
	while((*jumper_address & RB_mask)==0);

#if DEBUG_ONFI
	if(onfi_debug_file)
	{
		char my_temp[100];
		sprintf(my_temp,"Inside Erase Fn: Address is: %02x,%02x,%02x.",row_address[0],row_address[1],row_address[2]);
		onfi_debug_file<<my_temp<<endl;
	}
	else
		fprintf(stdout,"Inside Erase Fn: Address is: %02x,%02x,%02x.\n",row_address[0],row_address[1],row_address[2]);
#else
	if(verbose) fprintf(stdout,"Inside Erase Fn: Address is: %02x,%02x,%02x.\n",row_address[0],row_address[1],row_address[2]);
#endif

	// let us read the status register value
	uint8_t status;
	read_status(&status);
	if(status&0x20)
	{
		if(status&0x01)
		{
			fprintf(stdout,"Failed Erase Operation\n");
		}
		else
		{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Erase Operation Completed"<<endl;
	else fprintf(stdout,"Erase Operation Completed\n");
#else
	if(verbose) fprintf(stdout,"Erase Operation Completed\n");
#endif
		}
	}
	else
	{
#if DEBUG_ONFI
	if(onfi_debug_file) onfi_debug_file<<"Erase Operation, should not be here"<<endl;
	else fprintf(stdout,"Erase Operation, should not be here\n");
#else
	if(verbose) fprintf(stdout,"Erase Operation, should not be here\n");
#endif
	}

	disable_erase();
}


// following function can be used to read the status following any command
void onfi_interface::read_status(uint8_t* status_value)
{
	send_command(0x70);	
	//insert delay here
	// .. tWHR= 120ns
	tWHR;
	get_data(status_value,1);
}

// .. this function will read a random page and tries to verify if it was completely erased
// .. for elaborate verifiying, please use a different function
bool onfi_interface::verify_block_erase_sample(unsigned int my_block_number, bool verbose)
{

	// let us initialize a random seed
	srand(time(NULL));
	//pick a random page index to test to see if erase works
	uint16_t page_num_to_verify = rand()%num_pages_in_block;

	uint8_t my_test_block_address[5] = {0,0,0,0,0};
	// following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
	convert_pagenumber_to_columnrow_address(my_block_number, page_num_to_verify, my_test_block_address);
	uint8_t *page_address = my_test_block_address;


	// just a placeholder for return value
	bool return_value = true;


#if DEBUG_ONFI
	if(onfi_debug_file)
	{
		char my_temp[200];
		sprintf(my_temp,"Verifying erase operation on page no: %d (%02x,%02x,%02x,%02x,%02x)",page_num_to_verify,page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
		onfi_debug_file<<my_temp<<endl;
	}
	else fprintf(stdout,"Verifying erase operation on page no: %d (%02x,%02x,%02x,%02x,%02x)\n",page_num_to_verify,page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#else
	if(verbose) fprintf(stdout,"Verifying erase operation on page no: %d (%02x,%02x,%02x,%02x,%02x)\n",page_num_to_verify,page_address[0],page_address[1],page_address[2],page_address[3],page_address[4]);
#endif

	// let us read from that page and compare each value read from the page to 0xff
	uint8_t* data_read_from_page = new uint8_t[num_bytes_in_page+num_spare_bytes_in_page];
	
	//let us initialize the array to 0x00
        memset(data_read_from_page,0x00,num_bytes_in_page+num_spare_bytes_in_page);

	//let us read the page data to the cache memory of flash
	read_page(my_block_number, page_num_to_verify,5);
	//let us acquire the data from the cache to the local variable
	get_data(data_read_from_page,num_bytes_in_page+num_spare_bytes_in_page);

	//now let us iterate through all the bytes and check
	uint16_t byte_id = 0;
	uint16_t fail_count = 0;
	for(byte_id = 0;byte_id<(num_bytes_in_page+num_spare_bytes_in_page);byte_id++)
	{
		if(data_read_from_page[byte_id]!=0xff)
		{
			fail_count++;
			return_value = false;
			if(verbose)
			{
				#if DEBUG_ONFI
					if(onfi_debug_file)
					{
						onfi_debug_file<<"E:"<<std::hex<<byte_id<<","<<std::hex<<page_num_to_verify<<","<<std::hex<<data_read_from_page[byte_id]<<endl;
					}
					else fprintf(stdout,"E:%x,%x,%x\n",byte_id,page_num_to_verify,data_read_from_page[byte_id]);
				#else
					fprintf(stdout,"E:%x,%x,%x\n",byte_id,page_num_to_verify,data_read_from_page[byte_id]);
				#endif
			}
		}
	}
	if(fail_count) cout<<"The number of bytes in page id "<<page_num_to_verify<<" where erase operation failed is "<<std::dec<<fail_count<<endl;
	else cout<<"Erase operation SUCCESS!!. Tested on page id "<<page_num_to_verify<<endl;

	delete[] data_read_from_page;
	return return_value;
}


// this is the function that can be used to elaborately verify the erase operation
// .. the first argument is the address of the block
// .. the second argument defines if the complete block is to be verified
// .. .. if the second argument is false, then the pages from page_indices_selected are taken
bool onfi_interface::verify_block_erase(unsigned int my_block_number, bool complete_block,uint16_t* page_indices,uint16_t num_pages, bool verbose)
{
	// just a placeholder for return value
	bool return_value = true;
	//uint16_t num_bytes_to_test = num_bytes_in_page+num_spare_bytes_in_page;
	uint16_t num_bytes_to_test = num_bytes_in_page;

	// let us create a local variable that will hold the data read from the pages
	uint8_t* data_read_from_page = new uint8_t[num_bytes_to_test];
	uint16_t idx = 0;

	//test to see if the user wants the complete block or just the selected pages
	if(!complete_block)
	{
		// this means we are operating only on selected pages in the block
		// this means we are working on all the pages in the block
		// .. let us iterate through each pages in the block
		uint16_t curr_page_index = 0;
		for(idx = 0;idx<num_pages;idx++)
		{
			// let us grab the index from the array
			curr_page_index = page_indices[idx];

			// let us first reset all the values in the local variables to 0x00
                        memset(data_read_from_page,0x00,num_bytes_to_test);
			//first let us get the data from the page to the cache memory
			read_page(my_block_number, curr_page_index, 5);
			// now let us get the values from the cache memory to our local variable
			get_data(data_read_from_page,num_bytes_to_test);

			//now iterate through each of them to see if they are correct
			uint16_t byte_id = 0;
			uint16_t fail_count = 0;
			for(byte_id = 0;byte_id<(num_bytes_to_test);byte_id++)
			{
				if(data_read_from_page[byte_id]!=0xff)
				{
					fail_count++;
					return_value = false;

					if(verbose)
					{
						#if DEBUG_ONFI
							if(onfi_debug_file)
							{
								onfi_debug_file<<"E:"<<std::hex<<byte_id<<","<<std::hex<<curr_page_index<<","<<std::hex<<data_read_from_page[byte_id]<<endl;
							}
							else fprintf(stdout,"E:%x,%x,%x\n",byte_id,curr_page_index,data_read_from_page[byte_id]);
						#else
							fprintf(stdout,"E:%x,%x,%x\n",byte_id,curr_page_index,data_read_from_page[byte_id]);
						#endif
					}
				}
			}
			if(fail_count) cout<<"The number of bytes in page id "<<curr_page_index<<" where erase operation failed is "<<std::dec<<fail_count<<endl;
		}
	}else
	{
		// this means we are working on all the pages in the block
		// .. let us iterate through each pages in the block
		for(idx = 0;idx<num_pages_in_block;idx++)
		{
			// let us first reset all the values in the local variables to 0x00
                        memset(data_read_from_page,0x00,num_bytes_to_test);
			//first let us get the data from the page to the cache memory
			read_page(my_block_number, idx, 5);
			// now let us get the values from the cache memory to our local variable
			get_data(data_read_from_page,num_bytes_to_test);

			//now iterate through each of them to see if they are correct
			uint16_t byte_id = 0;
			uint16_t fail_count = 0;
			for(byte_id = 0;byte_id<(num_bytes_to_test);byte_id++)
			{
				if(data_read_from_page[byte_id]!=0xff)
				{
					fail_count++;
					return_value = false;

					if(verbose)
					{
						#if DEBUG_ONFI
							if(onfi_debug_file)
							{
								onfi_debug_file<<"E:"<<std::hex<<byte_id<<","<<std::hex<<idx<<","<<std::hex<<data_read_from_page[byte_id]<<endl;
							}
							else fprintf(stdout,"E:%x,%x,%x\n",byte_id,idx,data_read_from_page[byte_id]);
						#else
							fprintf(stdout,"E:%x,%x,%x\n",byte_id,idx,data_read_from_page[byte_id]);
						#endif
					}
				}
			}
			if(fail_count) cout<<"The number of bytes in page id "<<idx<<" where erase operation failed is "<<std::dec<<fail_count<<endl;
		}
	}
	delete[] data_read_from_page;
	return return_value;
}


// this function only verifies a single page as indicated by the address provided
// for the arguments,
// .. page_address is the address of the page 5-byte address
// .. data_to_progra is the data expected
// .. including_spare is to indicate if you want to program the spare section as well
// .. .. if including spare  = 1, then length of data_to_program should be (num of bytes in pages + num of spare bytes)
// .. verbose is for priting messages
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
	while((*jumper_address & RB_mask)==0);
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
	while((*jumper_address & RB_mask)==0);
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
	while((*jumper_address & RB_mask)==0);
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
	while((*jumper_address & RB_mask)==0);
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
	while((*jumper_address & RB_mask)==0);
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
	while((*jumper_address & RB_mask)==0);

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
void onfi_interface::set_features(uint8_t address, uint8_t* data_to_send,bool verbose, uint8_t command)
{
	// check if it is out of Busy cycle
	while((*jumper_address & RB_mask)==0);

	//send command
	send_command(command);
	//send the address
	send_addresses(&address);

	//now we wait
	tWB;

	// now send the parameters
	send_data(data_to_send,4);

	tWB;

	// check if it is out of Busy cycle
	while((*jumper_address & RB_mask)==0);
}

//The GET FEATURES (EEh) command reads the subfeature parameters (P1-P4) from the
// .. specified feature address. This command is accepted by the target only when all die
// .. (LUNs) on the target are idle.
// .. the parameters P1-P4 are in data_received argument
void onfi_interface::get_features(uint8_t address, uint8_t* data_received,bool verbose, uint8_t command)
{
	// check if it is out of Busy cycle
	while((*jumper_address & RB_mask)==0);

	//send command
	send_command(command);
	//send the address
	send_addresses(&address);

	//now we wait
	tWB;
	// check if it is out of Busy cycle
	while((*jumper_address & RB_mask)==0);
	asm("nop");

	get_data(data_received,4);
}

// following function will convert a block from MLC mode to SLC mode
// .. it uses set_features option to convert the block from SLC to MLC
void onfi_interface::convert_to_slc_set_features(unsigned int my_block_number)
{

	uint8_t my_test_block_address[5] = {0,0,0,0,0};
	// following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
	convert_pagenumber_to_columnrow_address(my_block_number, 0, my_test_block_address);

	// to convert to SLC using set features, we have to first make sure
	// .. at least half the pages are programmed
	// .. let us just program all the pages in the block
	program_pages_in_a_block(my_block_number,true);

	// now that we have programmed the block,
	// .. we will issue set_features command
	uint8_t parameters[4] = {01,01,0,0};
	// let us send the command for feature
	set_features(0x91,parameters);

	// now we must erase the block to initialize in SLC mode
	erase_block(my_block_number);

	// now the block should be in SLC mode
}

void onfi_interface::revert_to_mlc_set_features()
{
	// .. we will issue set_features command
	uint8_t parameters[4] = {02,01,0,0};
	// let us send the command for feature
	set_features(0x91,parameters);
}

void onfi_interface::test_device_voltage_high()
{
	*jumper_direction |= (RE_mask);
	*jumper_address |= (RE_mask);
}


void onfi_interface::test_device_voltage_low()
{
	*jumper_direction |= (RE_mask);
	*jumper_address &= ~(RE_mask);
}

void onfi_interface::convert_to_slc(unsigned int my_block_number, bool first_time)
{

	uint8_t my_test_block_address[5] = {0,0,0,0,0};
	// following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
	convert_pagenumber_to_columnrow_address(my_block_number, 0, my_test_block_address);

	// to convert to SLC, we have to first make sure
	// .. at least half the pages are programmed
	// .. let us just program all the pages in the block
	if(first_time)
	{
		erase_block(my_block_number);
		program_pages_in_a_block(my_block_number,true);
	}
	
	send_command(0xda);
	tWB;
	send_command(0x60);
	send_addresses(my_test_block_address+2);
	send_command(0xd0);
	tWB;
	while((*jumper_address & RB_mask)==0);

	//perform erase operation to init
	if(first_time) erase_block(my_block_number);
}

void onfi_interface::revert_to_mlc(unsigned int my_block_number)
{
	uint8_t my_test_block_address[5] = {0,0,0,0,0};
	// following function converts the my_page_number inside the my_block_number to {x,x,x,x,x} and saves to my_test_block_address
	convert_pagenumber_to_columnrow_address(my_block_number, 0, my_test_block_address);

	send_command(0xdf);
	tWB;
	send_command(0x60);
	send_addresses(my_test_block_address+2);
	send_command(0xd0);
	tWB;
	while((*jumper_address & RB_mask)==0);
}

// this function introduces delay
// .. the delay is caused using a for loop
__attribute__((always_inline)) void onfi_interface::delay_function(uint32_t loop_count)
{
	for(;loop_count>0;loop_count--)
		asm("nop");
}

void onfi_interface::profile_time()
{
	uint32_t delay = 10000;
	for(;delay<=60000;delay+=5000)
	{
#if PROFILE_DELAY_TIME
	time_info_file<<"Delay times for "<<delay<<": ";
	START_TIME;
#endif
		delay_function(delay);
#if PROFILE_DELAY_TIME
	END_TIME;
	PRINT_TIME;
#endif
	}
}
