#!/usr/bin/env python3
"""Differential oracle for gate w10_sessionrec_51b4df (gateway/session_recovery.py)."""
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


# Top-level import: from gateway.config import Platform.
_pkg("gateway")
_mod("gateway.config", Platform=types.SimpleNamespace(SLACK="slack"))

# session_keys.profile_from_session_key is imported inside a method; register the
# live implementation (copied verbatim from quarry/session_keys.py) so the
# parent-package chain does not drag in persistence during this gate.
def _ns(namespace):
    if namespace == "main":
        return "default"
    return "main" if namespace == "main~" else namespace


def _profile_from_session_key(session_key):
    if not session_key:
        return None
    parts = str(session_key).split(":")
    if len(parts) < 2 or parts[0] != "agent":
        return None
    return _ns(parts[1] or "main")


_mod("session_keys", profile_from_session_key=_profile_from_session_key,
     profile_from_session_key_namespace=_ns)

NAME = "w10_sessionrec_51b4df_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "gateway", "session_recovery.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "profile_from_key":
        return live.SessionRecoveryMixin._profile_from_session_key(r.get("session_key"))
    if f == "recovered_allowed":
        obj = live.SessionRecoveryMixin()
        obj.config = types.SimpleNamespace(multiplex_profiles=bool(r.get("multiplex_profiles", False)))
        active = r.get("active_profile") or "default"
        orig = live.SessionRecoveryMixin._active_profile_name
        live.SessionRecoveryMixin._active_profile_name = lambda self: active
        try:
            return obj._recovered_row_allowed_for_active_profile(
                requested_session_key=r.get("requested_session_key") or "",
                recovered=r.get("recovered") or {})
        finally:
            live.SessionRecoveryMixin._active_profile_name = orig
    if f == "matches_scope":
        s = r.get("source") or {}
        src = types.SimpleNamespace(platform=live.Platform.SLACK,
                                    chat_type=s.get("chat_type"), scope_id=s.get("scope_id"))
        return live.SessionRecoveryMixin._recovered_row_matches_source_scope(
            r.get("recovered") or {}, src)
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
print("ORACLE w10_sessionrec_51b4df: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
