#include "TestCases.hpp"

#include "onboard_autonomy/adapters/transport/TransportFactory.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void quiet_udp_reads_return_immediately() {
    auto transport =
        onboard_autonomy::adapters::transport::make_udp_transport(
            "127.0.0.1",
            0
        );
    std::array<std::uint8_t, 512> buffer{};

    const auto started_at = std::chrono::steady_clock::now();
    for (int attempt = 0; attempt < 10; ++attempt) {
        require(
            transport->read(buffer) == 0,
            "quiet UDP transport must report no available bytes"
        );
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;

    require(
        elapsed < std::chrono::milliseconds(500),
        "quiet UDP reads must not inherit a receive timeout"
    );
}

}  // namespace

void run_transport_tests() {
    quiet_udp_reads_return_immediately();
}
