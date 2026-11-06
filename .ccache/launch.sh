#!/usr/bin/env bash
# ccache_launch.sh - set repo-local ccache env and exec the real compiler.
# Usage (as a compiler wrapper):
#   CC="$(pwd)/.ccache/launch.sh gcc"      # or -C CANAME
#   CXX="$(pwd)/.ccache/launch.sh g++"
# Every MSYS2 ccache invocation needs USERPROFILE/HOME/LOCALAPPDATA (MSYS2 does
# not inherit them the way a native shell does), and the cache is pinned to a
# repo-local dir so it is self-contained and never touches the user's home.
#
# The wrapper is used by build/config.mk (CC/CCC via CCACHE_PREFIX), the
# standalone oracle gcc line in run_one_oracle.sh, and any manual compile.

REPO="$(cd "$(dirname "$0")/.." && pwd)"
CACHE_DIR="$REPO/.ccache"

# ccache 4.x wants these; without them it errors out before compiling.
export USERPROFILE="${USERPROFILE:-/c/Users/eman5}"
export HOME="${HOME:-$USERPROFILE}"
export LOCALAPPDATA="${LOCALAPPDATA:-$USERPROFILE/AppData/Local}"
export CCACHE_DIR="$CACHE_DIR"
export CCACHE_CONFIGPATH="$CACHE_DIR/ccache.conf"

if [ ! -f "$CCACHE_CONFIGPATH" ]; then
  mkdir -p "$CACHE_DIR"
  printf 'cache_dir = %s\nmax_size = 3G\n' "C:/Users/eman5/slermes/.ccache" > "$CCACHE_CONFIGPATH"
fi

# First non-option arg is the real compiler (gcc/g++/cc/clang...). If none,
# default to gcc. Remaining args are passed through unchanged.
COMP=""
CARGS=()
for a in "$@"; do
  if [ -z "$COMP" ] && [[ "$a" != -* ]]; then
    COMP="$a"
  else
    CARGS+=("$a")
  fi
done
[ -n "$COMP" ] || COMP=gcc

exec ccache "$COMP" "${CARGS[@]}"
