#!/usr/bin/env python3
"""Differential oracle for tools/delegate_tool_dispatch.py (gate w16_delegateto_2d04d4)."""
import contextlib
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
G = "w16_delegateto_2d04d4"
sys.path.insert(0, QUARRY)


def pkg(name):
    m = types.ModuleType(name)
    m.__path__ = [QUARRY + "/" + name.replace(".", "/")]
    sys.modules[name] = m
    return m


def stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def load(name, rel):
    spec = importlib.util.spec_from_file_location(name, QUARRY + "/" + rel)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


pkg("tools")
pkg("hermes_cli")
pkg("hermes_cli.observability")
pkg("gateway")
noop = lambda *a, **k: None
stub("hermes_cli.observability.shared_metrics_loop",
     begin_delegation_run=noop, finish_delegation_unit=noop)
stub("tools.async_delegation",
     _new_delegation_id=lambda: "d", record_unit_child=noop,
     _current_origin_session_id=lambda: "",
     dispatch_async_delegation_batch=lambda **k: {}, push_task_failure_notice=noop)
stub("tools.delegate_tool_child_run",
     _attach_child=noop, _close_child=noop, _detach_child=noop,
     _fabricated_entry=lambda *a, **k: {}, _signal_child_stop=noop)
stub("tools.delegate_tool_config",
     _get_max_async_children=lambda: 0, _get_prefix_stagger_seconds=lambda: 0.0,
     _get_independent_completions=lambda: False)
stub("tools.delegate_tool_progress",
     SUBAGENT_FAILURE_STATUSES=frozenset({"failed", "error", "timeout"}),
     _print_completion_line=noop, _quiet=contextlib.nullcontext,
     describe_subagent_failure=lambda *a, **k: "", format_batch_tag=lambda *a, **k: "")
stub("tools.delegate_tool_registry", _capture_gateway_steer_authority=lambda s: (None, None))
stub("tools.delegate_tool_results", _finalize_child_results=noop)

FLAGS = {"env": {}, "finite": None, "async_supported": False, "async_raises": False}


def supported():
    if FLAGS["async_raises"]:
        raise RuntimeError("probe")
    return FLAGS["async_supported"]


stub("gateway.session_context",
     get_session_env=lambda k, d="": FLAGS["env"].get(k, d),
     async_delivery_supported=supported,
     session_history_delivery_supported=lambda: False)
stub("gateway.session_finite", finite_turn_required=lambda: FLAGS["finite"])

live = load("tools.delegate_tool_dispatch", "tools/delegate_tool_dispatch.py")


class _Child:
    def __init__(self, summary):
        self.summary = summary

    def get_activity_summary(self):
        if self.summary is None or (isinstance(self.summary, dict) and self.summary.get("raises")):
            raise RuntimeError("child probe failed")
        return self.summary


def live_out(rec):
    func = rec.get("func")
    if func == "resolve_wake":
        o = rec.get("opts") or {}
        FLAGS["env"] = {"HERMES_SINGLE_QUERY_SESSION": o.get("single_query", "")}
        FLAGS["finite"] = o.get("finite", None)
        FLAGS["async_supported"] = bool(o.get("async_supported", False))
        FLAGS["async_raises"] = bool(o.get("async_raises", False))
        return live._resolve_async_wake_sid(
            o.get("wake_sid", ""), bool(o.get("history_delivery", False)),
            oneshot_session=bool(o.get("oneshot_session", False)))
    if func == "batch_progress":
        children = [_Child(s) for s in (rec.get("child_agents") or [])]
        parts, in_tool = live._batch_progress_token(children)
        return {"parts": [list(p) if p is not None else None for p in parts], "in_tool": in_tool}
    if func == "sync_note":
        return live._SYNC_FALLBACK_NOTES[rec.get("reason")]
    return None


def main():
    cases = mism = 0
    allowed = ("resolve_wake", "batch_progress", "sync_note")
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in allowed:
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
