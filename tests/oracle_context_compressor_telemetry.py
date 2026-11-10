#!/usr/bin/env python3
"""Oracle for agent/context_compressor_telemetry.py _begin_compression_telemetry + _safe_int.

Harness emits JSON lines: {"tag":"sf_int","value":<token>,"out":{...}} and
{"tag":"begin","case_id":...,"current_tokens":...,"facts":{...},"seed":...,"out":"<C json str>"}.

This ast-extracts the LIVE method (module import chain is poisonous here) and
drives it on a stub self; the C side passes the same facts and re-emits the dict
as compact JSON (parsed here). Compares key SETS and per-key values.
"""
import ast
import json
import sys
import uuid

sys.stdin.reconfigure(encoding="utf-8")
SRC = "C:/Users/eman5/hermes-agent/agent/context_compressor_telemetry.py"


def _scalar_int(v):
    try:
        return int(v)
    except (TypeError, ValueError):
        return None


ns = {"uuid": uuid, "Any": object}
tree = ast.parse(open(SRC, encoding="utf-8").read())
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name == "_safe_int":
        exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
for n in tree.body:
    if isinstance(n, ast.ClassDef) and n.name == "CompressionTelemetryMixin":
        for m in n.body:
            if isinstance(m, ast.FunctionDef) and m.name == "_begin_compression_telemetry":
                exec(compile(ast.Module(body=[m], type_ignores=[]), "x", "exec"), ns)

_begin = ns["_begin_compression_telemetry"]


class _Fake:
    def __init__(self, **kw):
        self.__dict__.update(kw)


passes = 0
fails = []
total = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        case = json.loads(line)
    except json.JSONDecodeError:
        continue
    tag = case.get("tag")
    if tag == "sf_int":
        v = case.get("value")
        if v == "C-NULL":
            v = None
        out = _scalar_int(v)
        got = case.get("out") or {}
        expect = {"coerced": out is not None}
        if out is not None:
            expect["out"] = out
        total += 1
        if got == expect:
            passes += 1
        else:
            fails.append(("sf_int", v, expect, got))
    elif tag == "begin":
        facts = case.get("facts") or {}
        seed = case.get("seed")
        if seed == "C-NULL" or seed is None:
            seed = None
        cur = case.get("current_tokens")
        self_ = _Fake(
            provider=facts.get("provider", ""),
            model=facts.get("model", ""),
            context_length=facts.get("context_length"),
            threshold_tokens=facts.get("threshold_tokens"),
            _compression_telemetry_seed=seed,
        )
        try:
            td = _begin(
                self_,
                current_tokens=cur,
                attempt_id=case.get("attempt_id"),
                session_id=case.get("session_id"),
                trigger_source=case.get("trigger_source"),
            )
            py = td
        except Exception as e:  # noqa
            py = {"__raise__": type(e).__name__}
        # C side returns a JSON string; parse it to a dict.
        c_raw = case.get("out")
        try:
            c_obj = json.loads(c_raw) if isinstance(c_raw, str) else (c_raw or {})
        except json.JSONDecodeError:
            c_obj = {}
        total += 1
        if "__raise__" in py:
            ok = False
        else:
            py_keys = set(py.keys())
            c_keys = set(c_obj.keys()) if isinstance(c_obj, dict) else set()
            ok = (py_keys == c_keys) and all(py[k] == c_obj.get(k) for k in py_keys)
        if ok:
            passes += 1
        else:
            fails.append(("begin", case.get("case_id"), py, c_obj))

print("pass/total: %d/%d" % (passes, total))
for f in fails[:8]:
    print("FAIL %s case=%s" % (f[0], f[1]))
    print("  py=", json.dumps(f[2], sort_keys=True)[:400])
    print("  c =", json.dumps(f[3], sort_keys=True)[:400])
if fails:
    print("ORACLE context_compressor_telemetry: %d cases, %d mismatches" % (total, len(fails)))
    sys.exit(1)
print("ORACLE context_compressor_telemetry: %d cases, 0 mismatches" % total)
