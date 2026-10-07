#include "media/h264.hpp"
#include "media/rtp.hpp"
#include "media/udp_socket.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::uint16_t port = 5004U;
    unsigned packet_limit = 10U;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--listen" && i + 1 < argc) port = static_cast<std::uint16_t>(std::stoul(argv[++i]));
        else if (argument == "--packets" && i + 1 < argc) packet_limit = static_cast<unsigned>(std::stoul(argv[++i]));
        else if (argument == "--help") {
            std::cout << "Usage: rtp-diagnostics [--listen PORT] [--packets COUNT]\n";
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Unknown or incomplete argument: " << argument << '\n';
            return EXIT_FAILURE;
        }
    }

    media::UdpSocket socket;
    std::string error;
    if (!socket.bind(port, error)) {
        std::cerr << error << '\n';
        return EXIT_FAILURE;
    }
    media::RtpStreamMonitor monitor;
    unsigned h264_packets = 0U;
    unsigned malformed_packets = 0U;
    const auto started = std::chrono::steady_clock::now();

    for (unsigned i = 0; i < packet_limit; ++i) {
        auto datagram = socket.receive(65535U, std::chrono::milliseconds(3000), error);
        if (!datagram) {
            std::cerr << error << '\n';
            break;
        }
        auto packet = media::RtpPacket::parse(*datagram, error);
        if (!packet) {
            ++malformed_packets;
            continue;
        }
        const auto now = std::chrono::steady_clock::now();
        const double arrival_ms = std::chrono::duration<double, std::milli>(now - started).count();
        monitor.observe(*packet, arrival_ms);
        if (media::inspect_h264_payload(packet->payload, error)) ++h264_packets;
    }

    const auto& stats = monitor.stats();
    std::cout << "{\n"
              << "  \"received\": " << stats.received << ",\n"
              << "  \"estimated_lost\": " << stats.estimated_lost << ",\n"
              << "  \"duplicates\": " << stats.duplicates << ",\n"
              << "  \"out_of_order\": " << stats.out_of_order << ",\n"
              << "  \"jitter_ticks\": " << stats.interarrival_jitter_ticks << ",\n"
              << "  \"valid_h264_payloads\": " << h264_packets << ",\n"
              << "  \"malformed_rtp_packets\": " << malformed_packets << "\n"
              << "}\n";
    return EXIT_SUCCESS;
}
