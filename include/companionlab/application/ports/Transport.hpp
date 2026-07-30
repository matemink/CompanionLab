#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace companionlab::application::ports {

class Transport {
public:
    virtual ~Transport() = default;

    virtual std::size_t read(std::span<std::uint8_t> destination) = 0;
    virtual std::size_t write(std::span<const std::uint8_t> source) = 0;
    [[nodiscard]] virtual std::string description() const = 0;
};

}  // namespace companionlab::application::ports
