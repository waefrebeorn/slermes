#!/usr/bin/env python3
"""Differential oracle for upstream Hermes pm/network.py (gate w5_network_5fdf96).

The tagged exception description is turned into a real Python exception and
handed to the live classifier / explainer / backoff calculator.
"""
import errno
import http.client
import importlib.util
import json
import socket
import ssl
import sys
import urllib.error

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/pm/network.py"
spec = importlib.util.spec_from_file_location("w5_network_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_network_live"] = live
spec.loader.exec_module(live)

def build_exc(d):
    k = d["kind"]
    if k == "http":
        return urllib.error.HTTPError("http://x", d.get("code", 0), "x", {}, None)
    if k == "url":
        return urllib.error.URLError(build_exc(d["reason"]))
    if k == "ssl_eof":
        return ssl.SSLEOFError()
    if k == "ssl_zero_return":
        return ssl.SSLZeroReturnError()
    if k == "ssl":
        return ssl.SSLError()
    if k == "gaierror":
        return socket.gaierror(socket.EAI_AGAIN if d.get("again") else 1, "x")
    if k == "timeout":
        return TimeoutError()
    if k == "connection":
        return ConnectionError()
    if k == "incomplete_read":
        return http.client.IncompleteRead(b"")
    if k == "os":
        return OSError(getattr(errno, d["errno_name"]), "x")
    return ValueError()

def httperr(code, headers, filename="http://x"):
    return urllib.error.HTTPError(filename, code, "x", headers, None)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    got = r.get("out")
    args = {k: v for k, v in r.items() if k not in ("func", "out")}
    try:
        if func == "transient":
            py = live.is_transient(build_exc(args["exc"]))
        elif func == "edge":
            py = live.edge_block(httperr(args.get("code", 0), args.get("headers") or {},
                                         args.get("filename", "http://x")))
        elif func == "delay":
            hdrs = {} if args.get("retry_after") is None else {"Retry-After": args["retry_after"]}
            py = live._delay(httperr(503, hdrs), args.get("attempt", 0))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR func={func} args={args}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH func={func} args={args} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_network_5fdf96: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
