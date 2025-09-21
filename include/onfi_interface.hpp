/**
File:			onfi_interface.hpp
Description:	This file contains the necessary functions that are defined by the ONFI protocol
				.. for interfacing with a serial NAND memory.
				The implementation here is tested on NAND memory from Micron.
Author: 		Prawar Poudel, pp0030 (at) uah (dot) edu
Date: 			18 July 2020
*/

#ifndef ONFI_INTERFACE_H
#define ONFI_INTERFACE_H

// include our next header
#include "microprocessor_interface.hpp"
#include "onfi/types.hpp"
#include "onfi/transport.hpp"
#include <vector>


/**
 * @defgroup onfi_interface_api ONFI Interface Facade
 * @brief User-facing API for exercising ONFI NAND flash devices.
 * @details Contains the primary `onfi_interface` class plus helpers that can be used by
 *          applications, benchmarks, and automated tests to communicate with ONFI NAND parts.
 */
/** @{ */

enum param_type
{
	JEDEC = 0,
	ONFI = 1
};

// print debug messages for ONFI things
#define DEBUG_ONFI false
#define PROFILE_DELAY_TIME true

/**
 * @class onfi_interface
 * @brief High-level façade for interacting with ONFI-compliant NAND devices.
 * @details Wraps the low-level `interface` HAL with helpers for initialization,
 *          block erase/program/verify cycles, parameter queries, and common
 *          maintenance routines required by the Micron 3D NAND parts used in
 *          this project. All public members are exposed so tooling and sample
 *          applications can orchestrate self-tests or production flows without
 *          duplicating protocol logic.
 * @ingroup onfi_interface_api
 */
class onfi_interface: public interface, public onfi::Transport
{
private:
    // private items go here
    std::fstream onfi_data_file;
	std::fstream time_info_file;
	mutable std::vector<uint8_t> scratch_buffer_;
	bool erase_enabled_ = true;

	uint8_t* ensure_scratch(size_t size);

public:
	// public items go here: this should be almost all the required functions

	/** @brief Construct the interface using asynchronous signalling defaults. */
	onfi_interface(){
		interface_type = asynchronous;
		flash_chip = default_async;
	}
	~onfi_interface()
	{
		deinitialize_onfi();
	}

	using interface::send_command;
	using interface::send_addresses;
	using interface::send_data;
	using interface::wait_ready_blocking;

	void send_command(uint8_t command) const override { interface::send_command(command); }
	void send_addresses(const uint8_t* address_to_send, uint8_t num_address_bytes = 1, bool verbose = false) const override {
		interface::send_addresses(address_to_send, num_address_bytes, verbose);
	}
	void send_data(const uint8_t* data_to_send, uint16_t num_data) const override { interface::send_data(data_to_send, num_data); }
	void wait_ready_blocking() const override { interface::wait_ready_blocking(); }

	// let us make these paramters public
	uint16_t num_bytes_in_page;
	uint16_t num_spare_bytes_in_page;
	uint16_t num_pages_in_block;
	uint16_t num_blocks;
	uint8_t num_column_cycles;
	uint8_t num_row_cycles;

	char manufacturer_id[13];
	char device_model[21];
	char onfi_version[5];
	char unique_id[33]; // 32 bytes for unique ID + null terminator


	/**
	 * @brief Initialize the ONFI stack and underlying HAL resources.
	 * @param ONFI_OR_JEDEC Select ONFI (default) or JEDEC parameter parsing.
	 */
	void get_started(param_type ONFI_OR_JEDEC=ONFI);

/**
	 * @brief Bring up the hardware bridge, map registers, and perform power-on checks.
	 * @param verbose Emit detailed status to stdout if true.
	 */
	void initialize_onfi(bool verbose = false);
	void deinitialize_onfi(bool verbose = false);

	/** @brief Drive indicator LEDs to confirm GPIO wiring. */
	void test_onfi_leds(bool verbose = false);

	/** @brief Open the onfi debug data capture stream. */
	void open_onfi_data_file();

/**
	 * @brief Read raw bytes from the NAND cache/register output path.
	 * @param data_received Destination buffer.
	 * @param num_data Number of bytes to read.
	 */
	void get_data(uint8_t* data_received,uint16_t num_data) const override;

/**
	 * @brief Read the NAND status register corresponding to the last command.
	 * @return Raw ONFI status byte.
	 */
	uint8_t get_status() override;

/**
	 * @brief Print a human-readable failure interpretation of the status register.
	 */
	void print_status_on_fail();

/**
	 * @brief Execute the ONFI device initialization sequence after power-on.
	 * @param verbose Emit detailed timing/status output if true.
	 */
	void device_initialization(bool verbose = false);

/**
	 * @brief Issue an ONFI reset and wait for the device to report ready.
	 * @param verbose Emit detailed timing/status output if true.
	 */
	void reset_device(bool verbose = false);

	/**
	 * @brief Read and decode the ONFI parameter page.
	 * @param ONFI_OR_JEDEC Select ONFI (default) or JEDEC parameter parsing.
	 * @param bytewise Read data byte-by-byte (true) or word-wise (false).
	 * @param verbose Emit raw page contents and decoded information.
	 */
	void read_parameters(param_type ONFI_OR_JEDEC=ONFI,bool bytewise = true, bool verbose = false);

	/** @brief Issue the ONFI Read-ID sequence. */
	void read_id();

	/**
	 * @brief Decode major/minor ONFI version fields from the parameter page.
	 * @param byte_4 MSB of the version field.
	 * @param byte_5 LSB of the version field.
	 * @param ret_whole Stores decoded major version.
	 * @param ret_decimal Stores decoded minor revision.
	 */
	void decode_ONFI_version(uint8_t byte_4,uint8_t byte_5,uint8_t* ret_whole,uint8_t* ret_decimal);

	/** @brief Cache page size geometry read from the parameter page. */
	void set_page_size(uint8_t byte_83,uint8_t byte_82,uint8_t byte_81,uint8_t byte_80);

	/** @brief Cache spare (OOB) size read from the parameter page. */
	void set_page_size_spare(uint8_t byte_85,uint8_t byte_84);

	/** @brief Cache pages-per-block geometry read from the parameter page. */
	void set_block_size(uint8_t byte_95,uint8_t byte_94,uint8_t byte_93,uint8_t byte_92);

	/** @brief Cache blocks-per-LUN geometry read from the parameter page. */
	void set_lun_size(uint8_t byte_99,uint8_t byte_98,uint8_t byte_97,uint8_t byte_96);
/**
 * @brief Test a block for factory bad-block markers in spare area.
 * @param my_block_number Block index to inspect.
 * @return true if the block is marked bad.
 */
	bool is_bad_block(unsigned int my_block_number);

	/**
	 * @brief Populate the cache register with a page of data.
	 * @param my_block_number Block to access.
	 * @param my_page_number Page within the block.
	 * @param address_length Column+row cycles (default 5).
	 * @param verbose Emit trace-level logs.
	 */
	void read_page(unsigned int my_block_number, unsigned int my_page_number,uint8_t address_length=5,bool verbose=false);

	/** @brief Open the timing profile output stream used by benchmarking tools. */
	void open_time_profile_file();

	/**
	 * @brief Adjust the NAND read column pointer between cache fetches.
	 * @param col_address Pointer to address bytes to send.
	 */
	void change_read_column(const uint8_t* col_address);

	/**
	 * @brief Issue a block erase operation.
	 * @param my_block_number Target block index.
	 * @param verbose Emit extra logging during the command sequence.
	 */
	void erase_block(unsigned int my_block_number, bool verbose = false);

	/** @brief Drive WP#/command pins high for voltage margin checks. */
	void test_device_voltage_high();
	/** @brief Drive WP#/command pins low for voltage margin checks. */
	void test_device_voltage_low();

	/**
	 * @brief Perform a timed partial erase pulse for characterization experiments.
	 * @param my_block_number Block to operate on.
	 * @param my_page_number Page seed used by the controller helper.
	 * @param loop_count Delay loop iterations controlling pulse width.
	 * @param verbose Emit additional diagnostic output.
	 */
	void partial_erase_block(unsigned int my_block_number, unsigned int my_page_number, uint32_t loop_count = 30000,bool verbose = false);

	/**
	 * @brief Verify that a block erase left all bits in the expected erased state.
	 * @param my_block_number Block to inspect.
	 * @param complete_block Verify every page when true; otherwise use `page_indices`.
	 * @param page_indices Specific page indices to check when `complete_block` is false.
	 * @param num_pages Number of entries in `page_indices`.
	 * @param verbose Emit per-page statistics.
	 * @return true when the block verifies successfully.
	 */
	bool verify_block_erase(unsigned int my_block_number, bool complete_block = false,
		const uint16_t* page_indices = nullptr, uint16_t num_pages = 0, bool verbose = false);

	/** @brief Assert the hardware write-protect to block program/erase operations. */
	void disable_erase();
	/** @brief Release hardware write-protect so program/erase are allowed. */
	void enable_erase();

	/**
	 * @brief Program a page from a buffer.
	 * @param my_block_number Block containing the page.
	 * @param my_page_number Page index inside the block.
	 * @param data_to_program Pointer to payload (optionally includes spare).
	 * @param including_spare Program spare/OOB region when true.
	 * @param verbose Emit detailed command logging.
	 */
	void program_page(unsigned int my_block_number, unsigned int my_page_number, uint8_t* data_to_program,bool including_spare = true,bool verbose = false);
	/**
	 * @brief Program using a reduced pulse width for characterization sweeps.
	 * @param my_block_number Block containing the page.
	 * @param my_page_number Page index inside the block.
	 * @param loop_count Delay loop iterations controlling pulse width.
	 * @param data_to_program Buffer to write.
	 * @param including_spare Program spare/OOB region when true.
	 * @param verbose Emit detailed command logging.
	 */
	void partial_program_page(unsigned int my_block_number, unsigned int my_page_number,uint32_t loop_count,uint8_t* data_to_program,bool including_spare = true,bool verbose = false);

	/**
	 * @brief Program TLC pages on Toshiba parts using vendor specific sequencing.
	 */
	void program_page_tlc_toshiba(unsigned int my_block_number,unsigned int my_page_number, uint8_t* data_to_program,bool including_spare = true,bool verbose = false);
	/**
	 * @brief Program a single subpage of a TLC location on Toshiba parts.
	 */
	void program_page_tlc_toshiba_subpage(unsigned int my_block_number,unsigned int my_page_number, unsigned int subpage_number, uint8_t* data_to_program,bool including_spare = true,bool verbose = false);

	/**
	 * @brief Verify a programmed page against expected data.
	 * @param my_block_number Block containing the page.
	 * @param my_page_number Page index inside the block.
	 * @param data_to_program Expected data buffer.
	 * @param verbose Emit per-byte mismatch details.
	 * @param max_allowed_errors Allowable bit errors before failing.
	 * @return true when the verification passes within tolerance.
	 */
	bool verify_program_page(unsigned int my_block_number, unsigned int my_page_number,uint8_t* data_to_program,bool verbose = false, int max_allowed_errors = 0);

	/**
	 * @brief Write ONFI feature parameters (EFh command).
	 * @param address Feature address within the NAND.
	 * @param data_to_send Four-byte payload describing the feature configuration.
	 * @param command Optional override for the command code.
	 */
	void set_features(uint8_t address, const uint8_t* data_to_send,
		onfi::FeatureCommand command = onfi::FeatureCommand::Set);

	/**
	 * @brief Read ONFI feature parameters (EEh command).
	 * @param address Feature address within the NAND.
	 * @param data_received Buffer to receive four bytes of feature data.
	 * @param command Optional override for the command code.
	 */
	void get_features(uint8_t address, uint8_t* data_received,
		onfi::FeatureCommand command = onfi::FeatureCommand::Get) const;

	/**
	 * @brief Busy-wait delay used by profiling and margining routines.
	 * @param loop_count Iterations of the internal delay loop.
	 */
	void delay_function(uint32_t loop_count) override;
	/** @brief Capture timing data for the most recent operation. */
	void profile_time();

	/**
	 * @brief Translate block/page IDs to the column/row address tuple expected by ONFI.
	 * @param my_block_number Block containing the page.
	 * @param my_page_number Page index inside the block.
	 * @param my_test_block_address Output buffer populated with {c1,c2,r1,r2,r3} bytes.
	 * @param verbose Emit detailed arithmetic steps when true.
	 */
	void convert_pagenumber_to_columnrow_address(unsigned int my_block_number, unsigned int my_page_number, uint8_t* my_test_block_address, bool verbose);
};

/** @} */

#endif
