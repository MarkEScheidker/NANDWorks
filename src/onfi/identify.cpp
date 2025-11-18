#include "onfi_interface.hpp"
#include "gpio.hpp"
#include "timing.hpp"
#include <stdio.h>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <array>
#include "logging.hpp"
#include "onfi/param_page.hpp"

void onfi_interface::read_id() {
    constexpr uint8_t kUniqueIdLength = 32;
    uint8_t address_to_read = 0x00;

    // Read the ONFI unique ID (32 bytes)
    send_command(0xED);
    send_addresses(&address_to_read);
    std::array<uint8_t, kUniqueIdLength> unique_bytes{};

    while (gpio_read(GPIO_RB) == 0);

    get_data(unique_bytes.data(), unique_bytes.size());
    memcpy(unique_id, unique_bytes.data(), unique_bytes.size());
    unique_id[unique_bytes.size()] = '\0';

    auto log_id_bytes = [](const char* label, const uint8_t* data, size_t len) {
        LOG_ONFI_DEBUG("-------------------------------------------------");
        LOG_ONFI_DEBUG("%s", label);
        if (DEBUG_ONFI) {
            printf("-------------------------------------------------\n");
            printf("%s\n", label);
        }
        for (size_t idx = 0; idx < len; ++idx) {
            if ((data[idx] >= 'a' && data[idx] <= 'z') || (data[idx] >= 'A' && data[idx] <= 'Z')) {
                LOG_ONFI_DEBUG("%c ,", data[idx]);
                if (DEBUG_ONFI) printf("%c ,", data[idx]);
            } else {
                LOG_ONFI_DEBUG("0x%x ,", data[idx]);
                if (DEBUG_ONFI) printf("0x%x ,", data[idx]);
            }
        }
        LOG_ONFI_DEBUG("");
        LOG_ONFI_DEBUG("-------------------------------------------------");
        if (DEBUG_ONFI) {
            printf("\n-------------------------------------------------\n");
        }
    };

    // Read standard ID data from address 0x00
    std::array<uint8_t, 8> id00{};
    send_command(0x90);
    send_addresses(&address_to_read);
    get_data(id00.data(), id00.size());
    log_id_bytes("The ID at 0x00 is: ", id00.data(), id00.size());

    // Read optional data at 0x20 and 0x40
    std::array<uint8_t, 4> id20{};
    address_to_read = 0x20;
    send_command(0x90);
    send_addresses(&address_to_read);
    get_data(id20.data(), id20.size());
    log_id_bytes("The ID at 0x20 is: ", id20.data(), id20.size());

    std::array<uint8_t, 6> id40{};
    address_to_read = 0x40;
    send_command(0x90);
    send_addresses(&address_to_read);
    get_data(id40.data(), id40.size());
    log_id_bytes("The ID at 0x40 is: ", id40.data(), id40.size());

    if(DEBUG_ONFI) printf("-------------------------------------------------\n");

    //this is where we determine if the default interface is asynchronous or toggle
    // .. ths is for TOSHIBA toggle chips
    if (id00[0] == 0x98) // this is for TOSHIBA chips
    {
        LOG_ONFI_DEBUG("Detected TOSHIBA");
        if ((id00[5] & 0x80)) // this is for TOGGLE
        {
            LOG_ONFI_DEBUG(" TOGGLE");
            interface_type = toggle; // this will affect DIN and DOUT cycles
            if (((id00[2] >> 2) & 0x02) == 0x02) // this is for TLC
            {
                LOG_ONFI_DEBUG(" TLC");
                flash_chip = toshiba_tlc_toggle; // this will affect how we program pages
            }
        }
        LOG_ONFI_DEBUG(" Chip.");
    } else if (id00[0] == 0x2c) // this is for Micron
    {
        LOG_ONFI_DEBUG("Detected MICRON");
        if (((id00[2] >> 2) & 0x02) == 0x02) // this is for TLC
        {
            LOG_ONFI_DEBUG(" TLC");
            flash_chip = micron_tlc; // this will affect how we program pages
        } else if (((id00[2] >> 2) & 0x01) == 0x01) // this is for MLC
        {
            LOG_ONFI_DEBUG(" MLC");
            flash_chip = micron_mlc; // this will affect how we program pages
        } else if (((id00[2] >> 2) & 0x02) == 0x00) // this is for SLC
        {
            // flash chip will be default type
            LOG_ONFI_DEBUG(" SLC");
        }
        LOG_ONFI_DEBUG(" Chip ");
    } else {
        LOG_ONFI_DEBUG("Detected Asynchronous NAND Flash Chip.");
    }
}

// parameter page_number is the global page number in the chip
// my_test_block_address is an array [c1,c2,r1,r2,r3]
void onfi_interface::read_parameters(param_type ONFI_OR_JEDEC, bool bytewise, bool verbose) {
    // make sure none of the LUNs are busy
    while (gpio_read(GPIO_RB) == 0);

    uint8_t address_to_send = 0x00;
    std::string type_parameter = "ONFI";
    if (ONFI_OR_JEDEC == JEDEC) {
        address_to_send = 0x40;
        type_parameter = "JEDEC";
    }

    LOG_ONFI_INFO_IF(verbose, "Reading %s parameters", type_parameter.c_str());

    // make sure none of the LUNs are busy
    while (gpio_read(GPIO_RB) == 0);

    LOG_ONFI_DEBUG_IF(verbose, ".. sending command");
    // read ID command
    send_command(0xEC);
    // send address 00

    LOG_ONFI_DEBUG_IF(verbose, ".. sending address");
    send_addresses(&address_to_send);

    //have some delay here and wait for busy signal again before reading the paramters
    // asm("nop"); // Replaced with pigpio delay if needed
    // make sure none of the LUNs are busy
    while (gpio_read(GPIO_RB) == 0);

    LOG_ONFI_DEBUG_IF(verbose, ".. acquiring %s parameters", type_parameter.c_str());
    // now read the 256-bytes of data
    uint16_t num_bytes_in_parameters = 256;
    uint8_t ONFI_parameters[num_bytes_in_parameters];

    if (not bytewise)
        get_data(ONFI_parameters, num_bytes_in_parameters);

    uint8_t col_address[2] = {0, 0};
    if(DEBUG_ONFI) printf("-------------------------------------------------\n");
    for (uint16_t idx = 0; idx < num_bytes_in_parameters; idx++) {
        if (bytewise) {
            col_address[1] = idx / 256;
            col_address[0] = idx % 256;
            change_read_column(col_address);
            get_data(ONFI_parameters + idx, 1);
        }
        if(DEBUG_ONFI) {
            if (idx % 20 == 0)
                printf("\n");
            if ((ONFI_parameters[idx] >= 'a' && ONFI_parameters[idx] <= 'z') || (
                    ONFI_parameters[idx] >= 'A' && ONFI_parameters[idx] <= 'Z'))
                printf("%c ,", ONFI_parameters[idx]);
            else
                printf("0x%x ,", ONFI_parameters[idx]);
        }
    }
    if(DEBUG_ONFI) printf("\n");
    if(DEBUG_ONFI) printf("-------------------------------------------------\n");

    LOG_ONFI_DEBUG_IF(verbose, ".. acquired %s parameters", type_parameter.c_str());

    uint8_t ret_whole, ret_decimal;

    // following function is irrelevant for JEDEC Page
    decode_ONFI_version(ONFI_parameters[4], ONFI_parameters[5], &ret_whole, &ret_decimal);
    snprintf(onfi_version, sizeof(onfi_version), "%c.%c", ret_whole, ret_decimal);

    // Use parser helpers to fill geometry fields
    onfi::Geometry g{};
    onfi::parse_geometry_from_parameters(ONFI_parameters, g);
    num_bytes_in_page = static_cast<uint16_t>(g.page_size_bytes);
    num_spare_bytes_in_page = static_cast<uint16_t>(g.spare_size_bytes);
    num_pages_in_block = static_cast<uint16_t>(g.pages_per_block);
    num_blocks = static_cast<uint16_t>(g.blocks_per_lun);
    num_column_cycles = g.column_cycles;
    num_row_cycles = g.row_cycles;
    // Extract manufacturer information (Bytes 32-43)
    memcpy(manufacturer_id, &ONFI_parameters[32], 12);
    manufacturer_id[12] = '\0'; // Null-terminate the string

    // Extract device model (Bytes 44-63)
    memcpy(device_model, &ONFI_parameters[44], 20);
    device_model[20] = '\0'; // Null-terminate the string


    #if DEBUG_ONFI
    if (onfi_debug_file) {
        char my_temp[400];

        sprintf(my_temp, "Printing information from the %s paramters\n", type_parameter.c_str());
        onfi_debug_file << my_temp;

        sprintf(my_temp, ".. The signature obtained from first 4-bytes are %c%c%c%c\n", ONFI_parameters[0],
                ONFI_parameters[1], ONFI_parameters[2], ONFI_parameters[3]);
        onfi_debug_file << my_temp;

        sprintf(my_temp, ".. Bytes 4 and 5 indicate the %s version supported: The maximum supported version is %c.%c\n",
                type_parameter.c_str(), ret_whole, ret_decimal);
        onfi_debug_file << my_temp;

        sprintf(my_temp, ".. Bytes 32 to 43 gives the manufacturer information: \"%c%c%c%c%c%c%c%c%c%c%c%c\"\n",
                ONFI_parameters[32], ONFI_parameters[33], ONFI_parameters[34], ONFI_parameters[35], ONFI_parameters[36],
                ONFI_parameters[37], ONFI_parameters[38], ONFI_parameters[39], ONFI_parameters[40], ONFI_parameters[41],
                ONFI_parameters[42], ONFI_parameters[43]);
        onfi_debug_file << my_temp;

        sprintf(my_temp, ".. Bytes 44 to 63 gives the device model: \"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\"\n",
                ONFI_parameters[44], ONFI_parameters[45], ONFI_parameters[46], ONFI_parameters[47], ONFI_parameters[48],
                ONFI_parameters[49], ONFI_parameters[50], ONFI_parameters[51], ONFI_parameters[52], ONFI_parameters[53],
                ONFI_parameters[54], ONFI_parameters[55], ONFI_parameters[56], ONFI_parameters[57], ONFI_parameters[58],
                ONFI_parameters[59], ONFI_parameters[60], ONFI_parameters[61], ONFI_parameters[62],
                ONFI_parameters[63]);
        onfi_debug_file << my_temp;

        sprintf(my_temp, ".. Bytes 80-83 gives the number of bytes per page: 0x%02x%02x%02x%02x (%d bytes per page)\n",
                ONFI_parameters[83], ONFI_parameters[82], ONFI_parameters[81], ONFI_parameters[80], num_bytes_in_page);
        onfi_debug_file << my_temp;

        sprintf(my_temp,
                ".. Bytes 84-85 gives the number of spare bytes per page: 0x%02x%02x (%d spare bytes per page)\n",
                ONFI_parameters[85], ONFI_parameters[85], num_spare_bytes_in_page);
        onfi_debug_file << my_temp;

        sprintf(my_temp,
                ".. Bytes 92-95 gives the number of pages in a block: 0x%02x%02x%02x%02x (%d pages in a block)\n",
                ONFI_parameters[95], ONFI_parameters[94], ONFI_parameters[93], ONFI_parameters[92], num_pages_in_block);
        onfi_debug_file << my_temp;

        sprintf(my_temp,
                ".. Bytes 96-99 gives the number of blocks in a LUN: 0x%02x%02x%02x%02x (%d blocks in a LUN)\n",
                ONFI_parameters[99], ONFI_parameters[98], ONFI_parameters[97], ONFI_parameters[96], num_blocks);
        onfi_debug_file << my_temp;

        sprintf(my_temp, ".. Byte 100 gives the number of LUNs per chip enable: %d\n", ONFI_parameters[100]);
        onfi_debug_file << my_temp;

        sprintf(my_temp, ".. Byte 102 gives information on how many bits are per cell: %d bits per cell\n",
                ONFI_parameters[102]);
        onfi_debug_file << my_temp;

        sprintf(my_temp, ".. Byte 180 gives read-retry levels supported: %d levels\n", ONFI_parameters[180] & 0x0f);
        onfi_debug_file << my_temp;

        sprintf(my_temp, ".. Number of Colum Cycles is: %d and number of Row Cycles is %d\n", num_column_cycles,
                num_row_cycles);
        onfi_debug_file << my_temp;

        onfi_debug_file << "***********************************************";
        onfi_debug_file << std::endl;
    } else {
        if(verbose) {
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
void onfi_interface::decode_ONFI_version(uint8_t byte_4, uint8_t byte_5, uint8_t *ret_whole, uint8_t *ret_decimal) {
    if (byte_4 & 0x02) //test bit9 of [byte_4,byte_5]
    {
        *ret_whole = '4';
        *ret_decimal = '0';
        return;
    } else if (byte_4 & 0x01) //test bit8 of [byte_4,byte_5]
    {
        *ret_whole = '3';
        *ret_decimal = '2';
        return;
    } else if (byte_5 & 0x80) //test bit7 of [byte_4,byte_5]
    {
        *ret_whole = '3';
        *ret_decimal = '1';
        return;
    } else if (byte_5 & 0x40) //test bit6 of [byte_4,byte_5]
    {
        *ret_whole = '3';
        *ret_decimal = '0';
        return;
    } else if (byte_5 & 0x20) //test bit5 of [byte_4,byte_5]
    {
        *ret_whole = '2';
        *ret_decimal = '3';
        return;
    } else if (byte_5 & 0x10) //test bit4 of [byte_4,byte_5]
    {
        *ret_whole = '2';
        *ret_decimal = '2';
        return;
    } else if (byte_5 & 0x8) //test bit3 of [byte_4,byte_5]
    {
        *ret_whole = '2';
        *ret_decimal = '1';
        return;
    } else if (byte_5 & 0x04) //test bit2 of [byte_4,byte_5]
    {
        *ret_whole = '2';
        *ret_decimal = '0';
        return;
    } else if (byte_5 & 0x02) //test bit1 of [byte_4,byte_5]
    {
        *ret_whole = '1';
        *ret_decimal = '0';
        return;
    } else {
        *ret_whole = 'x';
        *ret_decimal = 'x';
        return;
    }
}


// following function will set the size of page based on value read
void onfi_interface::set_page_size(uint8_t byte_83, uint8_t byte_82, uint8_t byte_81, uint8_t byte_80) {
    num_bytes_in_page = static_cast<uint16_t>(onfi::parse_page_size(byte_83, byte_82, byte_81, byte_80));
}

// following function will set the size of spare bytes in a page based on value read
void onfi_interface::set_page_size_spare(uint8_t byte_85, uint8_t byte_84) {
    num_spare_bytes_in_page = static_cast<uint16_t>(onfi::parse_spare_size(byte_85, byte_84));
}

// following function will set the number of pages in a block
void onfi_interface::set_block_size(uint8_t byte_95, uint8_t byte_94, uint8_t byte_93, uint8_t byte_92) {
    num_pages_in_block = static_cast<uint16_t>(onfi::parse_pages_per_block(byte_95, byte_94, byte_93, byte_92));
}

void onfi_interface::set_lun_size(uint8_t byte_99, uint8_t byte_98, uint8_t byte_97, uint8_t byte_96) {
    num_blocks = static_cast<uint16_t>(onfi::parse_blocks_per_lun(byte_99, byte_98, byte_97, byte_96));
}

// following function tests if the block address sent was a bad block or not
// the ONFI requirement is that the first byte in the spare area of the first page
// .. should have 0x00 in it to indicate if the block is bad
// this function takes in a block address
// .. and reads the first spare byte of the first page to determine if it is a bad block
bool onfi_interface::is_bad_block(unsigned int my_block_number) {
    //let us read the first spare byte i.e. byte number 16384 in the first page
    read_page(my_block_number, 0, 5);

    //let us change the read column point to the first byte of spare region
    uint8_t spare_col[2] = {
        static_cast<uint8_t>(num_bytes_in_page & 0xff),
        static_cast<uint8_t>((num_bytes_in_page >> 8) & 0xff)
    };
    change_read_column(spare_col);

    uint8_t spare_byte = 0xFF;
    get_data(&spare_byte, 1);

    uint8_t reset_col[2] = {0x00, 0x00};
    change_read_column(reset_col);

    return spare_byte == 0x00;
}

// write a function to perform an read operation from NAND flash to cache register
// .. reads one page from the NAND to the cache register
// .. during the read, you can use change_read_column and change_row_address
