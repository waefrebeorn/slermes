#!/usr/bin/env python3
"""Differential oracle for agent/turn_tool_validation.py (gate w13_turntoolv_de0ba5).

Drives the LIVE _preview_name and validate_tool_calls verdict decisions with
repair/deferred routing, message mutation and hook side effects stubbed off.
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

Q = "C:/Users/eman5/hermes-agent"
G = "w13_turntoolv_de0ba5"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _seed(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + pn.replace(".", "/")]
            sys.modules[pn] = pm
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parent = ".".join(parts[:-1])
    if parent:
        setattr(sys.modules[parent], parts[-1], mod)
    return mod


_seed("agent")
_seed("agent.message_metadata", append_message=lambda *a, **k: None)
_seed("agent.message_sanitization",
      close_interrupted_tool_sequence=lambda *a, **k: None,
      normalize_provider_tool_call_ids=lambda *a, **k: None)
_seed("session_rules")
_seed("session_rules.turns", coalesce_tool_call_id=lambda tc: "")
_seed("agent.turn_failure_copy", site_copy=lambda k: k, stamp_failure=lambda d, *a: d)
_seed("hermes_constants", FINISH_REASON_LENGTH="length")
_seed("agent.conversation_loop", _invalid_tool_name_error_content=lambda *a, **k: "")
_seed("hermes_cli")
_seed("hermes_cli.observability")
_seed("hermes_cli.observability.shared_metrics_model", record_tool_call_quality=lambda *a, **k: None)

_spec = importlib.util.spec_from_file_location(G + "_live", Q + "/agent/turn_tool_validation.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)
live._route_deferred_call = lambda agent, tc: False
live._partial_exit = lambda agent, messages, ch, acc, final: {"final_response": final, "partial": True}

_FUNCS = ("preview_name", "validate_facts")


def live_out(rec):
    func = rec.get("func")
    if func == "preview_name":
        return live._preview_name(rec.get("name") or "")
    if func == "validate_facts":
        calls = []
        for tc in (rec.get("tool_calls") or []):
            fn = tc.get("function") or {}
            calls.append(types.SimpleNamespace(
                function=types.SimpleNamespace(name=fn.get("name") or "", arguments=fn.get("arguments"))))
        am = types.SimpleNamespace(tool_calls=calls, provider_data=None)
        agent = types.SimpleNamespace(
            valid_tool_names=list(rec.get("valid_names") or []),
            _invalid_tool_retries=int(rec.get("invalid_tool_retries") or 0),
            _invalid_json_retries=int(rec.get("invalid_json_retries") or 0),
            _uniquify_tool_call_ids=lambda *a, **k: None,
            _repair_tool_call=lambda name: None,
            _vprint=lambda *a, **k: None,
            _buffer_vprint=lambda *a, **k: None,
            _flush_status_buffer=lambda *a, **k: None,
            _build_assistant_message=lambda *a, **k: {},
            _cleanup_task_resources=lambda *a, **k: None,
            _persist_session=lambda *a, **k: None,
            log_prefix="test")
        v = live.validate_tool_calls(
            agent, am, rec.get("finish_reason") or "", messages=[],
            conversation_history=None, api_call_count=1, effective_task_id="t")
        return {"action": v.action, "mixed": v.mixed_invalid_batch, "result": v.result}
    return None


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
        if rec.get("func") not in _FUNCS:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%s func=%s C=%r PY=%r" %
                  (G, rec.get("case"), rec.get("func"), got, py), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
    return 1 if (mism or cases == 0) else 0


if __name__ == "__main__":
    sys.exit(main())
