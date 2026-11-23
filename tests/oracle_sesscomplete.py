#!/usr/bin/env python3
"""Differential oracle for gateway/session_complete_path.py (gate sesscomplete).

Loads the LIVE `complete_path` via importlib and drives the real async function
with stubbed runtime collaborators, all scripted from the fixture:
  - gateway.session_busy_controls.authorize  -> injected raise (or success)
  - gateway.session_policy.*                 -> policy presence + host-cwd verdict
  - tui_gateway.server._path_completion_items -> scripted listing
  - gateway.run._profile_runtime_scope       -> no-op context manager
The C harness's `out` (a dict or a {"__raise__":...} refusal) is compared with
the live result. AuthorityRefusal is the real quarry exception class.

Input (harness output, one JSON per line): a `complete` case with `out`.
Summary: ORACLE sesscomplete: N cases, M mismatches.
"""
import asyncio
import contextlib
import importlib.util
import json
import sys
import types
from types import SimpleNamespace as NS

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


try:
    import gateway  # noqa: F401
except Exception:
    _seed("gateway")

SRC = QUARRY + "/gateway/session_complete_path.py"
_spec = importlib.util.spec_from_file_location("sesscomplete_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["sesscomplete_live"] = live
_spec.loader.exec_module(live)

AuthorityRefusal = __import__("persistence.core.errors", fromlist=["AuthorityRefusal"]).AuthorityRefusal


def live_result(rec):
    params = rec.get("params")
    sid = rec.get("session_id") or ""
    sessions = rec.get("sessions") or []
    policy_null = bool(rec.get("policy_null"))
    host_cwd = bool(rec.get("host_cwd"))
    items = rec.get("items") or []
    auth_raise = rec.get("authorize_raise") or None

    def author(*a, **k):
        if auth_raise:
            raise AuthorityRefusal(auth_raise)

    live.authorize = author  # module-level `from ... import authorize` binding
    _seed("gateway.session_busy_controls", authorize=author)
    _seed("gateway.session_policy",
          policy_for_source=(lambda r, s: None) if policy_null
          else (lambda r, s: NS(terminal_json="{}", cwd="/x")),
          terminal_uses_host_cwd=lambda d: host_cwd)
    _seed("tui_gateway", server=NS(_path_completion_items=lambda *a: items))
    _seed("gateway.run", _profile_runtime_scope=lambda p: contextlib.nullcontext())

    conn = NS(authority=NS(sessions={s: NS(source="local") for s in sessions},
                           runner="r", profile_id="p"))
    ref = NS(session_id=sid)
    try:
        return asyncio.run(live.complete_path(conn, ref, params))
    except AuthorityRefusal as exc:
        return {"__raise__": "AuthorityRefusal", "msg": str(exc)}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") != "complete":
            continue
        cases += 1
        py = live_result(rec)
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH sesscomplete case={rec.get('case')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print("ORACLE sesscomplete: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE sesscomplete: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
