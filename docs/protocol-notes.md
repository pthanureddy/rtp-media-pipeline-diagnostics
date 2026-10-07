# Protocol Boundaries

## RTP and H.264

RTP header validation follows the version-2 fixed header layout and accounts for CSRC, extension and padding lengths. The sequence monitor uses 16-bit modular distance and reports inferred gaps; it does not implement the full probation, dropout and restart rules of an RTCP receiver report.

H.264 inspection covers RFC 6184 single NAL, STAP-A and FU-A forms. It identifies payload structure and IDR fragments but does not reassemble access units, decode pictures or validate SPS/PPS semantics.

## RTSP

The parser covers RTSP/1.0 start lines, headers, body length and common Transport fields. It does not implement authentication, session state, retransmission, interleaved RTP over TCP or a camera client.

## STUN, ICE and TURN

STUN request/response handling demonstrates the address-discovery mechanism used by ICE: a transaction-scoped Binding Request and XOR-MAPPED-ADDRESS decoding. Full ICE requires candidate gathering, pair prioritization, connectivity checks, nomination and consent. TURN adds allocation, permission, channel and relay lifecycles. Those mechanisms are documented but not claimed as implemented here.

## GStreamer

The probe uses the GStreamer C API to parse a pipeline, transition it to PLAYING and require EOS. The default source and sink are deterministic. An RTSP receive pipeline requires authorized infrastructure and plugins, so it is intentionally not part of public CI.
