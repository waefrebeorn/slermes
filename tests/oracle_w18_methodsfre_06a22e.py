#!/usr/bin/env python3
"""Differential oracle for gate w18_methodsfre_06a22e (tui_gateway/methods_free_tier.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = [Q + "/" + name.replace(".", "/")]
    sys.modules[name] = m


_pkg("tui_gateway")
_pkg("hermes_cli")
ctx = types.ModuleType("tui_gateway.method_ctx")


class HandlerRegistry:
    def __init__(self):
        self._pending = []

    def method(self, name):
        def dec(fn):
            self._pending.append((name, fn))
            return fn
        return dec

    def profile_scoped(self, fn):
        return fn


ctx.HandlerRegistry = HandlerRegistry
ctx.bind_module = lambda *a, **k: None
sys.modules["tui_gateway.method_ctx"] = ctx

STATE = {}
ah = types.ModuleType("hermes_cli.anon_auth")
ah.mark_guest_notice_shown = lambda: STATE.get("guest", False)
ah.has_guest = lambda: False
ah.free_inference_active = lambda: False
ah.GUEST_MODEL = "guest"
ah.FREE_TIER_LABEL = "Free"
sys.modules["hermes_cli.anon_auth"] = ah
fo = types.ModuleType("hermes_cli.free_tier_offer")
fo.TRIAL = "trial"
fo.CONNECTOR = "connector"
fo.claim_offer = lambda kind: bool(kind in ("trial", "connector"))
sys.modules["hermes_cli.free_tier_offer"] = fo
ac = types.ModuleType("hermes_cli.anon_challenge")
ac.record_host_outcome = lambda url, attempt, outcome: bool(
    str(url).startswith("https://") and outcome == "success" and int(attempt) >= 0)
sys.modules["hermes_cli.anon_challenge"] = ac

NAME = "tui_gateway.methods_free_tier_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/tui_gateway/methods_free_tier.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

live._ok = lambda rid, payload: payload
live._err = lambda rid, code, msg: {"code": code, "message": msg}
_handlers = {name: fn for name, fn in live._registry._pending}

G = "w18_methodsfre_06a22e"


def compute(r):
    func = r.get("func")
    rid = r.get("rid")
    params = r.get("in") or {}
    if func == "ack_notice":
        STATE.clear()
        STATE["guest"] = r.get("guest", False)
        return _handlers["free_tier.ack_notice"](rid, params)
    if func == "claim_nudge":
        return _handlers["free_tier.claim_nudge"](rid, params)
    if func == "challenge_result":
        return _handlers["free_tier.challenge_result"](rid, params)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
