#!/usr/bin/env bash
# build_third_party.sh — Download and build third-party libraries for Slermes C
# Usage: ./scripts/build_third_party.sh [whisper_cpp|ctranslate2|all]

set -euo pipefail

cd "$(dirname "$0")/.."

THIRD_PARTY_DIR="$(pwd)/lib"
WHISPER_CPP_VERSION="v1.7.5"
CTRANSLATE2_VERSION="v4.4.0"

log() { echo "[$(date '+%H:%M:%S')] $*"; }

build_whisper_cpp() {
    log "Building whisper.cpp ${WHISPER_CPP_VERSION}..."
    
    local SRC_DIR="${THIRD_PARTY_DIR}/whisper_cpp_src"
    local BUILD_DIR="${SRC_DIR}/build"
    local INSTALL_DIR="${THIRD_PARTY_DIR}/whisper_cpp"
    
    # Clone if not exists
    if [ ! -d "${SRC_DIR}/.git" ]; then
        log "Cloning whisper.cpp..."
        git clone --depth 1 --branch "${WHISPER_CPP_VERSION}" https://github.com/ggerganov/whisper.cpp "${SRC_DIR}"
    else
        log "whisper.cpp already cloned"
    fi
    
    # Build
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -DBUILD_SHARED_LIBS=OFF \
        -DWHISPER_BUILD_TESTS=OFF \
        -DWHISPER_BUILD_EXAMPLES=OFF \
        -DWHISPER_BUILD_SERVER=OFF \
        -DWHISPER_ALL_WARNINGS=OFF \
        -DGGML_NATIVE=ON \
        -DGGML_OPENMP=OFF \
        -DGGML_CUDA=OFF \
        2>&1 | tail -20
    
    make -j"$(nproc)" 2>&1 | tail -10
    make install 2>&1 | tail -10
    
    # Copy headers to include
    mkdir -p "${INSTALL_DIR}/include"
    cp -r "${SRC_DIR}/include/"* "${INSTALL_DIR}/include/" 2>/dev/null || true
    
    # Verify
    if [ -f "${INSTALL_DIR}/lib/libwhisper.a" ] || [ -f "${INSTALL_DIR}/lib/libwhisper.so" ]; then
        log "✓ whisper.cpp built successfully"
        ls -lh "${INSTALL_DIR}/lib/"
    else
        log "✗ whisper.cpp build failed - check output"
        return 1
    fi
}

build_ctranslate2() {
    log "Building CTranslate2 ${CTRANSLATE2_VERSION}..."
    
    local SRC_DIR="${THIRD_PARTY_DIR}/ctranslate2_src"
    local BUILD_DIR="${SRC_DIR}/build"
    local INSTALL_DIR="${THIRD_PARTY_DIR}/ctranslate2"
    
    # Clone if not exists
    if [ ! -d "${SRC_DIR}/.git" ]; then
        log "Cloning CTranslate2..."
        git clone --depth 1 --branch "${CTRANSLATE2_VERSION}" https://github.com/OpenNMT/CTranslate2 "${SRC_DIR}"
        cd "${SRC_DIR}"
        git submodule update --init --recursive
    else
        log "CTranslate2 already cloned"
    fi
    
    # Build
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -DBUILD_SHARED_LIBS=OFF \
        -DCT2_BUILD_TESTS=OFF \
        -DCT2_BUILD_TOOLS=OFF \
        -DWITH_CUDA=OFF \
        -DWITH_MKL=OFF \
        -DWITH_OPENMP=OFF \
        -DOPENMP_RUNTIME=COMP \
        -DOPENMP_CXX_FLAGS="" \
        -DOpenMP_CXX_LIB_NAMES="" \
        2>&1 | tail -20
    
    make -j"$(nproc)" 2>&1 | tail -10
    make install 2>&1 | tail -10
    
    # Verify
    if [ -f "${INSTALL_DIR}/lib/libctranslate2.a" ] || [ -f "${INSTALL_DIR}/lib/libctranslate2.so" ]; then
        log "✓ CTranslate2 built successfully"
        ls -lh "${INSTALL_DIR}/lib/"
    else
        log "⚠ CTranslate2 build failed (optional - whisper.cpp provides core inference)"
        return 0
    fi
}

case "${1:-all}" in
    whisper_cpp)
        build_whisper_cpp
        ;;
    ctranslate2)
        build_ctranslate2
        ;;
    all)
        build_whisper_cpp
        build_ctranslate2
        ;;
    *)
        echo "Usage: $0 [whisper_cpp|ctranslate2|all]"
        exit 1
        ;;
esac

log "Third-party build complete"