#ifndef ONFI_DATA_SINK_H
#define ONFI_DATA_SINK_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ostream>
#include <iomanip>

namespace onfi {

class DataSink {
public:
    virtual ~DataSink() {}
    virtual void write(const uint8_t* data, std::size_t n) = 0;
    virtual void newline() {}
    virtual void flush() {}
};

class FileDataSink : public DataSink {
    std::ofstream out_;
public:
    explicit FileDataSink(const char* path) : out_(path, std::ios::binary | std::ios::out) {}
    void write(const uint8_t* data, std::size_t n) override {
        out_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(n));
    }
    void newline() override { out_.put('\n'); }
    void flush() override { out_.flush(); }
};

class OstreamDataSink : public DataSink {
    std::ostream& out_;
public:
    explicit OstreamDataSink(std::ostream& o) : out_(o) {}
    void write(const uint8_t* data, std::size_t n) override {
        out_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(n));
    }
    void newline() override { out_.put('\n'); }
    void flush() override { out_.flush(); }
};

class HexOstreamDataSink : public DataSink {
    std::ostream& out_;
    std::size_t bytes_per_line_;
    bool show_offsets_;
    std::size_t offset_ = 0;
public:
    explicit HexOstreamDataSink(std::ostream& o, std::size_t bytes_per_line = 16, bool show_offsets = true)
        : out_(o), bytes_per_line_(bytes_per_line), show_offsets_(show_offsets) {}

    void write(const uint8_t* data, std::size_t n) override {
        std::size_t i = 0;
        while (i < n) {
            // Start of line
            if (i % bytes_per_line_ == 0) {
                if (show_offsets_) {
                    out_ << std::setw(8) << std::setfill('0') << std::hex << std::uppercase
                         << static_cast<unsigned int>(offset_) << ": ";
                    out_ << std::dec << std::nouppercase; // restore
                }
            }

            // Collect one line
            std::size_t line_start = i;
            std::size_t line_end = std::min(line_start + bytes_per_line_, n);

            // Hex bytes
            for (std::size_t j = line_start; j < line_end; ++j) {
                if (j > line_start) out_ << ' ';
                // extra spacer in middle for readability
                if (bytes_per_line_ == 16 && j == line_start + 8) out_ << ' ';
                out_ << std::setw(2) << std::setfill('0') << std::hex << std::uppercase
                     << static_cast<unsigned int>(data[j])
                     << std::dec << std::nouppercase;
            }

            // Pad if last line short
            if (line_end - line_start < bytes_per_line_) {
                std::size_t missing = bytes_per_line_ - (line_end - line_start);
                std::size_t gaps = (bytes_per_line_ == 16 && (line_end - line_start) <= 8)
                                     ? (missing + 1) : missing; // account for middle spacer
                for (std::size_t k = 0; k < gaps; ++k) {
                    out_ << "   ";
                }
            }

            // ASCII sidebar
            out_ << "  |";
            for (std::size_t j = line_start; j < line_end; ++j) {
                char c = static_cast<char>(data[j]);
                out_ << ((c >= 32 && c <= 126) ? c : '.');
            }
            // Pad ASCII if short
            for (std::size_t j = line_end; j < line_start + bytes_per_line_; ++j) out_ << ' ';
            out_ << "|\n";

            offset_ += (line_end - line_start);
            i = line_end;
        }
    }
    void newline() override { out_.put('\n'); }
    void flush() override { out_.flush(); }
};

} // namespace onfi

#endif // ONFI_DATA_SINK_H
