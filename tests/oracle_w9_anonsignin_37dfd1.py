#!/usr/bin/env python3
"""Differential oracle for hermes_cli/anon_sign_in.py (w9_anonsignin_37dfd1)."""
import importlib.util
import json
import sys
import types

G = "w9_anonsignin_37dfd1"
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hc = types.ModuleType("hermes_cli")
    _hc.__path__ = [Q + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hc
_mod = types.ModuleType("hermes_cli.auth_constants")
_mod.httpx = types.SimpleNamespace()
sys.modules["hermes_cli.auth_constants"] = _mod

SRC = Q + "/hermes_cli/anon_sign_in.py"
spec = importlib.util.spec_from_file_location("anonsignin_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["anonsignin_live"] = live
spec.loader.exec_module(live)

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
    try:
        if func == "format_wait_line":
            py = live.format_wait_line(r.get("expires_in"))
        elif func == "completed_copy":
            st = live.Completed(email=r.get("email") or "", model=r.get("model") or "",
                                model_changed=bool(r.get("model_changed")),
                                handoff=tuple(r.get("handoff") or []))
            py = st.copy_terminal if r.get("terminal") else st.copy
        elif func == "code_copy_with_wait":
            st = live.Code(link="", code="", expires_in=r.get("expires_in"), interval=0)
            py = st.copy_with_wait
        elif func == "state_copy":
            kind = r.get("kind")
            st = {"code": live.Code(link="", code="", expires_in=0, interval=0),
                  "waiting": live.Waiting(), "cancelled": live.Cancelled(),
                  "timed_out": live.TimedOut(), "already_signed_in": live.AlreadySignedIn(),
                  "base": live.SignInState()}.get(kind)
            if st is None:
                continue
            py = st.copy
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
