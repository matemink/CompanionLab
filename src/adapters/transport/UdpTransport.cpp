#include "companionlab/adapters/transport/TransportFactory.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <WinSock2.h>
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace companionlab::adapters::transport {
namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

void close_socket(const SocketHandle socket_handle) {
#ifdef _WIN32
    closesocket(socket_handle);
#else
    close(socket_handle);
#endif
}

class UdpTransport final : public application::ports::Transport {
public:
    UdpTransport(std::string bind_address, const std::uint16_t port)
        : bind_address_(std::move(bind_address)), port_(port) {
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif

        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == kInvalidSocket) {
            cleanup_network();
            throw std::runtime_error("Unable to create UDP socket");
        }

        const int reuse_address = 1;
        setsockopt(
            socket_,
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuse_address),
            sizeof(reuse_address)
        );

#ifdef _WIN32
        const DWORD timeout_ms = 250;
        setsockopt(
            socket_,
            SOL_SOCKET,
            SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout_ms),
            sizeof(timeout_ms)
        );
#else
        const timeval timeout{
            .tv_sec = 0,
            .tv_usec = 250000,
        };
        setsockopt(
            socket_,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)
        );
#endif

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port_);
        if (inet_pton(
                AF_INET,
                bind_address_.c_str(),
                &address.sin_addr
            ) != 1) {
            close_socket(socket_);
            socket_ = kInvalidSocket;
            cleanup_network();
            throw std::invalid_argument("UDP bind address must be IPv4");
        }

        if (bind(
                socket_,
                reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)
            ) != 0) {
            close_socket(socket_);
            socket_ = kInvalidSocket;
            cleanup_network();
            throw std::runtime_error("Unable to bind UDP socket");
        }
    }

    ~UdpTransport() override {
        if (socket_ != kInvalidSocket) {
            close_socket(socket_);
        }
        cleanup_network();
    }

    std::size_t read(std::span<std::uint8_t> destination) override {
        sockaddr_storage sender{};
#ifdef _WIN32
        int sender_length = sizeof(sender);
        const int received = recvfrom(
            socket_,
            reinterpret_cast<char*>(destination.data()),
            static_cast<int>(destination.size()),
            0,
            reinterpret_cast<sockaddr*>(&sender),
            &sender_length
        );
        if (received == SOCKET_ERROR) {
            const int error = WSAGetLastError();
            if (error == WSAETIMEDOUT || error == WSAEWOULDBLOCK) {
                return 0;
            }
            throw std::runtime_error("UDP receive failed");
        }
#else
        socklen_t sender_length = sizeof(sender);
        const auto received = recvfrom(
            socket_,
            destination.data(),
            destination.size(),
            0,
            reinterpret_cast<sockaddr*>(&sender),
            &sender_length
        );
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK ||
                errno == EINTR) {
                return 0;
            }
            throw std::runtime_error(
                std::string("UDP receive failed: ") +
                std::strerror(errno)
            );
        }
#endif

        peer_ = sender;
        peer_length_ = sender_length;
        return static_cast<std::size_t>(received);
    }

    std::size_t write(std::span<const std::uint8_t> source) override {
        if (peer_length_ == 0) {
            return 0;
        }

#ifdef _WIN32
        const int sent = sendto(
            socket_,
            reinterpret_cast<const char*>(source.data()),
            static_cast<int>(source.size()),
            0,
            reinterpret_cast<const sockaddr*>(&peer_),
            peer_length_
        );
        if (sent == SOCKET_ERROR) {
            throw std::runtime_error("UDP send failed");
        }
#else
        const auto sent = sendto(
            socket_,
            source.data(),
            source.size(),
            0,
            reinterpret_cast<const sockaddr*>(&peer_),
            peer_length_
        );
        if (sent < 0) {
            throw std::runtime_error(
                std::string("UDP send failed: ") +
                std::strerror(errno)
            );
        }
#endif
        return static_cast<std::size_t>(sent);
    }

    [[nodiscard]] std::string description() const override {
        return "udp://" + bind_address_ + ':' + std::to_string(port_);
    }

private:
    void cleanup_network() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    std::string bind_address_;
    std::uint16_t port_;
    SocketHandle socket_{kInvalidSocket};
    sockaddr_storage peer_{};
#ifdef _WIN32
    int peer_length_{0};
#else
    socklen_t peer_length_{0};
#endif
};

}  // namespace

std::unique_ptr<application::ports::Transport> make_udp_transport(
    const std::string& bind_address,
    const std::uint16_t port
) {
    return std::make_unique<UdpTransport>(bind_address, port);
}

}  // namespace companionlab::adapters::transport
