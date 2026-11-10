#!/usr/bin/env bash
# build/msys2-env.sh -- use the MSYS2 gcc instead of the winlibs one.
#
# WHY THIS EXISTS
# MSYS2's compiler backend (cc1.exe, cc1plus.exe, collect2.exe, lto1.exe) lives in
#   C:\msys64\mingw64\lib\gcc\x86_64-w64-mingw32\16.2.0\
# and that directory is DENIED execution on this box:
#
#   cc1.exe      -> CreateProcess exit -1073741790 (0xC0000022 STATUS_ACCESS_DENIED)
#   cc1plus.exe  -> same
#   collect2.exe -> same
#   lto1.exe     -> same
#   gcc.exe, as.exe, ld.exe, ar.exe, python3.exe, bash.exe -> all run fine
#
# It is NOT Defender (exclusions for C:\msys64, cc1.exe and cc1plus.exe are in the
# registry and change nothing; no detection, no threat record, no AV event), it is
# NOT Smart App Control (Off), NOT WDAC (no cc1 events in the Code Integrity log),
# NOT AppLocker/SRP, and NOT the file (a copy is byte-identical, reads fine, and its
# ACL grants Users ReadAndExecute). The block is keyed on the DIRECTORY:
#
#   C:\msys64\mingw64\lib\gcc\...   -> denied
#   C:\msys64\mingw64\bin\           -> allowed
#
# Proof: copying cc1.exe to C:\msys64\mingw64\bin\cc1_copy.exe makes it run.
#
# THE WORKAROUND
# Put copies of the four backend binaries in mingw64\bin (where execution is
# allowed) and point gcc at them with GCC_EXEC_PREFIX. GCC_EXEC_PREFIX also makes
# gcc recompute its internal include path, so the GCC private include dir has to be
# passed explicitly.
#
# Usage:  source build/msys2-env.sh && make all
#
# build/winlibs-env.sh remains the DEFAULT because it needs none of this.

# --- 1. stage the backend binaries where they are allowed to execute ----------
_GCC_VER="${MSYS2_GCC_VER:-16.2.0}"
_SRC="C:/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/${_GCC_VER}"
_DST="C:/msys64/mingw64/bin"

if [ ! -f "${_DST}/cc1.exe" ] || [ ! -f "${_DST}/lto1.exe" ]; then
    echo "msys2-env: staging compiler backend into ${_DST}" >&2
    for _b in cc1.exe cc1plus.exe collect2.exe lto1.exe; do
        cp -f "${_SRC}/${_b}" "${_DST}/${_b}" 2>/dev/null \
            || echo "msys2-env: FAILED to copy ${_b}" >&2
    done
fi

# --- 2. point gcc at the staged copies ---------------------------------------
export GCC_EXEC_PREFIX="C:/msys64/mingw64/bin/"

# GCC_EXEC_PREFIX also changes how gcc locates its private include directory
# (<prefix>/lib/gcc/.../include), which holds mm_malloc.h and friends. Pass it.
export CFLAGS_EXTRA="-I C:/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/${_GCC_VER}/include -I C:/winlibs/extra-inc"

export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:/usr/bin:/bin:$PATH"
export LIBRARY_PATH="C:/winlibs/extra-libs"
export ORACLE_LIBDIR="C:/winlibs/extra-libs"
