# ── Slermes C Docker image ──
# Multi-stage build: compile static binary, copy into slim runtime image.
# Runtime deps: ca-certificates (for HTTPS), libssl3t64 (for crypto at runtime
# if the static fallback doesn't cover it).
#
# The binary is installed as `slermes`. A `hermes` symlink is also created
# for backward compatibility with workflows that reference the Python name.

# Stage 1: build
FROM debian:13-slim AS build
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        gcc make libssl-dev pkg-config ca-certificates && \
    rm -rf /var/lib/apt/lists/*
COPY . /build
WORKDIR /build
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
