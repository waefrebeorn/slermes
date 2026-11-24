#!/usr/bin/env python3
"""Differential oracle for gateway/run_adapters_recovery.py (w17_w17_runadapter_1b5147_1b5147)."""
import importlib.util
import json
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


gateway = _mod("gateway"); gateway.__path__ = []
class Platform:
    LOCAL = "local"; RELAY = "relay"
    def __init__(self, value): self.value = value
_mod("gateway.config", Platform=Platform)
gp = _mod("gateway.platforms"); gp.__path__ = []
_mod("gateway.platforms.base", BasePlatformAdapter=object)
_mod("gateway.platforms.helpers", carry_inbound_dedup=lambda *a, **k: None,
     hand_over_held_inbound=lambda *a, **k: None)
_mod("gateway.restart", is_global_startup_conflict=lambda *a, **k: False)
_mod("gateway.run_shutdown", _log_suppressed=lambda *a, **k: None)

SRC = Q + "/gateway/run_adapters_recovery.py"
spec = importlib.util.spec_from_file_location("w17_runadapter_1b5147_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_runadapter_1b5147_live"] = live
spec.loader.exec_module(live)

# Pin the constants the C port mirrors; drift must be loud, not silent.
assert tuple(live._TOKEN_CONTENTION_RECONNECT_DELAYS) == (120.0, 300.0, 600.0)
assert live._TOKEN_CONTENTION_EPISODE_SECS == 1800.0
assert live._RECONNECT_SUSPEND_GAP_SECS == 60.0

_state = {"now": 0.0}
live.time = types.SimpleNamespace(monotonic=lambda: _state["now"])
Mixin = live.GatewaySecondaryReconnectMixin

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
        if func == "message":
            platform = types.SimpleNamespace(value=r.get("platform_value", ""))
            py = live._adapter_unavailable_message(platform, retrying=r.get("retrying", True))
        elif func == "contention":
            py = live.is_token_contention(r.get("error_code"))
        elif func == "discount":
            _state["now"] = r.get("now", 0.0)
            entries = [dict(e) for e in r.get("entries", [])]
            live.discount_suspended_wait(entries, r.get("waited_from", 0.0), r.get("planned", 0.0))
            py = [e.get("queued_at") for e in entries]
        elif func == "delay":
            _state["now"] = r.get("now", 0.0)
            self_ = types.SimpleNamespace()
            if r.get("has_strikes", True):
                self_._token_contention_strikes = {
                    str(k): list(v) for k, v in (r.get("strikes") or {}).items()}
            self_._update_platform_runtime_status = lambda *a, **k: None
            fatal = {}
            ad = r.get("adapter", {})
            adapter = types.SimpleNamespace(
                fatal_error_retryable=ad.get("fatal_error_retryable", False),
                fatal_error_code=ad.get("fatal_error_code"),
                fatal_error_message=ad.get("fatal_error_message", ""),
                platform=types.SimpleNamespace(value=ad.get("platform_value", "")),
                _set_fatal_error=lambda code, msg, retryable: fatal.update(code=code, msg=msg, retryable=retryable))
            delay = Mixin._token_contention_reconnect_delay(self_, r.get("status_key", ""), adapter)
            strikes = getattr(self_, "_token_contention_strikes", {})
            py = {"delay": float(delay),
                  "strikes": {str(k): [v[0], v[1]] for k, v in strikes.items()},
                  "fatal": bool(fatal),
                  "fatal_message": fatal.get("msg") if fatal else None}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w17_runadapter_1b5147: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
