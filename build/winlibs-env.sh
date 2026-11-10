#!/usr/bin/env bash
# build/winlibs-env.sh -- toolchain environment for building slermes on Windows.
#
# WHY THIS EXISTS
# MSYS2's gcc backend (cc1/cc1plus/collect2/lto1) stopped executing on this box
# on 2026-11-08: every one of them fails with STATUS_ACCESS_DENIED
# (0xC0000022) while the gcc.exe driver still runs. `pacman -Qk` reports all
# 1163 files present, the DLLs all resolve, and reinstalling the package changes
# nothing - the block is external (a Defender configuration change at 23:52
# that night). So the compiler is replaced, not repaired.
#
# WINLIBS
# winlibs GCC 16.2.0 (mingw-w64, ucrt, posix-seh) extracted at C:\winlibs\mingw64
# is self-contained and links cleanly on its own.
#
# THE -L GOTCHA
# The winlibs linker is a native Windows binary, so -L/LIBRARY_PATH arguments
# MUST be in Windows form (C:/...), not MSYS form (/c/...). A POSIX path is
# silently not searched and you get "cannot find -lz" for a library that is
# sitting right there.
#
# THE MIXING GOTCHA
# Do NOT point -L at C:\msys64\mingw64\lib wholesale: MSYS2's libgcc.a then
# wins over winlibs' and winlibs' own crt2.o fails with undefined references to
# _gnu_exception_handler / __mingw_initltssuo_force. Only the third-party
# archives are borrowed, and they live in C:\winlibs\extra-libs with nothing
# else in the directory.
#
# Usage:  source build/winlibs-env.sh && make all
export WINLIBS=/c/winlibs/mingw64

# GCC_EXEC_PREFIX makes gcc look for its subprograms AND its header search
# path under the given prefix. If a build/msys2-env.sh was ever sourced in
# the same shell, it is still pointing at C:/msys64/mingw64/bin/ -- winlibs
# gcc then finds an EMPTY include list and every <stddef.h>/<stdio.h> style
# include fails while the -I-pathed ones still compile, so the breakage is
# invisible until one file trips it. Always clear it here.
unset GCC_EXEC_PREFIX
unset COMPILER_PATH
export PATH="/c/winlibs/mingw64/bin:/c/msys64/usr/bin:/c/msys64/mingw64/bin:/usr/bin:/bin:$PATH"

# Headers and libraries are deliberately separated, and BOTH are isolated from
# MSYS2's trees. MSYS2's <string.h> declares strndup() under _GNU_SOURCE (which
# this build defines) while the winlibs UCRT runtime does not provide it, so
# including MSYS2's whole include tree makes the hermes_win32.h strndup shim a
# "static declaration follows non-static declaration" error. Borrow only the
# third-party header subtrees instead, so <string.h> resolves to winlibs' own.
#
#   extra-inc : SDL2/, tre/, openssl/, curl/, pcre2.h, zlib.h, zconf.h
#   extra-libs: libz libpcre2-8 libtre libssl libcrypto libSDL2 libSDL2_ttf
#               libSDL2main (.a and .dll.a)
#
# Do NOT add C:/msys64/mingw64/lib to LIBRARY_PATH - its libgcc.a then shadows
# winlibs' own and winlibs' crt2.o fails with undefined references to
# _gnu_exception_handler / __mingw_initltssuo_force.
export CFLAGS_EXTRA="-I C:/winlibs/extra-inc"
# CPATH does NOT work for this: the Windows-form path contains a colon, so
# gcc splits "C:/winlibs/extra-inc" into two entries ("C" and
# "/winlibs/extra-inc") and neither resolves. -I on the command line is
# literal and works. CFLAGS_EXTRA is referenced by build/config.mk.
export LIBRARY_PATH="C:/winlibs/extra-libs"
# run_one_oracle.sh uses this because the ccache wrapper strips LIBRARY_PATH
# from the compiler it execs - pass the borrowed lib dir explicitly instead.
export ORACLE_LIBDIR="C:/winlibs/extra-libs"

# ccache refuses to run without USERPROFILE (it uses it for its config/cache
# dir). The make invocation passes it as a variable, but every other consumer
# (the build-health gate spawns `sh -c` directly) needs it in the environment.
export USERPROFILE="C:/Users/eman5"
export LOCALAPPDATA="C:/Users/eman5/AppData/Local"
