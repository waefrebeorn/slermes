#!/usr/bin/env python3
"""Differential oracle for hermes_cli/free_tier_offer.py (w13_freetiero_db4384)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, path=None, **attrs):
    m = types.ModuleType(name)
    if path is not None:
        m.__path__ = [path]
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg = _mod("hermes_cli", Q + "/hermes_cli")
anon = _mod("hermes_cli.anon_auth",
            free_inference_active=lambda state=None: False,
            current_nous_state=lambda: {},
            is_guest_state=lambda state: False,
            guest_allowed=lambda: False)

SRC = Q + "/hermes_cli/free_tier_offer.py"
spec = importlib.util.spec_from_file_location("w13_freetiero_db4384_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w13_freetiero_db4384_live"] = live
spec.loader.exec_module(live)
_ORIGINAL_ELIGIBLE = live._eligible


def compat(v):
    if isinstance(v, (set, frozenset)):
        return sorted((compat(x) for x in v), key=repr)
    if isinstance(v, (list, tuple)):
        return [compat(x) for x in v]
    if isinstance(v, dict):
        return {str(k): compat(x) for k, x in v.items()}
    return v


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
        if func == "constants":
            py = {
                "OFFER_DELAY_S": live.OFFER_DELAY_S,
                "REOFFER_AFTER_S": list(live.REOFFER_AFTER_S),
                "TRIAL": live.TRIAL, "CONNECTOR": live.CONNECTOR,
                "CONNECTED_APP_FLAG": live.CONNECTED_APP_FLAG,
            }
        elif func == "due_at":
            py = live._due_at(r.get("state") or {})
        elif func == "due_in":
            live._eligible = lambda kind: bool(r.get("eligible", False))
            live._clock = lambda: float(r.get("now", 0))
            try:
                py = live._due_in(r.get("kind", "trial"), r.get("state") or {})
            finally:
                live._eligible = _ORIGINAL_ELIGIBLE
        elif func == "eligible":
            live._eligible = _ORIGINAL_ELIGIBLE
            free = bool(r.get("free_active", False))
            guest = bool(r.get("is_guest", False))
            allowed = bool(r.get("guest_allowed", False))
            flag = r.get("flag")
            anon.free_inference_active = lambda state=None: free
            anon.is_guest_state = lambda state: guest
            anon.guest_allowed = lambda: allowed
            anon.current_nous_state = lambda: {live.CONNECTED_APP_FLAG: flag}
            py = live._eligible(r.get("kind", "trial"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = compat(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w13_freetiero_db4384: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
