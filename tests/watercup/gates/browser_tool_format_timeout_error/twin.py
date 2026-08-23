"""twin for browser_tool_format_timeout_error.

Imports LIVE tools/browser_tool.py::_format_browser_timeout_error and traces
the same rung order as the C harness (browser_tool_env.c::format_timeout_error).

Input format per cup: "<command><TAB><timeout><TAB><stdout><TAB><stderr>"
(one tab-separated tuple per input; fields must not contain tabs).

Env-dependent branches (_is_local_mode/_running_in_docker under the `open`
non-sandbox path) are excluded from this walk on both sides: inputs never
combine command=="open" with a sandbox-free combined output.
"""
import os, sys
import importlib
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

spec = ilu.spec_from_file_location(
    "wc_live_browser_tool", os.path.join(DEV, "tools", "browser_tool.py"))
bt = importlib.util.module_from_spec(spec)
sys.modules["wc_live_browser_tool"] = bt
spec.loader.exec_module(bt)

SANDBOX_HINT = (
    "Chromium sandbox launch failed. Set AGENT_BROWSER_ARGS="
    "'--no-sandbox,--disable-dev-shm-usage' in your environment, "
    "or run: npx agent-browser install --with-deps")


def trace(command, timeout, out, err):
    c = Cup("browser_format_timeout_error")
    parts = ["Command timed out after %d seconds" % timeout]
    c.valstr(parts[0])                       # V1: header
    detail = (err or out or "").strip()
    c.branch(bool(detail))                   # B1
    if detail:
        parts.append(detail[:1500])
        c.valstr("\n".join(parts))           # V2: with detail
    combined = ("%s\n%s" % (err, out)).lower()
    c.valstr(combined)                       # V3
    hints = []
    if "sandbox" in combined:                # B2
        c.branch(True)
        hints.append(SANDBOX_HINT)
        c.valstr("\n".join(parts + hints))   # V4: with hint
    else:
        c.branch(False)
        is_open = command == "open"
        local = bt._is_local_mode()
        c.branch(is_open and local)          # B3 (never true on this walk)
    c.valstr("\n".join(parts + hints))       # V4: final string
    # verify the traced ladder against the LIVE implementation
    live = bt._format_browser_timeout_error(command, timeout, out, err)
    p = ["Command timed out after %d seconds" % timeout]
    if detail:
        p.append(detail[:1500])
    if hints:
        p.extend(hints)
    assert live == "\n".join(p), \
        "LIVE diverged from traced ladder: %r vs %r" % (live, "\n".join(p))
    c.finalize()
    return c


def run(inputs):
    cups = []
    for raw in inputs:
        command, timeout_s, out, err = raw.split("\t", 3)
        cups.append(trace(command, int(timeout_s), out, err).finalize())
    return cups
