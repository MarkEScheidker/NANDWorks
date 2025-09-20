#ifndef ONFI_DEVICE_H
#define ONFI_DEVICE_H

#include <stdint.h>
#include <vector>
#include "onfi/types.hpp"
#include "onfi/controller.hpp"
#include "onfi/data_sink.hpp"
#include "onfi/address.hpp"
#include "microprocessor_interface.hpp" // for enums

namespace onfi {

// Higher-level device wrapper: owns geometry, routes flows via controller.
class NandDevice {
    OnfiController& ctrl_;
public:
    Geometry geometry{};
    default_interface_type interface_type = asynchronous;
    chip_type chip = default_async;

    explicit NandDevice(OnfiController& ctrl) : ctrl_(ctrl) {}

    // Read a full page (+optional spare) into a buffer.
    // If bytewise=true, performs column changes for each byte.
    void read_page(unsigned int block, unsigned int page, bool including_spare,
                   bool bytewise, std::vector<uint8_t>& out) const;

    // Program a page from provided data; including_spare controls total bytes.
    void program_page(unsigned int block, unsigned int page, const uint8_t* data,
                      bool including_spare) const;

    // Erase block
    void erase_block(unsigned int block) const;

    // Partial erase block (custom flow), page is used to form row address
    void partial_erase_block(unsigned int block, unsigned int page_in_block, uint32_t loop_count) const;

    // Toshiba TLC helpers
    void program_tlc_subpage(unsigned int block, unsigned int page, unsigned int subpage_number,
                             const uint8_t* data, bool including_spare) const;
    void program_tlc_page(unsigned int block, unsigned int page,
                          const uint8_t* data, bool including_spare) const;
    void read_tlc_subpages(unsigned int block, unsigned int page, DataSink& sink) const;

    // Read selected or full block pages and stream to sink
    void read_block(unsigned int block,
                    bool complete_block,
                    const uint16_t* page_indices,
                    uint16_t num_pages,
                    bool including_spare,
                    bool bytewise,
                    DataSink& sink) const;

    // Program pages in a block with either zeroed data or provided/random data
    void program_block(unsigned int block,
                       bool complete_block,
                       const uint16_t* page_indices,
                       uint16_t num_pages,
                       const uint8_t* provided_data,
                       bool including_spare,
                       bool randomize) const;

    // Verification helpers
    bool verify_program_page(unsigned int block, unsigned int page,
                             const uint8_t* expected,
                             bool including_spare,
                             bool verbose,
                             int max_allowed_errors,
                             uint32_t* out_byte_errors = nullptr,
                             uint32_t* out_bit_errors = nullptr) const;

    bool verify_program_block(unsigned int block,
                              bool complete_block,
                              const uint16_t* page_indices,
                              uint16_t num_pages,
                              const uint8_t* expected,
                              bool including_spare,
                              bool verbose,
                              int max_allowed_errors) const;

    bool verify_erase_block(unsigned int block,
                            bool complete_block,
                            const uint16_t* page_indices,
                            uint16_t num_pages,
                            bool including_spare,
                            bool verbose) const;
};

} // namespace onfi

#endif // ONFI_DEVICE_H
