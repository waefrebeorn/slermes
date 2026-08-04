# Slermes Docker Image
# Build: docker build -t slermes:latest -f packaging/docker/Dockerfile .
# Run:   docker run -it --rm -v ~/.slermes:/home/slermes/.slermes slermes:latest

# ─── Build Stage ───────────────────────────────────────────────────
FROM alpine:3.19 AS builder

RUN apk add --no-cache \
    gcc \
    musl-dev \
    openssl-dev \
    openssl-libs-static \
    ncurses-dev \
    ncurses-static \
    wayland-dev \
    wayland-libs-client \
    libxkbcommon-dev \
    mesa-dev \
    pcre2-dev \
    sdl2-dev \
    sdl2_ttf-dev \
    zlib-dev \
    zlib-static \
    pkgconf \
    make \
    bash \
    curl \
    unzip \
    file

WORKDIR /build

# Copy source
COPY . .

# Fetch SQLite amalgamation + build static binary
RUN make deps 2>/dev/null; \
    make -j$(nproc) static 2>&1 || \
    make -j$(nproc) all 2>&1

# ─── Runtime Stage ─────────────────────────────────────────────────
FROM alpine:3.19

RUN apk add --no-cache \
    ca-certificates \
    tzdata \
    && adduser -D -h /home/slermes slermes

# Copy binary from builder
COPY --from=builder /build/slermes /usr/local/bin/slermes

# Create config directory
RUN mkdir -p /home/slermes/.slermes && chown -R slermes:slermes /home/slermes

USER slermes
WORKDIR /home/slermes

# Health check
HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
    CMD slermes --version || exit 1

ENTRYPOINT ["slermes"]
CMD ["--help"]
