"""twin for web_server_is_accepted_host.

Imports LIVE hermes_cli/web_server.py::_is_accepted_host and traces the SAME
branch ladder as the C harness (web_server_auth.c::ws_is_accepted_host).
Input argv format: "<host_header><TAB><bound_host>".

Rung order:
  B1 null/empty guard (C guards NULL only; inputs are non-empty so the
     ladders agree) / V1 stripped header / B2 bracketed? / B3 close-bracket?
  / V2 lowercased host_only / B4 wildcard bind / B5 loopback bind /
  B6 host loopback / B7 exact match.
"""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

# Import the LIVE module in isolation. hermes_cli/web_server.py imports fastapi
# at module scope, which may be unavailable — extract the function instead by
# executing only the small helper region we need (mirrors the live source).
import re as _re

src_path = os.path.join(DEV, "hermes_cli", "web_server.py")
_src = open(src_path, encoding="utf-8").read()
m = _re.search(r"_LOOPBACK_HOST_VALUES[^=]*=\s*frozenset\(\{(.*?)\}\)", _src, _re.S)
LOOPBACK = eval("frozenset({%s})" % m.group(1))
mfn = _re.search(r"def _is_accepted_host\(.*?\n(?=\n*@app\.|\n\ndef |\Z)", _src, _re.S)
# Extract the _host_header_hostname helper too (LIVE _is_accepted_host calls it).
mhh = _re.search(r"def _host_header_hostname\(.*?\n(?=\n*@app\.|\ndef |\Z)", _src, _re.S)
import re as _re_mod
ns = {"_LOOPBACK_HOST_VALUES": None, "re": _re_mod}
exec(mhh.group(0), ns)  # noqa: S102 — executes the LIVE source verbatim
exec(mfn.group(0), ns)  # noqa: S102 — executes the LIVE source verbatim
ns["_is_accepted_host"].__globals__["_LOOPBACK_HOST_VALUES"] = LOOPBACK
_is_accepted_host = ns["_is_accepted_host"]


def trace(header, bound):
    c = Cup("ws_is_accepted_host")
    # B1: C-side NULL guard — argv strings are never NULL; LIVE's falsy-empty
    # guard is excluded from this walk (documented in gate.json notes).
    c.branch(False)
    h = header.strip()
    c.valstr(h)                       # V1
    if h.startswith("["):             # B2
        c.branch(True)
        close = h.find("]")
        if close != -1:               # B3
            c.branch(True)
            host_only = h[1:close]
        else:
            c.branch(False)
            host_only = h.strip("[]")
    else:
        c.branch(False)
        host_only = h.rsplit(":", 1)[0] if ":" in h else h
    host_only = host_only.lower()
    c.valstr(host_only)               # V2
    if bound in {"0.0.0.0", "::"}:    # B4
        c.branch(True)
        result = True
    else:
        c.branch(False)
        bound_lc = bound.lower()
        if bound_lc in LOOPBACK:      # B5
            c.branch(True)
            result = host_only in LOOPBACK   # B6
            c.branch(result)
        else:
            c.branch(False)
            result = host_only == bound_lc   # B7
            c.branch(result)
    # cross-check against LIVE implementation (must agree on value)
    live = _is_accepted_host(header, bound)
    assert live == result, "LIVE diverged from traced ladder: %r %r -> %r vs %r" % (
        header, bound, live, result)
    c.finalize()
    return c


def run(inputs):
    cups = []
    for raw in inputs:
        header, bound = raw.split("\t", 1)
        cups.append(trace(header, bound).finalize())
    return cups
