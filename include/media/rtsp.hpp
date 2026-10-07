#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace media {

struct RtspMessage {
    bool request{};
    std::string method;
    std::string uri;
    int status_code{};
    std::string reason;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    [[nodiscard]] std::optional<std::string> header(std::string_view name) const;
    static std::optional<RtspMessage> parse(std::string_view wire, std::string& error);
};

struct RtspTransport {
    bool unicast{};
    bool multicast{};
    std::optional<unsigned> client_rtp_port;
    std::optional<unsigned> client_rtcp_port;
};

std::optional<RtspTransport> parse_rtsp_transport(std::string_view value, std::string& error);

}  // namespace media
