#!/usr/bin/env python3
"""
sta_oracle_hermes_cli_shared_metrics_contract.py — Python oracle for
shared_metrics_contract.py pure functions.

Reads op-code fixtures from a file (argv[1], one per line, op|arg0|arg1|...),
calls the REAL Python function, emits JSON results to stdout.
"""
import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from hermes_cli.observability.shared_metrics_contract import (
    counter_dimensions_are_valid,
    execution_surface,
    task_start_fields,
    task_entrypoint,
    task_terminal_fields,
    task_terminal_state,
    duration_bucket,
    count_bucket,
    provider_family,
    model_family,
    model_call_outcome,
    model_call_fields,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def emit_result(op, inp, out):
    emit({"op": op, "in": inp, "out": out})


def main():
    fixture_path = sys.argv[1] if len(sys.argv) > 1 else "/dev/stdin"
    with open(fixture_path) as f:
        for raw in f:
            line = raw.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.split("|")
            op = parts[0]
            args = parts[1:]

            try:
                if op == "duration_bucket":
                    ms = int(args[0]) if args and args[0] else 0
                    result = duration_bucket(ms)
                    emit_result(op, ms, result)

                elif op == "count_bucket":
                    c = int(args[0]) if args and args[0] else 0
                    result = count_bucket(c)
                    emit_result(op, c, result)

                elif op == "execution_surface":
                    val = args[0] if args and args[0] else ""
                    result = execution_surface({"execution_surface": val, "platform": val})
                    emit_result(op, val, result)

                elif op == "task_entrypoint":
                    ep = args[0] if len(args) > 0 and args[0] else ""
                    surface = args[1] if len(args) > 1 and args[1] else ""
                    has_parent_task = (len(args) > 2 and args[2] == "true")
                    has_parent_session = (len(args) > 3 and args[3] == "true")
                    kwargs = {"entrypoint": ep, "execution_surface": surface}
                    if has_parent_task:
                        kwargs["parent_task_id"] = "some_task"
                    if has_parent_session:
                        kwargs["parent_session_id"] = "some_session"
                    result = task_entrypoint(kwargs, surface)
                    emit_result(op, {"ep": ep, "surface": surface, "parent_task": has_parent_task, "parent_session": has_parent_session}, result)

                elif op == "task_start_fields":
                    ep = args[0] if len(args) > 0 and args[0] else ""
                    surface = args[1] if len(args) > 1 and args[1] else ""
                    kwargs = {"entrypoint": ep, "execution_surface": surface}
                    result = task_start_fields(kwargs)
                    emit_result(op, {"entrypoint": ep, "platform": surface}, result)

                elif op == "task_terminal_fields":
                    ep = args[0] if len(args) > 0 and args[0] else ""
                    surface = args[1] if len(args) > 1 and args[1] else ""
                    dur_ms = int(args[2]) if len(args) > 2 and args[2] else 0
                    mc = int(args[3]) if len(args) > 3 and args[3] else 0
                    tc = int(args[4]) if len(args) > 4 and args[4] else 0
                    rc = int(args[5]) if len(args) > 5 and args[5] else 0
                    kwargs = {"entrypoint": ep, "execution_surface": surface}
                    result = task_terminal_fields(kwargs, duration_ms=dur_ms, model_call_count=mc, tool_call_count=tc, retry_count=rc)
                    emit_result(op, {"ep": ep, "surface": surface, "dur_ms": dur_ms, "mc": mc, "tc": tc, "rc": rc}, result)

                elif op == "task_terminal_state":
                    reason = args[0] if len(args) > 0 and args[0] else ""
                    interrupted = len(args) > 1 and args[1] == "true"
                    completed = len(args) > 2 and args[2] == "true"
                    failed = len(args) > 3 and args[3] == "true"
                    kwargs = {"turn_exit_reason": reason, "interrupted": interrupted, "completed": completed, "failed": failed}
                    out_str, end_reason, termination = task_terminal_state(kwargs)
                    emit_result(op, {"reason": reason, "interrupted": interrupted, "completed": completed, "failed": failed}, [out_str, end_reason, termination])

                elif op == "model_family":
                    declared = args[0] if len(args) > 0 and args[0] else ""
                    model = args[1] if len(args) > 1 and args[1] else ""
                    resp = args[2] if len(args) > 2 and args[2] else ""
                    kwargs = {"model_family": declared, "model": model, "response_model": resp}
                    result = model_family(kwargs)
                    emit_result(op, {"declared": declared, "model": model, "response_model": resp}, result)

                elif op == "model_call_outcome":
                    outcome = args[0] if args and args[0] else ""
                    result = model_call_outcome({"outcome": outcome})
                    emit_result(op, outcome, result)

                elif op == "provider_family":
                    provider = args[0] if args and args[0] else ""
                    result = provider_family({"provider": provider})
                    emit_result(op, provider, result)

                elif op == "counter_dims_valid":
                    metric = args[0]
                    dims = {}
                    for a in args[1:]:
                        if "=" in a:
                            k, v = a.split("=", 1)
                            dims[k] = v
                    result = counter_dimensions_are_valid(metric, dims)
                    emit_result(op, {"metric": metric, "dims": dims}, result)

                else:
                    emit({"error": "unknown op: " + op})

            except Exception as e:
                emit({"op": op, "error": str(e)})


if __name__ == "__main__":
    main()