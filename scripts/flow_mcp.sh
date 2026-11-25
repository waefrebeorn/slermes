#!/usr/bin/env bash
# End-to-end flow: start the MCP server, speak JSON-RPC to it, get real answers, stop it.
#
# This is the first test in the repo that exercises the product rather than a pure
# function. It exists because mcp-serve used to print "Failed to start server" -- the real
# 400-line implementation was never in build/objects.mk and four stubs shadowed its API --
# and then, once wired, it accepted connections and answered nothing, because a SOCKET is
# not a file descriptor on Windows and read()/write() fail on one.
set -u
cd "$(dirname "$0")/.." || exit 1
PORT=${PORT:-9100}
URL="http://127.0.0.1:$PORT/mcp"
BIN=./slermes.exe

[ -x "$BIN" ] || { echo "flow-mcp: no $BIN"; exit 2; }
bash scripts/bundle_dlls.sh >/dev/null 2>&1

"$BIN" mcp-serve > .testbin/flow-mcp.log 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null' EXIT
sleep 2

fail=0
probe() { # name, payload, needle
  out=$(curl -s -m 8 -X POST "$URL" -H 'Content-Type: application/json' -d "$2")
  case "$out" in
    *"$3"*) echo "   ok    $1" ;;
    *)      echo "   FAIL  $1 -> ${out:-<empty>}"; fail=1 ;;
  esac
}

probe "initialize"  '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' '"serverInfo"'
probe "tools/list"  '{"jsonrpc":"2.0","id":2,"method":"tools/list"}'              '"tools"'
# A listing is not execution: read_file must return the repository README through the same
# JSON-RPC path, or the registry is only a table of names.
probe "tools/call"  '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"read_file","arguments":{"path":"README.md","limit":2}}}' 'Slermes'
# Every published schema must be a usable JSON-Schema object; a typo in one C string literal is
# otherwise invisible until a client tries to consume it.
if command -v python >/dev/null 2>&1; then
  if python scripts/check_tool_schemas.py --url "$URL" >> .testbin/flow-mcp.log 2>&1; then
    echo "   ok    tool schemas"
  else
    echo "   FAIL  tool schemas (see .testbin/flow-mcp.log)"; fail=1
  fi
fi

if [ $fail -ne 0 ]; then
  echo "flow-mcp: FAIL"; sed -n '1,10p' .testbin/flow-mcp.log; exit 1
fi
echo "flow-mcp: PASS (end-to-end JSON-RPC answered on :$PORT)"
