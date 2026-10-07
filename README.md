# RTP Media Pipeline Diagnostics

A C++20 diagnostics toolkit for the media path between an IP camera and a video service. It validates RTP packets, inspects H.264 payload formats, tracks sequence loss and jitter, parses RTSP control messages, exercises UDP sockets, decodes STUN XOR-mapped addresses used in NAT discovery, and runs a real GStreamer pipeline probe.

The repository is a deterministic engineering lab. It does not claim to be a production streaming server, a complete RTSP client, or a WebRTC/ICE/TURN implementation.

## Implemented scope

- RTP v2 parser with CSRC, header-extension and padding validation.
- Stream monitor for sequence gaps, duplicate packets, out-of-order delivery and RFC 3550-style interarrival jitter.
- H.264 RTP payload inspection for single NAL units, STAP-A aggregation and FU-A fragmentation, including IDR detection.
- RTSP/1.0 request and response parser with CSeq and Content-Length validation.
- RTSP Transport parsing for unicast/multicast and RTP/RTCP client ports.
- Cross-platform UDP socket wrapper with timeout handling and a loopback integration test.
- STUN Binding Request encoding and IPv4 XOR-MAPPED-ADDRESS response parsing for NAT-discovery fundamentals.
- GStreamer pipeline executable that creates, runs and verifies a `videotestsrc` media pipeline through EOS.
- CMake, CTest, sanitizers, Docker and GitHub Actions for GCC and Clang.

## Architecture

```text
UDP datagram -> RTP parser -> sequence/jitter monitor -> H.264 payload inspector
                     |
RTSP text ---------->+---- control-plane validation
STUN response ------>+---- NAT-discovery evidence
GStreamer pipeline ->+---- multimedia framework probe
```

The core library has no third-party runtime dependency. GStreamer is linked only by the probe executable when development packages are available.

## Build and test on Linux

Prerequisites: C++20 compiler, CMake 3.20+, Ninja, pkg-config and GStreamer 1.0 development packages.

```bash
sudo apt-get install cmake ninja-build g++ pkg-config libgstreamer1.0-dev gstreamer1.0-tools gstreamer1.0-plugins-base
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMEDIA_ENABLE_SANITIZERS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/gstreamer-pipeline-probe
```

Or use the container build, which compiles, runs all tests and exercises GStreamer:

```bash
docker build -t rtp-media-pipeline-diagnostics .
docker run --rm rtp-media-pipeline-diagnostics --help
```

## RTP diagnostics CLI

Listen for ten RTP datagrams on loopback UDP port 5004:

```bash
./build/rtp-diagnostics --listen 5004 --packets 10
```

The result reports received packets, inferred sequence gaps, duplicates, out-of-order delivery, jitter in RTP clock ticks, valid H.264 payloads and malformed packets. The receiver binds to loopback intentionally; expanding the trust boundary requires an explicit deployment decision.

## GStreamer probe

The default self-contained pipeline requires no media files or camera:

```bash
./build/gstreamer-pipeline-probe
```

An alternate pipeline can be passed as one shell argument. For example, an engineer with an authorized RTSP endpoint and the required plugins could evaluate an RTSP/H.264 receive path with `rtspsrc`, `rtph264depay` and `h264parse`. Credentials and real camera URLs must not be committed.

## Verification

The test executable currently contains 30 deterministic tests covering malformed and valid RTP, H.264 packetization, RTSP parsing, transport negotiation fields, STUN transaction validation, sequence/jitter behavior and UDP loopback I/O. GitHub Actions builds with GCC and Clang under AddressSanitizer and UndefinedBehaviorSanitizer, runs CTest, executes the GStreamer probe and checks the CLI.

See [requirements and traceability](docs/requirements.md), [architecture](docs/architecture.md), and [protocol boundaries](docs/protocol-notes.md).

## Limitations

- RTP monitoring is single-SSRC and in-memory; it is not an RTCP implementation.
- Only H.264 single NAL, STAP-A and FU-A payloads are inspected; media is not decoded.
- STUN support is deliberately limited to Binding Request and IPv4 XOR-MAPPED-ADDRESS parsing.
- ICE candidate gathering/checks, TURN allocation/relaying, DTLS-SRTP, WebRTC signaling, congestion control, H.265 payload parsing and NAT simulation are out of scope.
- The GStreamer probe validates local pipeline construction and state transitions, not camera interoperability, latency or stream quality.
- No production availability, throughput, device compatibility or deployment claim is made.

## License

MIT. See [LICENSE](LICENSE).
