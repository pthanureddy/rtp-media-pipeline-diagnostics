#include "media/rtp.hpp"

#include <algorithm>
#include <cmath>

namespace media {
namespace {

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) |
                                      static_cast<std::uint16_t>(bytes[offset + 1]));
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

}  // namespace

std::optional<RtpPacket> RtpPacket::parse(std::span<const std::uint8_t> bytes, std::string& error) {
    error.clear();
    if (bytes.size() < 12U) {
        error = "RTP packet is shorter than the fixed header";
        return std::nullopt;
    }
    if ((bytes[0] >> 6U) != 2U) {
        error = "RTP version must be 2";
        return std::nullopt;
    }

    const bool has_padding = (bytes[0] & 0x20U) != 0U;
    const bool has_extension = (bytes[0] & 0x10U) != 0U;
    const std::size_t csrc_count = bytes[0] & 0x0FU;
    std::size_t offset = 12U + csrc_count * 4U;
    if (bytes.size() < offset) {
        error = "RTP CSRC list is truncated";
        return std::nullopt;
    }

    RtpPacket packet;
    packet.marker = (bytes[1] & 0x80U) != 0U;
    packet.payload_type = bytes[1] & 0x7FU;
    packet.sequence = read_u16(bytes, 2U);
    packet.timestamp = read_u32(bytes, 4U);
    packet.ssrc = read_u32(bytes, 8U);
    for (std::size_t i = 0; i < csrc_count; ++i) {
        packet.csrcs.push_back(read_u32(bytes, 12U + i * 4U));
    }

    if (has_extension) {
        if (bytes.size() < offset + 4U) {
            error = "RTP extension header is truncated";
            return std::nullopt;
        }
        packet.extension_profile = read_u16(bytes, offset);
        const std::size_t extension_size = static_cast<std::size_t>(read_u16(bytes, offset + 2U)) * 4U;
        offset += 4U;
        if (bytes.size() < offset + extension_size) {
            error = "RTP extension data is truncated";
            return std::nullopt;
        }
        packet.extension_data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                     bytes.begin() + static_cast<std::ptrdiff_t>(offset + extension_size));
        offset += extension_size;
    }

    std::size_t payload_end = bytes.size();
    if (has_padding) {
        const std::size_t padding_size = bytes.back();
        if (padding_size == 0U || padding_size > payload_end - offset) {
            error = "RTP padding length is invalid";
            return std::nullopt;
        }
        payload_end -= padding_size;
    }
    packet.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                          bytes.begin() + static_cast<std::ptrdiff_t>(payload_end));
    return packet;
}

RtpStreamMonitor::RtpStreamMonitor(std::uint32_t clock_rate) : clock_rate_(clock_rate) {}

void RtpStreamMonitor::observe(const RtpPacket& packet, double arrival_time_ms) {
    ++stats_.received;
    const bool already_seen = !seen_.insert(packet.sequence).second;
    if (already_seen) {
        ++stats_.duplicates;
    } else if (!highest_sequence_) {
        highest_sequence_ = packet.sequence;
    } else {
        const std::uint16_t forward = static_cast<std::uint16_t>(packet.sequence - *highest_sequence_);
        if (forward > 0U && forward < 0x8000U) {
            if (forward > 1U) {
                stats_.estimated_lost += static_cast<std::uint64_t>(forward - 1U);
            }
            highest_sequence_ = packet.sequence;
        } else if (packet.sequence != *highest_sequence_) {
            ++stats_.out_of_order;
        }
    }

    const double arrival_ticks = arrival_time_ms * static_cast<double>(clock_rate_) / 1000.0;
    const double transit = arrival_ticks - static_cast<double>(packet.timestamp);
    if (previous_transit_) {
        const double delta = std::abs(transit - *previous_transit_);
        stats_.interarrival_jitter_ticks += (delta - stats_.interarrival_jitter_ticks) / 16.0;
    }
    previous_transit_ = transit;
}

}  // namespace media
