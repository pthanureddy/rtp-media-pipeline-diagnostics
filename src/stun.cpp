#include "media/stun.hpp"

#include <sstream>

namespace media {
namespace {

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) |
                                      static_cast<std::uint16_t>(bytes[offset + 1U]));
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

void write_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void write_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value >> 24U));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

}  // namespace

std::vector<std::uint8_t> make_stun_binding_request(const std::array<std::uint8_t, 12>& transaction_id) {
    std::vector<std::uint8_t> request;
    request.reserve(20U);
    write_u16(request, 0x0001U);
    write_u16(request, 0U);
    write_u32(request, stun_magic_cookie);
    request.insert(request.end(), transaction_id.begin(), transaction_id.end());
    return request;
}

std::optional<StunMappedAddress> parse_stun_binding_response(
    std::span<const std::uint8_t> bytes,
    const std::array<std::uint8_t, 12>& expected_transaction_id,
    std::string& error) {
    error.clear();
    if (bytes.size() < 20U) {
        error = "STUN response is shorter than the fixed header";
        return std::nullopt;
    }
    if (read_u16(bytes, 0U) != 0x0101U || read_u32(bytes, 4U) != stun_magic_cookie) {
        error = "STUN response type or magic cookie is invalid";
        return std::nullopt;
    }
    for (std::size_t i = 0; i < expected_transaction_id.size(); ++i) {
        if (bytes[8U + i] != expected_transaction_id[i]) {
            error = "STUN transaction ID does not match the request";
            return std::nullopt;
        }
    }
    const std::size_t message_length = read_u16(bytes, 2U);
    if (20U + message_length > bytes.size()) {
        error = "STUN attribute block is truncated";
        return std::nullopt;
    }

    std::size_t offset = 20U;
    while (offset + 4U <= 20U + message_length) {
        const std::uint16_t type = read_u16(bytes, offset);
        const std::size_t length = read_u16(bytes, offset + 2U);
        offset += 4U;
        if (offset + length > 20U + message_length) {
            error = "STUN attribute is truncated";
            return std::nullopt;
        }
        if (type == 0x0020U) {
            if (length != 8U || bytes[offset + 1U] != 0x01U) {
                error = "Only IPv4 XOR-MAPPED-ADDRESS attributes are supported";
                return std::nullopt;
            }
            const std::uint16_t xor_port = read_u16(bytes, offset + 2U);
            const std::uint32_t xor_address = read_u32(bytes, offset + 4U);
            const std::uint16_t port = xor_port ^ static_cast<std::uint16_t>(stun_magic_cookie >> 16U);
            const std::uint32_t address = xor_address ^ stun_magic_cookie;
            std::ostringstream ip;
            ip << ((address >> 24U) & 0xFFU) << '.' << ((address >> 16U) & 0xFFU) << '.'
               << ((address >> 8U) & 0xFFU) << '.' << (address & 0xFFU);
            return StunMappedAddress{ip.str(), port};
        }
        offset += (length + 3U) & ~std::size_t{3U};
    }
    error = "STUN response has no XOR-MAPPED-ADDRESS attribute";
    return std::nullopt;
}

}  // namespace media
