#include "media/rtsp.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <sstream>
#include <vector>

namespace media {
namespace {

std::string trim(std::string value) {
    const auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t end = value.find(delimiter, start);
        parts.emplace_back(value.substr(start, end == std::string_view::npos ? value.size() - start : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1U;
    }
    return parts;
}

}  // namespace

std::optional<std::string> RtspMessage::header(std::string_view name) const {
    const auto found = headers.find(lower(name));
    if (found == headers.end()) return std::nullopt;
    return found->second;
}

std::optional<RtspMessage> RtspMessage::parse(std::string_view wire, std::string& error) {
    error.clear();
    const std::size_t boundary = wire.find("\r\n\r\n");
    if (boundary == std::string_view::npos) {
        error = "RTSP message is missing the header terminator";
        return std::nullopt;
    }
    std::istringstream stream{std::string(wire.substr(0, boundary))};
    std::string first_line;
    std::getline(stream, first_line);
    if (!first_line.empty() && first_line.back() == '\r') first_line.pop_back();

    RtspMessage message;
    std::istringstream first(first_line);
    std::string version;
    if (first_line.rfind("RTSP/", 0) == 0) {
        message.request = false;
        if (!(first >> version >> message.status_code) || version != "RTSP/1.0") {
            error = "Invalid RTSP status line";
            return std::nullopt;
        }
        std::getline(first, message.reason);
        message.reason = trim(message.reason);
    } else {
        message.request = true;
        if (!(first >> message.method >> message.uri >> version) || version != "RTSP/1.0") {
            error = "Invalid RTSP request line";
            return std::nullopt;
        }
    }

    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            error = "Invalid RTSP header line";
            return std::nullopt;
        }
        message.headers[lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1U));
    }

    const auto cseq = message.header("cseq");
    if (!cseq || cseq->empty()) {
        error = "RTSP CSeq header is required";
        return std::nullopt;
    }

    const std::string_view available_body = wire.substr(boundary + 4U);
    std::size_t expected_body = available_body.size();
    if (const auto content_length = message.header("content-length")) {
        expected_body = 0U;
        const char* begin = content_length->data();
        const char* end = begin + content_length->size();
        const auto parsed = std::from_chars(begin, end, expected_body);
        if (parsed.ec != std::errc{} || parsed.ptr != end || expected_body > available_body.size()) {
            error = "Invalid RTSP Content-Length";
            return std::nullopt;
        }
    }
    message.body = std::string(available_body.substr(0, expected_body));
    return message;
}

std::optional<RtspTransport> parse_rtsp_transport(std::string_view value, std::string& error) {
    error.clear();
    RtspTransport transport;
    for (auto token : split(value, ';')) {
        token = trim(token);
        const std::string normalized = lower(token);
        if (normalized == "unicast") transport.unicast = true;
        if (normalized == "multicast") transport.multicast = true;
        constexpr std::string_view prefix = "client_port=";
        if (normalized.rfind(prefix, 0) == 0) {
            const std::string ports = normalized.substr(prefix.size());
            const std::size_t dash = ports.find('-');
            unsigned rtp = 0;
            unsigned rtcp = 0;
            const char* rtp_begin = ports.data();
            const char* rtp_end = ports.data() + (dash == std::string::npos ? ports.size() : dash);
            auto rtp_result = std::from_chars(rtp_begin, rtp_end, rtp);
            if (rtp_result.ec != std::errc{} || rtp_result.ptr != rtp_end || rtp > 65535U) {
                error = "Invalid RTSP client RTP port";
                return std::nullopt;
            }
            transport.client_rtp_port = rtp;
            if (dash != std::string::npos) {
                const char* rtcp_begin = ports.data() + dash + 1U;
                const char* rtcp_end = ports.data() + ports.size();
                auto rtcp_result = std::from_chars(rtcp_begin, rtcp_end, rtcp);
                if (rtcp_result.ec != std::errc{} || rtcp_result.ptr != rtcp_end || rtcp > 65535U) {
                    error = "Invalid RTSP client RTCP port";
                    return std::nullopt;
                }
                transport.client_rtcp_port = rtcp;
            }
        }
    }
    return transport;
}

}  // namespace media
