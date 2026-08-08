#include "onboard_autonomy/adapters/transport/TransportFactory.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#ifndef _WIN32
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace onboard_autonomy::adapters::transport {
namespace {

#ifndef _WIN32

speed_t to_posix_baud(const std::uint32_t baud_rate) {
    switch (baud_rate) {
        case 57600:
            return B57600;
        case 115200:
            return B115200;
#ifdef B460800
        case 460800:
            return B460800;
#endif
#ifdef B921600
        case 921600:
            return B921600;
#endif
        default:
            throw std::invalid_argument("Unsupported serial baud rate");
    }
}

class PosixSerialTransport final : public application::ports::Transport {
public:
    PosixSerialTransport(
        std::string device,
        const std::uint32_t baud_rate
    )
        : device_(std::move(device)), baud_rate_(baud_rate) {
        file_descriptor_ = open(
            device_.c_str(),
            O_RDWR | O_NOCTTY | O_CLOEXEC
        );
        if (file_descriptor_ < 0) {
            throw std::runtime_error(
                "Unable to open serial device " + device_ + ": " +
                std::strerror(errno)
            );
        }

        termios options{};
        if (tcgetattr(file_descriptor_, &options) != 0) {
            close(file_descriptor_);
            throw std::runtime_error("Unable to read serial settings");
        }

        cfmakeraw(&options);
        const speed_t baud = to_posix_baud(baud_rate_);
        cfsetispeed(&options, baud);
        cfsetospeed(&options, baud);
        options.c_cflag |= CLOCAL | CREAD;
        const auto clear_control_flag = [&options](const tcflag_t flag) {
            options.c_cflag &= ~flag;
        };
        clear_control_flag(static_cast<tcflag_t>(CSTOPB));
        clear_control_flag(static_cast<tcflag_t>(CRTSCTS));
        clear_control_flag(static_cast<tcflag_t>(PARENB));
        clear_control_flag(static_cast<tcflag_t>(CSIZE));
        options.c_cflag |= CS8;
        options.c_cc[VMIN] = 0;
        // The application loop owns timing; transport reads must not
        // stall camera and autonomy polling.
        options.c_cc[VTIME] = 0;

        if (tcsetattr(file_descriptor_, TCSANOW, &options) != 0) {
            close(file_descriptor_);
            throw std::runtime_error("Unable to configure serial device");
        }
    }

    ~PosixSerialTransport() override {
        if (file_descriptor_ >= 0) {
            close(file_descriptor_);
        }
    }

    std::size_t read(std::span<std::uint8_t> destination) override {
        const auto received = ::read(
            file_descriptor_,
            destination.data(),
            destination.size()
        );
        if (received < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                return 0;
            }
            throw std::runtime_error(
                std::string("Serial read failed: ") +
                std::strerror(errno)
            );
        }
        return static_cast<std::size_t>(received);
    }

    std::size_t write(std::span<const std::uint8_t> source) override {
        const auto sent = ::write(
            file_descriptor_,
            source.data(),
            source.size()
        );
        if (sent < 0) {
            throw std::runtime_error(
                std::string("Serial write failed: ") +
                std::strerror(errno)
            );
        }
        return static_cast<std::size_t>(sent);
    }

    [[nodiscard]] std::string description() const override {
        return "serial://" + device_ + "?baud=" +
               std::to_string(baud_rate_);
    }

private:
    std::string device_;
    std::uint32_t baud_rate_;
    int file_descriptor_{-1};
};

#endif

}  // namespace

std::unique_ptr<application::ports::Transport> make_serial_transport(
    const std::string& device,
    const std::uint32_t baud_rate
) {
#ifdef _WIN32
    static_cast<void>(device);
    static_cast<void>(baud_rate);
    throw std::runtime_error(
        "Serial transport is currently supported on Linux only"
    );
#else
    return std::make_unique<PosixSerialTransport>(device, baud_rate);
#endif
}

}  // namespace onboard_autonomy::adapters::transport
