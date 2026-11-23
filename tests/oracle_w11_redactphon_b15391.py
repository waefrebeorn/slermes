#!/usr/bin/env python3
"""Differential oracle for agent/redact_phone_policy.py (gate w11_redactphon_b15391).

The config read and the two downstream redactors are stubbed and injected per
case.  The stub redactors echo the keep flag they were called with, so the gate
verifies the policy module's own decision and plumbing (fails closed, bool-only
opt-in) rather than re-testing agent.redact.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

ERROR = {"v": False}
SECURITY = {"v": None}


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


hc = _pkg("hermes_cli")
backend = types.ModuleType("hermes_cli.config_backend")
class ConfigBackendUnavailable(Exception):
    pass
backend.ConfigBackendUnavailable = ConfigBackendUnavailable
hc.config_backend = backend
sys.modules["hermes_cli.config_backend"] = backend

config_mod = types.ModuleType("hermes_cli.config")
def _load_config_readonly():
    if ERROR["v"]:
        raise ConfigBackendUnavailable()
    return {"security": SECURITY["v"]}
config_mod.load_config_readonly = _load_config_readonly
hc.config = config_mod
sys.modules["hermes_cli.config"] = config_mod

agent_pkg = _pkg("agent")
redact_mod = types.ModuleType("agent.redact")
def _mirror(text, *, keep_phone_numbers=False):
    return {"keep": keep_phone_numbers, "text": text}
redact_mod.redact_for_egress = _mirror
redact_mod.redact_sensitive_text = _mirror
agent_pkg.redact = redact_mod
sys.modules["agent.redact"] = redact_mod

SRC = QUARRY + "/agent/redact_phone_policy.py"
spec = importlib.util.spec_from_file_location("w11_redactphon_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_redactphon_live"] = live
spec.loader.exec_module(live)


def compute(r):
    ERROR["v"] = bool(r.get("config_error", False))
    SECURITY["v"] = r.get("security") if "security" in r else None
    f = r.get("func")
    if f == "chat_keeps_phone_numbers":
        return live.chat_keeps_phone_numbers()
    if f == "redact_for_chat_egress":
        return live.redact_for_chat_egress(r.get("text", ""))
    if f == "redact_assistant_reply":
        return live.redact_assistant_reply(r.get("text", ""))
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
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w11_redactphon_b15391: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_redactphon_b15391: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
