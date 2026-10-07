#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace media {

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool bind(std::uint16_t port, std::string& error);
    [[nodiscard]] std::uint16_t local_port(std::string& error) const;
    std::optional<std::vector<std::uint8_t>> receive(
        std::size_t maximum_bytes, std::chrono::milliseconds timeout, std::string& error);
    static bool send_to(std::string_view host, std::uint16_t port,
                        const std::vector<std::uint8_t>& data, std::string& error);

private:
#ifdef _WIN32
    std::uintptr_t handle_;
#else
    int handle_;
#endif
};

}  // namespace media
