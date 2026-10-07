FROM ubuntu:24.04 AS build
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config libgstreamer1.0-dev \
    gstreamer1.0-tools gstreamer1.0-plugins-base ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure \
    && ./build/gstreamer-pipeline-probe

FROM ubuntu:24.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libgstreamer1.0-0 gstreamer1.0-plugins-base \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/build/rtp-diagnostics /usr/local/bin/rtp-diagnostics
COPY --from=build /src/build/gstreamer-pipeline-probe /usr/local/bin/gstreamer-pipeline-probe
ENTRYPOINT ["rtp-diagnostics"]
CMD ["--help"]
