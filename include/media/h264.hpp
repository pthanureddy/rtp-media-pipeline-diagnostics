#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace media {

enum class H264Packetization { single_nal, stap_a, fu_a };

struct H264PayloadInfo {
    H264Packetization packetization{};
    std::uint8_t nal_type{};
    bool keyframe{};
    bool fragment_start{};
    bool fragment_end{};
    std::vector<std::uint8_t> contained_nal_types;
};

std::optional<H264PayloadInfo> inspect_h264_payload(std::span<const std::uint8_t> payload,
                                                    std::string& error);

}  // namespace media
