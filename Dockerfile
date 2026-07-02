# ── Slermes C Docker image ──
# Multi-stage build: compile static binary, copy into slim runtime image.
# Pure C11 — no C++, no ncurses, no Wayland needed in Docker.
#
# The binary is installed as `slermes`. A `hermes` symlink is also created
# for backward compatibility with workflows that reference the Python name.

# Stage 1: build
FROM debian:13-slim AS build
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        gcc make libssl-dev pkg-config ca-certificates \
        unzip curl && \
    rm -rf /var/lib/apt/lists/*
COPY . /build
WORKDIR /build
RUN mkdir -p lib/syslib && ln -sf /usr/lib/x86_64-linux-gnu/libncursesw.so.6 lib/syslib/libncursesw.so && \
    ln -sf /usr/lib/x86_64-linux-gnu/libtinfo.so.6 lib/syslib/libtinfo.so && \
    ln -sf /usr/lib/x86_64-linux-gnu/libpanelw.so.6 lib/syslib/libpanelw.so
RUN make deps
RUN bash ./scripts/build_third_party.sh whisper_cpp 2>&1 | tail -5 || echo "whisper skipped"
RUN make -j$(nproc)
RUN make static 2>/dev/null; true

# Stage 2: runtime
FROM debian:13-slim
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates libssl3t64 && \
    rm -rf /var/lib/apt/lists/*

# Copy binary from build stage
COPY --from=build /build/slermes /usr/local/bin/slermes
RUN ln -s slermes /usr/local/bin/hermes

# Runtime volume for user data
VOLUME [ "/opt/data" ]
ENV SLERMES_HOME=/opt/data

# Create non-root user
RUN useradd -m -d /opt/data slermes && \
    mkdir -p /opt/data && \
    chown -R slermes:slermes /opt/data

USER slermes
ENTRYPOINT ["slermes"]
CMD ["--help"]
