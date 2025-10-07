#ifndef NANDWORKS_COMMAND_ARGUMENTS_HPP
#define NANDWORKS_COMMAND_ARGUMENTS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nandworks {

class CommandArguments {
public:
    CommandArguments();
    CommandArguments(std::unordered_map<std::string, std::vector<std::string>> options,
                     std::vector<std::string> positionals);

    bool has(std::string_view long_name) const;
    const std::vector<std::string>& values(std::string_view long_name) const;
    std::optional<std::string> value(std::string_view long_name) const;
    std::string value_or(std::string_view long_name, std::string_view fallback) const;

    int64_t value_as_int(std::string_view long_name, int64_t fallback) const;
    int64_t require_int(std::string_view long_name) const;

    std::size_t positional_count() const noexcept;
    const std::string& positional(std::size_t index) const;
    const std::vector<std::string>& positionals() const noexcept { return positionals_; }

private:
    std::unordered_map<std::string, std::vector<std::string>> options_;
    std::vector<std::string> positionals_;
};

} // namespace nandworks

#endif // NANDWORKS_COMMAND_ARGUMENTS_HPP
