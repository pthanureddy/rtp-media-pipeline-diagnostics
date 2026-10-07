#!/usr/bin/env bash
set -euo pipefail
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMEDIA_ENABLE_SANITIZERS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/gstreamer-pipeline-probe
./build/rtp-diagnostics --help
