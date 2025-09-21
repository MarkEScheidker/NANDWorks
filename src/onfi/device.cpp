#include "onfi/device.hpp"
#include <algorithm>
#include <vector>
//#include "logging.hpp"  // add if needed for verbose prints

namespace onfi {

void NandDevice::read_page(unsigned int block, unsigned int page, bool including_spare,
                           bool bytewise, std::vector<uint8_t>& out) const {
    const uint32_t total = geometry.page_size_bytes + (including_spare ? geometry.spare_size_bytes : 0);
    out.assign(total, 0xFF);

    uint8_t addr[8] = {0};
    const uint8_t addr_len = static_cast<uint8_t>(geometry.column_cycles + geometry.row_cycles);
    to_col_row_address(geometry.pages_per_block, geometry.column_cycles, geometry.row_cycles,
                       block, page, addr);

    const bool needs_pre_zero_cmd = (chip == toshiba_tlc_toggle);
    ctrl_.page_read(addr, addr_len, needs_pre_zero_cmd);

    if (bytewise) {
        uint8_t col[2] = {0, 0};
        for (uint32_t i = 0; i < total; ++i) {
            col[1] = static_cast<uint8_t>(i / 256);
            col[0] = static_cast<uint8_t>(i % 256);
            ctrl_.change_read_column(col);
            ctrl_.read_data(&out[i], 1);
        }
    } else {
        ctrl_.read_data(out.data(), static_cast<uint16_t>(total));
    }
}

void NandDevice::program_page(unsigned int block, unsigned int page, const uint8_t* data,
                              bool including_spare) const {
    uint8_t addr[8] = {0};
    const uint8_t addr_len = static_cast<uint8_t>(geometry.column_cycles + geometry.row_cycles);
    to_col_row_address(geometry.pages_per_block, geometry.column_cycles, geometry.row_cycles,
                       block, page, addr);
    (void)addr_len;

    const uint32_t total = geometry.page_size_bytes + (including_spare ? geometry.spare_size_bytes : 0);
    ctrl_.program_page(addr, data, total);
}

void NandDevice::erase_block(unsigned int block) const {
    uint8_t addr[8] = {0};
    to_col_row_address(geometry.pages_per_block, geometry.column_cycles, geometry.row_cycles,
                       block, 0, addr);
    ctrl_.erase_block(addr + geometry.column_cycles);
}

void NandDevice::partial_erase_block(unsigned int block, unsigned int page_in_block, uint32_t loop_count) const {
    uint8_t addr[8] = {0};
    to_col_row_address(geometry.pages_per_block, geometry.column_cycles, geometry.row_cycles,
                       block, page_in_block, addr);
    ctrl_.partial_erase_block(addr + geometry.column_cycles, loop_count);
}

void NandDevice::program_tlc_subpage(unsigned int block, unsigned int page, unsigned int subpage_number,
                                     const uint8_t* data, bool including_spare) const {
    uint8_t code = 0x01;
    if (subpage_number == 2) code = 0x02;
    else if (subpage_number >= 3) code = 0x03;

    uint8_t addr[8] = {0};
    to_col_row_address(geometry.pages_per_block, geometry.column_cycles, geometry.row_cycles,
                       block, page, addr);
    const uint32_t total = geometry.page_size_bytes + (including_spare ? geometry.spare_size_bytes : 0);

    ctrl_.prefix_command(code);
    const uint8_t confirm = (code < 0x03) ? 0x1A : 0x10;
    ctrl_.program_page_confirm(addr, data, total, confirm);
}

void NandDevice::program_tlc_page(unsigned int block, unsigned int page,
                                  const uint8_t* data, bool including_spare) const {
    program_tlc_subpage(block, page, 1, data, including_spare);
    program_tlc_subpage(block, page, 2, data, including_spare);
    program_tlc_subpage(block, page, 3, data, including_spare);
}

void NandDevice::read_tlc_subpages(unsigned int block, unsigned int page, DataSink& sink) const {
    uint8_t addr[8] = {0};
    to_col_row_address(geometry.pages_per_block, geometry.column_cycles, geometry.row_cycles,
                       block, page, addr);
    const uint32_t total = geometry.page_size_bytes + geometry.spare_size_bytes;
    std::vector<uint8_t> buf(total);

    for (uint8_t code = 0x01; code <= 0x03; ++code) {
        ctrl_.prefix_command(code);
        ctrl_.page_read(addr, static_cast<uint8_t>(geometry.column_cycles + geometry.row_cycles), /*pre_zero*/false);
        ctrl_.read_data(buf.data(), static_cast<uint16_t>(total));
        sink.write(buf.data(), buf.size());
        sink.newline();
    }
    sink.flush();
}

void NandDevice::read_block(unsigned int block,
                            bool complete_block,
                            const uint16_t* page_indices,
                            uint16_t num_pages,
                            bool including_spare,
                            bool bytewise,
                            DataSink& sink) const {
    if (complete_block) {
        for (uint32_t p = 0; p < geometry.pages_per_block; ++p) {
            std::vector<uint8_t> page;
            read_page(block, p, including_spare, bytewise, page);
            sink.write(page.data(), page.size());
            sink.newline();
        }
    } else {
        for (uint16_t i = 0; i < num_pages; ++i) {
            std::vector<uint8_t> page;
            read_page(block, page_indices[i], including_spare, bytewise, page);
            sink.write(page.data(), page.size());
            sink.newline();
        }
    }
    sink.flush();
}

void NandDevice::program_block(unsigned int block,
                               bool complete_block,
                               const uint16_t* page_indices,
                               uint16_t num_pages,
                               const uint8_t* provided_data,
                               bool including_spare,
                               bool randomize) const {
    const uint32_t total = geometry.page_size_bytes + (including_spare ? geometry.spare_size_bytes : 0);
    std::vector<uint8_t> buf;
    if (provided_data) {
        buf.assign(provided_data, provided_data + total);
    } else {
        buf.assign(total, 0x00);
        if (randomize) {
            for (uint32_t i = 0; i < total; ++i) buf[i] = static_cast<uint8_t>(rand() % 255);
        }
        // Avoid marking bad block: set first spare byte != 0x00
        if (including_spare && total > geometry.page_size_bytes) buf[geometry.page_size_bytes] = 0xFF;
    }

    if (complete_block) {
        for (uint32_t p = 0; p < geometry.pages_per_block; ++p) {
            program_page(block, p, buf.data(), including_spare);
        }
    } else {
        std::vector<uint16_t> sorted(page_indices, page_indices + num_pages);
        std::sort(sorted.begin(), sorted.end());
        for (uint16_t idx : sorted) program_page(block, idx, buf.data(), including_spare);
    }
}

bool NandDevice::verify_program_page(unsigned int block, unsigned int page,
                                     const uint8_t* expected,
                                     bool including_spare,
                                     bool verbose,
                                     int max_allowed_errors,
                                     uint32_t* out_byte_errors,
                                     uint32_t* out_bit_errors) const {
    const uint32_t total = geometry.page_size_bytes + (including_spare ? geometry.spare_size_bytes : 0);
    std::vector<uint8_t> got;
    read_page(block, page, including_spare, /*bytewise*/false, got);

    uint32_t byte_fail = 0, bit_fail = 0;
    for (uint32_t i = 0; i < total; ++i) {
        if (got[i] != expected[i]) {
            ++byte_fail;
            uint8_t diff = got[i] ^ expected[i];
            bit_fail += __builtin_popcount(static_cast<unsigned int>(diff));
        }
    }
    if (out_byte_errors) *out_byte_errors = byte_fail;
    if (out_bit_errors) *out_bit_errors = bit_fail;
    return static_cast<int>(byte_fail) <= max_allowed_errors;
}

bool NandDevice::verify_program_block(unsigned int block,
                                      bool complete_block,
                                      const uint16_t* page_indices,
                                      uint16_t num_pages,
                                      const uint8_t* expected,
                                      bool including_spare,
                                      bool verbose,
                                      int max_allowed_errors) const {
    const uint32_t total = geometry.page_size_bytes + (including_spare ? geometry.spare_size_bytes : 0);
    std::vector<uint8_t> default_buf;
    const uint8_t* exp = expected;
    if (!exp) { default_buf.assign(total, 0x00); exp = default_buf.data(); }

    bool ok = true;
    if (complete_block) {
        for (uint32_t p = 0; p < geometry.pages_per_block; ++p) {
            if (!verify_program_page(block, p, exp, including_spare, verbose, max_allowed_errors)) ok = false;
        }
    } else {
        for (uint16_t i = 0; i < num_pages; ++i) {
            if (!verify_program_page(block, page_indices[i], exp, including_spare, verbose, max_allowed_errors)) ok = false;
        }
    }
    return ok;
}

bool NandDevice::verify_erase_block(unsigned int block,
                                    bool complete_block,
                                    const uint16_t* page_indices,
                                    uint16_t num_pages,
                                    bool including_spare,
                                    bool verbose) const {
    const uint32_t total = geometry.page_size_bytes + (including_spare ? geometry.spare_size_bytes : 0);
    std::vector<uint8_t> got;
    bool ok = true;
    auto check_page = [&](unsigned int page) {
        read_page(block, page, including_spare, /*bytewise*/false, got);
        for (uint32_t i = 0; i < total; ++i) if (got[i] != 0xFF) { ok = false; break; }
    };
    if (complete_block) {
        for (uint32_t p = 0; p < geometry.pages_per_block; ++p) check_page(p);
    } else {
        for (uint16_t i = 0; i < num_pages; ++i) check_page(page_indices[i]);
    }
    return ok;
}

} // namespace onfi
