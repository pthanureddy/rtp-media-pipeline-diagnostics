# Requirements and Traceability

| ID | Requirement | Implementation | Verification |
|---|---|---|---|
| RTP-01 | Reject truncated packets and non-v2 headers | `src/rtp.cpp` | Short-header and version tests |
| RTP-02 | Parse CSRC, extension and padding safely | `src/rtp.cpp` | CSRC, extension, padding and truncation tests |
| RTP-03 | Track gaps, duplicates, ordering and jitter | `RtpStreamMonitor` | Four stream-monitor tests |
| AVC-01 | Identify H.264 single NAL types and IDR frames | `src/h264.cpp` | IDR and SPS tests |
| AVC-02 | Validate STAP-A and FU-A payload forms | `src/h264.cpp` | Five aggregation/fragmentation tests |
| RTSP-01 | Parse RTSP/1.0 requests and responses | `src/rtsp.cpp` | Request, response and header tests |
| RTSP-02 | Validate CSeq and Content-Length | `src/rtsp.cpp` | Missing CSeq and truncated-body tests |
| RTSP-03 | Extract RTP/RTCP client ports | `parse_rtsp_transport` | Valid and invalid port tests |
| NET-01 | Receive and send loopback UDP datagrams with timeout | `UdpSocket` | UDP loopback integration test |
| NAT-01 | Encode a STUN Binding Request | `make_stun_binding_request` | Request wire-format test |
| NAT-02 | Validate transaction ID and decode IPv4 XOR address | `parse_stun_binding_response` | Success and mismatch tests |
| GST-01 | Build and run a GStreamer pipeline to EOS | `gstreamer-pipeline-probe` | Local/Docker and CI command |

These requirements cover controlled protocol parsing and diagnostics. They do not imply a complete RTP/RTCP, RTSP, ICE, TURN, WebRTC or codec implementation.
