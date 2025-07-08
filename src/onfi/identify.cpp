#include "onfi_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <pigpio.h>

using namespace std;

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
	while(gpioRead(RB_PIN)==0);	
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
void onfi_interface::read_parameters(param_type ONFI_OR_JEDEC, bool bytewise, bool verbose)
{

	// make sure none of the LUNs are busy
	while(gpioRead(RB_PIN)==0);

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
	while(gpioRead(RB_PIN)==0);

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
	// asm("nop"); // Replaced with pigpio delay if needed
	// make sure none of the LUNs are busy
	while(gpioRead(RB_PIN)==0);

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