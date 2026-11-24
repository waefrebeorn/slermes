#!/usr/bin/env python3
"""Differential oracle for gateway/session_mutation_model.py (gate w18_sessionmut_44a137).

Top-level imports (gateway.session_route_identity, persistence.core.errors)
and the lazy hermes_cli.model_switch constants are stubbed. A reset-with-flags
AuthorityRefusal is surfaced as the marker object the C port returns.
"""
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


class AuthorityRefusal(Exception):
    def __init__(self, reason, field=None, detail=None, **kw):
        self.reason = reason
        self.field = field
        self.detail = detail
        super().__init__(reason)


gateway = _mod("gateway")
gateway.__path__ = []
_mod("gateway.session_route_identity",
     ROUTE_BOUND_KEYS=("provider", "base_url", "api_mode", "context_length",
                       "api_key", "api", "api_key_from", "key_env", "api_key_env"))
persistence = _mod("persistence")
persistence.__path__ = []
core = _mod("persistence.core")
core.__path__ = []
_mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
cli = _mod("hermes_cli")
cli.__path__ = []
_mod("hermes_cli.model_switch",
     MODEL_SWITCH_ERR_RESET_WITH_FLAGS="reset_with_flags",
     MODEL_SWITCH_ERROR_TEXT={
         "reset_with_flags": "/model reset takes no flags — it clears this "
                             "session's model override so the configured "
                             "default applies again."},
     MODEL_SWITCH_RESET_TOKEN="reset")

SRC = Q + "/gateway/session_mutation_model.py"
spec = importlib.util.spec_from_file_location("w18_sessionmut_44a137_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w18_sessionmut_44a137_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "selected_provider_key_env":
        return live._selected_provider_key_env(r.get("config", {}), r.get("provider", ""))
    if func == "is_model_reset":
        try:
            return live.is_model_reset(r.get("payload", {}))
        except AuthorityRefusal as exc:
            return {"refused": exc.reason, "field": exc.field, "detail": exc.detail}
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_sessionmut_44a137: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
