#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace media {

inline constexpr std::uint32_t stun_magic_cookie = 0x2112A442U;

struct StunMappedAddress {
    std::string ipv4;
    std::uint16_t port{};
};

std::vector<std::uint8_t> make_stun_binding_request(const std::array<std::uint8_t, 12>& transaction_id);
std::optional<StunMappedAddress> parse_stun_binding_response(
    std::span<const std::uint8_t> bytes,
    const std::array<std::uint8_t, 12>& expected_transaction_id,
    std::string& error);

}  // namespace media
