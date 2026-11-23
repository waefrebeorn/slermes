#!/usr/bin/env python3
"""Differential oracle for tools/delegation_redaction.py (gate w14_delegation_58ac47).

The live module's redaction primitives (agent.redact: ~2000 lines of regex
patterns) are stubbed with two deterministic, documented transforms so the
structural redaction logic can be compared. C mirrors the same two stubs:
  force=True  -> redact_for_egress: "SECRET" -> "***"
  force=False -> redact_sensitive_text: "VAULT" -> "***"  (_redact_enabled False)
"""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_delegation_58ac47"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_ag = _mod("agent")
_ag.__path__ = []
_red = _mod("agent.redact",
    REDACTION_UNAVAILABLE="[redaction-unavailable]",
    _redact_enabled=lambda: False,
    redact_for_egress=lambda text: str(text or "").replace("SECRET", "***"),
    redact_sensitive_text=lambda text: str(text or "").replace("VAULT", "***"),
    is_secret_field_name=lambda key: str(key).lower() in
        {"api_key", "token", "secret", "password", "credential"})
_to = _mod("tools")
_to.__path__ = []
_mod("tools.delegation_output_schema",
     extract_json_candidate=lambda s: "",
     validate_output=lambda summary, schema: (True, []))

SRC = QUARRY + "/tools/delegation_redaction.py"
_spec = importlib.util.spec_from_file_location("delegation_redaction_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["delegation_redaction_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    force = bool(rec.get("force", True))
    if func == "text":
        return live.redact_child_text(rec.get("text"), force=force)
    if func == "value":
        return live.redact_child_value(rec.get("value"), force=force)
    if func == "result":
        return live.redact_child_result(rec.get("result") or {}, rec.get("output_schema"), force=force)
    if func == "event":
        return live.redact_completion_event(rec.get("event") or {}, force=force)
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("text", "value", "result", "event"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0

if __name__ == "__main__":
    sys.exit(main())
