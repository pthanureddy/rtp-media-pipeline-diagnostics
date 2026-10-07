#include "media/h264.hpp"
#include "media/rtp.hpp"
#include "media/rtsp.hpp"
#include "media/stun.hpp"
#include "media/udp_socket.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using Test = std::pair<std::string, std::function<void()>>;
int assertions = 0;

#define CHECK(condition) do { ++assertions; if (!(condition)) throw std::runtime_error(#condition); } while (false)

std::vector<std::uint8_t> basic_rtp(std::uint16_t sequence, std::uint32_t timestamp,
                                    const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> result = {0x80U, 0xE0U,
        static_cast<std::uint8_t>(sequence >> 8U), static_cast<std::uint8_t>(sequence),
        static_cast<std::uint8_t>(timestamp >> 24U), static_cast<std::uint8_t>(timestamp >> 16U),
        static_cast<std::uint8_t>(timestamp >> 8U), static_cast<std::uint8_t>(timestamp),
        0x12U, 0x34U, 0x56U, 0x78U};
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

std::vector<std::uint8_t> stun_response(const std::array<std::uint8_t, 12>& transaction,
                                        std::uint32_t ipv4, std::uint16_t port) {
    constexpr std::uint32_t cookie = media::stun_magic_cookie;
    const std::uint16_t xor_port = port ^ static_cast<std::uint16_t>(cookie >> 16U);
    const std::uint32_t xor_ip = ipv4 ^ cookie;
    std::vector<std::uint8_t> bytes = {0x01,0x01,0x00,0x0C,0x21,0x12,0xA4,0x42};
    bytes.insert(bytes.end(), transaction.begin(), transaction.end());
    bytes.insert(bytes.end(), {0x00,0x20,0x00,0x08,0x00,0x01,
        static_cast<std::uint8_t>(xor_port >> 8U), static_cast<std::uint8_t>(xor_port),
        static_cast<std::uint8_t>(xor_ip >> 24U), static_cast<std::uint8_t>(xor_ip >> 16U),
        static_cast<std::uint8_t>(xor_ip >> 8U), static_cast<std::uint8_t>(xor_ip)});
    return bytes;
}

}  // namespace

int main() {
    const std::vector<Test> tests = {
        {"RTP basic header", [] { std::string e; auto p=media::RtpPacket::parse(basic_rtp(42,99,{0x65}),e); CHECK(p); CHECK(p->sequence==42); CHECK(p->marker); CHECK(p->payload_type==96); }},
        {"RTP short header rejected", [] { std::string e; CHECK(!media::RtpPacket::parse(std::vector<std::uint8_t>(11),e)); }},
        {"RTP version rejected", [] { auto b=basic_rtp(1,1,{1}); b[0]=0x40; std::string e; CHECK(!media::RtpPacket::parse(b,e)); }},
        {"RTP CSRC parsed", [] { auto b=basic_rtp(1,1,{1}); b[0]=0x81; b.insert(b.begin()+12,{0,0,0,7}); std::string e; auto p=media::RtpPacket::parse(b,e); CHECK(p); CHECK(p->csrcs.at(0)==7); }},
        {"RTP extension parsed", [] { auto b=basic_rtp(1,1,{1}); b[0]=0x90; b.insert(b.begin()+12,{0xBE,0xDE,0,1,1,2,3,4}); std::string e; auto p=media::RtpPacket::parse(b,e); CHECK(p); CHECK(p->extension_profile==0xBEDE); CHECK(p->extension_data.size()==4); }},
        {"RTP truncated extension rejected", [] { auto b=basic_rtp(1,1,{1}); b[0]=0x90; b.insert(b.begin()+12,{0xBE,0xDE,0,2,1,2}); std::string e; CHECK(!media::RtpPacket::parse(b,e)); }},
        {"RTP padding removed", [] { auto b=basic_rtp(1,1,{0x65,0,0,3}); b[0]=0xA0; std::string e; auto p=media::RtpPacket::parse(b,e); CHECK(p); CHECK(p->payload.size()==1); }},
        {"RTP bad padding rejected", [] { auto b=basic_rtp(1,1,{0x65,9}); b[0]=0xA0; std::string e; CHECK(!media::RtpPacket::parse(b,e)); }},
        {"H264 IDR detected", [] { std::string e; auto h=media::inspect_h264_payload(std::array<std::uint8_t,2>{0x65,1},e); CHECK(h); CHECK(h->keyframe); CHECK(h->nal_type==5); }},
        {"H264 SPS detected", [] { std::string e; auto h=media::inspect_h264_payload(std::array<std::uint8_t,2>{0x67,1},e); CHECK(h); CHECK(h->nal_type==7); CHECK(!h->keyframe); }},
        {"H264 FU-A start", [] { std::string e; auto h=media::inspect_h264_payload(std::array<std::uint8_t,3>{0x7C,0x85,1},e); CHECK(h); CHECK(h->fragment_start); CHECK(h->keyframe); }},
        {"H264 FU-A end", [] { std::string e; auto h=media::inspect_h264_payload(std::array<std::uint8_t,3>{0x7C,0x45,1},e); CHECK(h); CHECK(h->fragment_end); }},
        {"H264 invalid FU-A rejected", [] { std::string e; CHECK(!media::inspect_h264_payload(std::array<std::uint8_t,2>{0x7C,0xC5},e)); }},
        {"H264 STAP-A parsed", [] { std::string e; auto h=media::inspect_h264_payload(std::array<std::uint8_t,8>{0x78,0,2,0x67,1,0,1,0x65},e); CHECK(h); CHECK(h->contained_nal_types.size()==2); CHECK(h->keyframe); }},
        {"H264 truncated STAP-A rejected", [] { std::string e; CHECK(!media::inspect_h264_payload(std::array<std::uint8_t,4>{0x78,0,5,0x67},e)); }},
        {"RTSP request parsed", [] { std::string e; auto m=media::RtspMessage::parse("OPTIONS rtsp://camera/live RTSP/1.0\r\nCSeq: 1\r\n\r\n",e); CHECK(m); CHECK(m->request); CHECK(m->method=="OPTIONS"); }},
        {"RTSP response parsed", [] { std::string e; auto m=media::RtspMessage::parse("RTSP/1.0 200 OK\r\nCSeq: 2\r\n\r\n",e); CHECK(m); CHECK(m->status_code==200); }},
        {"RTSP case-insensitive header", [] { std::string e; auto m=media::RtspMessage::parse("PLAY x RTSP/1.0\r\ncSeQ: 7\r\n\r\n",e); CHECK(m); CHECK(m->header("CSEQ")=="7"); }},
        {"RTSP missing CSeq rejected", [] { std::string e; CHECK(!media::RtspMessage::parse("PLAY x RTSP/1.0\r\nUser-Agent: test\r\n\r\n",e)); }},
        {"RTSP bad content length rejected", [] { std::string e; CHECK(!media::RtspMessage::parse("SET_PARAMETER x RTSP/1.0\r\nCSeq: 3\r\nContent-Length: 8\r\n\r\nabc",e)); }},
        {"RTSP transport ports", [] { std::string e; auto t=media::parse_rtsp_transport("RTP/AVP;unicast;client_port=5000-5001",e); CHECK(t); CHECK(t->unicast); CHECK(t->client_rtp_port==5000); CHECK(t->client_rtcp_port==5001); }},
        {"RTSP bad port rejected", [] { std::string e; CHECK(!media::parse_rtsp_transport("RTP/AVP;client_port=70000",e)); }},
        {"STUN request encoded", [] { std::array<std::uint8_t,12> id{}; id[0]=9; auto b=media::make_stun_binding_request(id); CHECK(b.size()==20); CHECK(b[0]==0 && b[1]==1); CHECK(b[8]==9); }},
        {"STUN XOR address parsed", [] { std::array<std::uint8_t,12> id{}; id[2]=4; auto b=stun_response(id,0xC0000201U,3478); std::string e; auto a=media::parse_stun_binding_response(b,id,e); CHECK(a); CHECK(a->ipv4=="192.0.2.1"); CHECK(a->port==3478); }},
        {"STUN transaction mismatch", [] { std::array<std::uint8_t,12> id{}; auto b=stun_response(id,0xC0000201U,3478); auto other=id; other[0]=1; std::string e; CHECK(!media::parse_stun_binding_response(b,other,e)); }},
        {"RTP sequence gap counted", [] { std::string e; auto p1=media::RtpPacket::parse(basic_rtp(10,100,{0x65}),e); auto p2=media::RtpPacket::parse(basic_rtp(13,200,{0x65}),e); media::RtpStreamMonitor m; m.observe(*p1,1); m.observe(*p2,2); CHECK(m.stats().estimated_lost==2); }},
        {"RTP duplicate counted", [] { std::string e; auto p=media::RtpPacket::parse(basic_rtp(10,100,{0x65}),e); media::RtpStreamMonitor m; m.observe(*p,1); m.observe(*p,2); CHECK(m.stats().duplicates==1); }},
        {"RTP out of order counted", [] { std::string e; auto p1=media::RtpPacket::parse(basic_rtp(10,100,{0x65}),e); auto p2=media::RtpPacket::parse(basic_rtp(12,200,{0x65}),e); auto p3=media::RtpPacket::parse(basic_rtp(11,150,{0x65}),e); media::RtpStreamMonitor m; m.observe(*p1,1); m.observe(*p2,2); m.observe(*p3,3); CHECK(m.stats().out_of_order==1); }},
        {"RTP jitter updated", [] { std::string e; auto p1=media::RtpPacket::parse(basic_rtp(1,0,{0x65}),e); auto p2=media::RtpPacket::parse(basic_rtp(2,9000,{0x65}),e); media::RtpStreamMonitor m; m.observe(*p1,0); m.observe(*p2,120); CHECK(m.stats().interarrival_jitter_ticks>0); }},
        {"UDP loopback", [] { media::UdpSocket s; std::string e; CHECK(s.bind(0,e)); const auto port=s.local_port(e); CHECK(port>0); std::vector<std::uint8_t> data{1,2,3}; CHECK(media::UdpSocket::send_to("127.0.0.1",port,data,e)); auto received=s.receive(32,std::chrono::milliseconds(500),e); CHECK(received); CHECK(*received==data); }}
    };

    int failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& exception) { ++failures; std::cerr << "FAIL " << name << ": " << exception.what() << '\n'; }
    }
    std::cout << tests.size() << " tests, " << assertions << " assertions, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
