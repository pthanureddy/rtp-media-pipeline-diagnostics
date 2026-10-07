# Architecture

## Boundaries

`media_diagnostics` is a dependency-free C++ library. Wire-format parsers return explicit failure details and never read beyond validated lengths. UDP I/O is isolated from parsing so byte fixtures remain deterministic and fuzzing can be added without network setup.

The `rtp-diagnostics` executable owns transport timing and feeds validated packets into the stream monitor and H.264 inspector. It emits one compact summary after a bounded packet count or timeout.

The `gstreamer-pipeline-probe` executable is deliberately separate. CMake builds it only when `gstreamer-1.0` is discoverable through pkg-config, keeping core compilation possible on machines without multimedia packages while ensuring CI exercises the real framework API.

## Error model

Parsing functions return `std::optional` and populate an explanatory error string. Network methods distinguish bind, timeout, receive and destination failures. The CLI counts malformed RTP separately from packets with unsupported H.264 payload types.

## Security and reliability notes

- All length fields are checked before slicing.
- The UDP receiver binds to loopback by default.
- No external URLs, credentials or media files are required by tests.
- CI runs the C++ tests with address and undefined-behavior sanitizers under two compilers.
- No parser result is treated as decoded or authenticated media.
