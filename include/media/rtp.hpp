#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace media {

struct RtpPacket {
    bool marker{};
    std::uint8_t payload_type{};
    std::uint16_t sequence{};
    std::uint32_t timestamp{};
    std::uint32_t ssrc{};
    std::vector<std::uint32_t> csrcs;
    std::optional<std::uint16_t> extension_profile;
    std::vector<std::uint8_t> extension_data;
    std::vector<std::uint8_t> payload;

    static std::optional<RtpPacket> parse(std::span<const std::uint8_t> bytes, std::string& error);
};

struct StreamStats {
    std::uint64_t received{};
    std::uint64_t estimated_lost{};
    std::uint64_t duplicates{};
    std::uint64_t out_of_order{};
    double interarrival_jitter_ticks{};
};

class RtpStreamMonitor {
public:
    explicit RtpStreamMonitor(std::uint32_t clock_rate = 90000);
    void observe(const RtpPacket& packet, double arrival_time_ms);
    [[nodiscard]] const StreamStats& stats() const noexcept { return stats_; }

private:
    std::uint32_t clock_rate_;
    StreamStats stats_{};
    std::optional<std::uint16_t> highest_sequence_;
    std::optional<double> previous_transit_;
    std::unordered_set<std::uint16_t> seen_;
};

}  // namespace media
