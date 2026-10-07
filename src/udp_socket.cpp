#include "media/udp_socket.hpp"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace media {
namespace {

#ifdef _WIN32
constexpr std::uintptr_t invalid_socket = static_cast<std::uintptr_t>(INVALID_SOCKET);
using socket_length = int;
void close_socket(std::uintptr_t handle) { closesocket(static_cast<SOCKET>(handle)); }
#else
constexpr int invalid_socket = -1;
using socket_length = socklen_t;
void close_socket(int handle) { close(handle); }
#endif

}  // namespace

UdpSocket::UdpSocket() : handle_(invalid_socket) {
#ifdef _WIN32
    WSADATA data{};
    WSAStartup(MAKEWORD(2, 2), &data);
#endif
}

UdpSocket::~UdpSocket() {
    if (handle_ != invalid_socket) close_socket(handle_);
#ifdef _WIN32
    WSACleanup();
#endif
}

bool UdpSocket::bind(std::uint16_t port, std::string& error) {
    error.clear();
    if (handle_ != invalid_socket) close_socket(handle_);
    handle_ = static_cast<decltype(handle_)>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (handle_ == invalid_socket) {
        error = "Could not create UDP socket";
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::bind(static_cast<int>(handle_), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        error = "Could not bind UDP socket";
        close_socket(handle_);
        handle_ = invalid_socket;
        return false;
    }
    return true;
}

std::uint16_t UdpSocket::local_port(std::string& error) const {
    error.clear();
    sockaddr_in address{};
    socket_length length = sizeof(address);
    if (handle_ == invalid_socket ||
        getsockname(static_cast<int>(handle_), reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        error = "Could not read local UDP port";
        return 0U;
    }
    return ntohs(address.sin_port);
}

std::optional<std::vector<std::uint8_t>> UdpSocket::receive(
    std::size_t maximum_bytes, std::chrono::milliseconds timeout, std::string& error) {
    error.clear();
    if (handle_ == invalid_socket) {
        error = "UDP socket is not bound";
        return std::nullopt;
    }
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(static_cast<int>(handle_), &read_set);
    timeval duration{};
    duration.tv_sec = static_cast<long>(timeout.count() / 1000);
    duration.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
    const int ready = select(static_cast<int>(handle_) + 1, &read_set, nullptr, nullptr, &duration);
    if (ready == 0) {
        error = "UDP receive timed out";
        return std::nullopt;
    }
    if (ready < 0) {
        error = "UDP receive failed while waiting";
        return std::nullopt;
    }
    std::vector<std::uint8_t> buffer(maximum_bytes);
    const int received = recvfrom(static_cast<int>(handle_), reinterpret_cast<char*>(buffer.data()),
                                  static_cast<int>(buffer.size()), 0, nullptr, nullptr);
    if (received < 0) {
        error = "UDP receive failed";
        return std::nullopt;
    }
    buffer.resize(static_cast<std::size_t>(received));
    return buffer;
}

bool UdpSocket::send_to(std::string_view host, std::uint16_t port,
                        const std::vector<std::uint8_t>& data, std::string& error) {
    error.clear();
#ifdef _WIN32
    WSADATA startup{};
    WSAStartup(MAKEWORD(2, 2), &startup);
#endif
    const auto socket_handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_handle == invalid_socket) {
        error = "Could not create UDP sender";
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    const std::string host_copy(host);
    if (inet_pton(AF_INET, host_copy.c_str(), &address.sin_addr) != 1) {
        error = "UDP destination must be an IPv4 address";
        close_socket(socket_handle);
        return false;
    }
    const int sent = sendto(static_cast<int>(socket_handle), reinterpret_cast<const char*>(data.data()),
                            static_cast<int>(data.size()), 0,
                            reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    close_socket(socket_handle);
#ifdef _WIN32
    WSACleanup();
#endif
    if (sent != static_cast<int>(data.size())) {
        error = "UDP datagram was not fully sent";
        return false;
    }
    return true;
}

}  // namespace media
