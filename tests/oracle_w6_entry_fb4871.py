#!/usr/bin/env python3
"""Differential oracle for tui_gateway/entry.py (gate w6_entry_fb4871).

Loads the LIVE module by path with its heavy process/bootstrap imports stubbed
via sys.modules registration and a parent-package bootstrap. The env-driven
grace is driven by injecting os.environ inside the oracle process (the C side
takes the value from the fixture), avoiding any Windows setenv quirk.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Parent-package bootstrap + load-time leaf stubs. The real tui_gateway._env
# is imported from the stub package's __path__.
_mod("hermes_bootstrap", harden_import_path=lambda: None)
pkg = _mod("tui_gateway")
pkg.__path__ = [Q + "/tui_gateway"]
_mod("tui_gateway._stdin_recovery", handle_spurious_eof=lambda *a, **k: False)
_mod("tui_gateway.event_replay", replay_epoch=lambda: 0)
_mod("tui_gateway.transport", TeeTransport=type("TeeTransport", (object,), {}))
_mod("tui_gateway.server",
     _CRASH_LOG="", _err=lambda *a, **k: None, dispatch=lambda *a, **k: None,
     resolve_skin=lambda: "dark", write_json=lambda *a, **k: True,
     _launch_home=lambda: "", _stdio_transport=None, _stdio_is_rpc_channel=False,
     install_tui_message_injector=lambda: None, _ensure_skin_watcher=lambda: None,
     _start_backend_heartbeat_refresher=lambda: None,
     _schedule_startup_orphan_sweep=lambda: None,
     _start_cache_expiry_compactor=lambda: None, _shutdown_sessions=lambda: None)

SRC = Q + "/tui_gateway/entry.py"
spec = importlib.util.spec_from_file_location("w6_entry_fb4871_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_entry_fb4871_live"] = live
spec.loader.exec_module(live)

ENVNAME = "HERMES_TUI_GATEWAY_SHUTDOWN_GRACE_S"


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", round(float(x), 9))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def compute(r):
    f = r.get("func")
    if f == "grace_default":
        return live._DEFAULT_SHUTDOWN_GRACE_S
    if f == "grace":
        v = r.get("env_value")
        if v is None:
            os.environ.pop(ENVNAME, None)
        else:
            os.environ[ENVNAME] = v
        return live._shutdown_grace_seconds()
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        want = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s case=%s: %r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if _canon(want) != _canon(got):
        mism += 1
        print("MISMATCH %s case=%s C=%r PY=%r" % (r.get("func"), r.get("case"), got, want),
              file=sys.stderr)
print("ORACLE w6_entry_fb4871: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
