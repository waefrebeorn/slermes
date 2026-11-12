#!/usr/bin/env python3
"""Oracle for agent/compaction_events.py — the hermes.compaction v1 payload
builders.

The harness (t_port_compaction_events.c) emits one JSON line per case with a
"fn" discriminator; "out" is the serialized C result. The oracle execs the real
Python builders (AST-extracted, so the module's own imports are not needed) and
compares parsed JSON.
"""
import ast
import json
import sys
from types import SimpleNamespace

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

    # _payload defaults a missing attempt_id to uuid.uuid4().hex; the harness
    # always supplies one, so a deterministic stub keeps the oracle stable.
    class _FakeUuid4:
        hex = "0" * 32
    ns["uuid"] = SimpleNamespace(uuid4=lambda: _FakeUuid4())
    return ns


def main() -> int:
    ns = build_ns()
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        try:
            if fn == "closed":
                allowed = c["allowed"]
                dflt = c.get("dflt", "other")
                if dflt == "":
                    dflt = None          # harness emits "" for a NULL default
                want = ns["_closed"](c["in"], frozenset(allowed), dflt)
            elif fn == "int":
                want = ns["_int"](c["in"])
            elif fn == "failure_class":
                want = ns["failure_class"](c["in"])
            elif fn == "event_name":
                want = ns["event_name"](c["in"])
            elif fn == "payload":
                want = ns["_payload"](**c["in"])
            elif fn == "attempt_outcome":
                i = c["in"]
                want = ns["_attempt_outcome"](i["cs"], i["failure"], i["hr"])
            elif fn == "attempt_trigger":
                want = list(ns["_attempt_trigger"](c["in"]))
            elif fn == "attempt_payload":
                i = c["in"]
                want = ns["attempt_payload"](i.get("record", i),
                                             compression_count=i.get("cc"))
            elif fn == "micro_payload":
                i = c["in"]
                want = ns["micro_payload"](i.get("record", i))
            elif fn == "prune_payload":
                i = c["in"]
                want = ns["prune_payload"](
                    session_id=i["session_id"], tokens_before=i["tb"],
                    tokens_after=i["ta"], messages=i["m"],
                    tool_results_pruned=i["tr"])
            elif fn == "provider_native_payload":
                i = c["in"]
                want = ns["provider_native_payload"](
                    session_id=i["session_id"], compression_count=i.get("cc"))
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        # Was an attempt_id supplied? If not, both sides generate a random one.
        if fn == "payload":
            supplied = isinstance(c["in"], dict) and "attempt_id" in c["in"]
        elif fn in ("attempt_payload", "micro_payload"):
            rec = c["in"].get("record", c["in"]) if isinstance(c["in"], dict) else {}
            supplied = isinstance(rec, dict) and "attempt_id" in rec
        elif fn in ("prune_payload", "provider_native_payload"):
            supplied = False
        else:
            supplied = True
        if fn == "event_name":
            got = c["out"]                 # bare string, not JSON
        else:
            got = json.loads(c["out"]) if isinstance(c["out"], str) else c["out"]
        # A missing attempt_id defaults to a fresh uuid in Python and a fresh
        # random hex here, so the value itself is not comparable.
        if isinstance(got, dict) and isinstance(want, dict) and not supplied:
            got.pop("attempt_id", None)
            want.pop("attempt_id", None)
        if got != want:
            mism += 1
            keys = set(got) | set(want) if isinstance(got, dict) and isinstance(want, dict) else ()
            diff = [k for k in keys if got.get(k) != want.get(k)][:5]
            print(f"MISMATCH {c.get('case')} ({fn}): C={c['out'][:200]} "
                  f"PY={want}  diff_keys={diff}")
    if total == 0:
        print("ORACLE compaction_events: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE compaction_events: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
