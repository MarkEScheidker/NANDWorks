#include "onfi_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <pigpio.h>

using namespace std;

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
void onfi_interface::open_onfi_debug_file()
{
#if DEBUG_ONFI
	onfi_debug_file.open("onfi_debug_log.txt",std::fstream::out);
#endif
}
void onfi_interface::open_onfi_data_file()
{
	onfi_data_file.open("data_file.txt",std::fstream::out);
}
void onfi_interface::open_time_profile_file()
{
#if PROFILE_TIME
	time_info_file.open("time_info_file.txt",std::fstream::out);
#endif
}

// follow the following function call by get_data() function call
// .. please change this if the device has multiple dies
void onfi_interface::initialize_onfi(bool verbose)
{
	cout << "Entering initialize_onfi()" << endl;
	open_interface_debug_file();
	//open the ONFI debug file
	open_onfi_debug_file();
	// open the data file
	open_onfi_data_file();

	// No need for bridge_base_virtual or fd with pigpio

#if DEBUG_ONFI
	if(onfi_debug_file)
		onfi_debug_file<<"I: Successful initialization of pigpio and pin modes"<<endl;
	else
		cout<<"I: Successful initialization of pigpio and pin modes"<<endl;
#else
	if(verbose) cout<<"I: Successful initialization of pigpio and pin modes"<<endl;
#endif
}

void onfi_interface::deinitialize_onfi(bool verbose)
{
	// No need for unmap_physical or close_physical with pigpio
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
	gpioDelay(20000); // 20ms delay
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
	cout << "Setting pin direction inactive" << endl;
	set_pin_direction_inactive();
	cout << "Setting default pin values" << endl;
	set_default_pin_values();

	// we need to set CE to low for RESET to work
	cout << "Setting CE low" << endl;
	set_ce_low();

	//insert delay here
	cout << "Delaying for 50us" << endl;
	gpioDelay(50); // 50 us max

	// wait for R/B signal to go high
	cout << "Waiting for R/B signal to go high" << endl;
	int timeout = 0;
	while(gpioRead(GPIO_RB)==0)
	{
		timeout++;
		if(timeout > 1000000)
		{
			cout << "Timed out waiting for R/B to go high" << endl;
			break;
		}
	}

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

	int timeout = 0;
	while(gpioRead(GPIO_RB)==0)
	{
		timeout++;
		if(timeout > 1000000)
		{
			cout << "Timed out waiting for R/B to go high" << endl;
			break;
		}
	}
}