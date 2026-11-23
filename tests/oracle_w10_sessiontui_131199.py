#!/usr/bin/env python3
"""Differential oracle for gate w10_sessiontui_131199 (gateway/session_tui_side.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = [os.path.join(Q, *name.split("."))]
        sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("gateway")
_mod("gateway.session_busy_controls", authorize=lambda *a, **k: None)
_pkg("persistence")
_pkg("persistence.core")


class AuthorityRefusal(Exception):
    def __init__(self, reason, field=None, detail=None):
        super().__init__(reason)
        self.reason, self.field, self.detail = reason, field, detail


_mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)

NAME = "w10_sessiontui_131199_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "gateway", "session_tui_side.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "abandoned":
        h = r.get("holder")
        if not h:
            holder = None
        else:
            first = h[0] if isinstance(h, list) else h
            if not isinstance(first, dict) or "abandoned" not in first or first.get("abandoned") is None:
                holder = [types.SimpleNamespace()]
            else:
                holder = [types.SimpleNamespace(abandoned=lambda: first.get("abandoned"))]
        return live._abandoned(holder)
    if f == "admit_valid":
        params = r.get("params") or {}
        sid = r.get("sid")
        required = r.get("required", "text")
        optional = tuple(r.get("optional") or ())
        ref = types.SimpleNamespace(session_id=sid)
        conn = types.SimpleNamespace(authority=None)
        try:
            live._admit(conn, ref, params, "bg", required=required, optional=optional)
            return True
        except Exception as exc:
            if type(exc).__name__ == "AuthorityRefusal" and getattr(exc, "reason", None) == "invalid_params":
                return False
            # A valid parameter set reaches `connection.authority` (None here) and
            # raises a non-AuthorityRefusal error; that means validation passed.
            return True
    return None


n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_sessiontui_131199: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
