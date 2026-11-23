#!/usr/bin/env python3
"""Differential oracle for agent/compaction_events.py (gate w9_compactione_f03b0c).

Executes the LIVE pure payload builders via AST extraction (so the module's
heavy publish/relay imports are not needed) with the real closed-set constants,
and compares each harness "out" to the live result.
"""
import ast
import json
import sys
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/compaction_events.py"
HERMES = "C:/Users/eman5/hermes-agent"

KEEP_FUNCS = {
    "_closed", "_int", "failure_class", "event_name", "_payload",
    "_attempt_outcome", "_attempt_trigger", "attempt_payload", "micro_payload",
    "prune_payload", "provider_native_payload",
}
KEEP_CONSTS = {
    "_KINDS", "_SCOPES", "_METHODS", "_TRIGGERS", "_TRIGGER_CLASSES",
    "_TRIGGER_CLASS_VALUES", "_OVERFLOW_REASONS", "_OUTCOMES", "_SPLIT_STATUSES",
    "_TOKEN_COUNT_METHODS", "_EXTRA_FAILURE_CLASSES", "_ABORTED_CLASSES",
    "_COMMIT_MODES", "_MICRO_OUTCOMES", "_FIELDS", "_ATTEMPT_INTS",
    "COMPACTION_MARK", "ATTEMPT_MARK",
}


def build_ns():
    sys.path.insert(0, HERMES)
    ns = {}
    try:
        from hermes_cli.observability.shared_metrics_contract import (
            COMPRESSION_FAILURE_CLASSES, COMPRESSION_SKIP_CLASSES)
        ns["COMPRESSION_FAILURE_CLASSES"] = COMPRESSION_FAILURE_CLASSES
        ns["COMPRESSION_SKIP_CLASSES"] = COMPRESSION_SKIP_CLASSES
    except Exception:
        ns["COMPRESSION_SKIP_CLASSES"] = frozenset({
            "attempt_superseded", "empty_post_handoff_window", "explicit_interrupt",
            "insufficient_messages", "lock_contended", "no_compressible_window",
            "snapshot_stale"})
        ns["COMPRESSION_FAILURE_CLASSES"] = ns["COMPRESSION_SKIP_CLASSES"] | frozenset({
            "aux_model_fallback", "commit_fence_cancelled", "empty_transcript",
            "exception", "feasibility_skip", "no_progress", "none", "other",
            "pool_saturated", "rollback", "session_split_failed",
            "stall_deterministic_fallback", "stall_interrupted",
            "summary_auth_failure", "summary_empty_content_failure",
            "summary_generation_aborted", "summary_generation_failed",
            "summary_network_failure", "summary_overload_degraded",
            "summary_overload_failure", "summary_truncated_failure", "unknown",
            "would_grow"})
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    nodes = []
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in KEEP_FUNCS:
            nodes.append(n)
        elif isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) \
                and n.targets[0].id in KEEP_CONSTS:
            nodes.append(n)
    exec(compile(ast.fix_missing_locations(
        ast.Module(body=nodes, type_ignores=[])), SRC, "exec"), ns)
    ns["_failure_classes"] = lambda: (
        ns["COMPRESSION_FAILURE_CLASSES"] | ns["_EXTRA_FAILURE_CLASSES"])
    ns["_skip_classes"] = lambda: ns["COMPRESSION_SKIP_CLASSES"]

    class _FakeUuid4:
        hex = "0" * 32
    ns["uuid"] = SimpleNamespace(uuid4=lambda: _FakeUuid4())
    return ns


NS = build_ns()


def compute(r):
    func = r.get("func")
    if func == "closed":
        dflt = r.get("dflt")
        if dflt == "":
            dflt = None
        return NS["_closed"](r.get("value"), frozenset(r.get("allowed") or []), dflt)
    if func == "int":
        return NS["_int"](r.get("value"))
    if func == "failure_class":
        return NS["failure_class"](r.get("value"))
    if func == "event_name":
        return NS["event_name"](r.get("payload"))
    if func == "attempt_outcome":
        i = r
        return NS["_attempt_outcome"](i.get("cs"), i.get("failure"), i.get("hr"))
    if func == "attempt_trigger":
        return list(NS["_attempt_trigger"](r.get("trigger_source")))
    if func == "payload":
        return NS["_payload"](**r.get("values", {}))
    if func == "attempt_payload":
        return NS["attempt_payload"](r.get("record"), compression_count=r.get("cc"))
    if func == "micro_payload":
        return NS["micro_payload"](r.get("record"))
    if func == "prune_payload":
        return NS["prune_payload"](
            session_id=r.get("session_id", ""), tokens_before=r.get("tb"),
            tokens_after=r.get("ta"), messages=r.get("m"),
            tool_results_pruned=r.get("tr"))
    if func == "provider_native_payload":
        return NS["provider_native_payload"](
            session_id=r.get("session_id", ""), compression_count=r.get("cc"))
    return None


def supplied(r):
    func = r.get("func")
    if func == "payload":
        return "attempt_id" in r.get("values", {})
    if func == "attempt_payload":
        rec = r.get("record")
        return isinstance(rec, dict) and "attempt_id" in rec
    if func in ("micro_payload", "prune_payload", "provider_native_payload"):
        return False
    return True


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
    if isinstance(got, dict) and isinstance(py, dict) and not supplied(r):
        got.pop("attempt_id", None)
        py.pop("attempt_id", None)
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w9_compactione_f03b0c: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w9_compactione_f03b0c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
