#include "media/h264.hpp"

namespace media {

std::optional<H264PayloadInfo> inspect_h264_payload(std::span<const std::uint8_t> payload,
                                                    std::string& error) {
    error.clear();
    if (payload.empty()) {
        error = "H.264 RTP payload is empty";
        return std::nullopt;
    }
    const std::uint8_t type = payload[0] & 0x1FU;
    H264PayloadInfo info;

    if (type >= 1U && type <= 23U) {
        info.packetization = H264Packetization::single_nal;
        info.nal_type = type;
        info.keyframe = type == 5U;
        info.contained_nal_types.push_back(type);
        return info;
    }

    if (type == 24U) {
        info.packetization = H264Packetization::stap_a;
        info.nal_type = type;
        std::size_t offset = 1U;
        while (offset < payload.size()) {
            if (offset + 2U > payload.size()) {
                error = "STAP-A NAL length is truncated";
                return std::nullopt;
            }
            const std::size_t nal_size = (static_cast<std::size_t>(payload[offset]) << 8U) |
                                         static_cast<std::size_t>(payload[offset + 1U]);
            offset += 2U;
            if (nal_size == 0U || offset + nal_size > payload.size()) {
                error = "STAP-A NAL unit length is invalid";
                return std::nullopt;
            }
            const std::uint8_t contained_type = payload[offset] & 0x1FU;
            info.contained_nal_types.push_back(contained_type);
            info.keyframe = info.keyframe || contained_type == 5U;
            offset += nal_size;
        }
        return info;
    }

    if (type == 28U) {
        if (payload.size() < 2U) {
            error = "FU-A payload is missing the fragment header";
            return std::nullopt;
        }
        info.packetization = H264Packetization::fu_a;
        info.nal_type = payload[1] & 0x1FU;
        info.fragment_start = (payload[1] & 0x80U) != 0U;
        info.fragment_end = (payload[1] & 0x40U) != 0U;
        if (info.fragment_start && info.fragment_end) {
            error = "FU-A fragment cannot be both start and end";
            return std::nullopt;
        }
        info.keyframe = info.nal_type == 5U;
        info.contained_nal_types.push_back(info.nal_type);
        return info;
    }

    error = "Unsupported H.264 RTP packetization type";
    return std::nullopt;
}

}  // namespace media
