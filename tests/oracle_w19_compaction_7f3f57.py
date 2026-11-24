#!/usr/bin/env python3
"""Differential oracle for persistence/workers/compaction.py (gate w19_compaction_7f3f57)."""
import ast
import dataclasses
import json
import sys
import typing
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "persistence" / "workers" / "compaction.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_compaction_7f3f57"

_WANT_CLASSES = {"WorkerCompaction", "WorkerCompressionPublication"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    import types
    mod = types.ModuleType("compaction_live")
    sys.modules["compaction_live"] = mod
    g = mod.__dict__
    g.update(dict(dataclass=dataclasses.dataclass, field=dataclasses.field,
                  Sequence=typing.Sequence, Mapping=typing.Mapping, Any=object,
                  Protocol=object, TYPE_CHECKING=False, JsonValue=object, SessionId=str,
                  WorkerMessage=dict, RowAnnotation=dict, WorkerAppendResult=dict,
                  WorkerExecutionId=str, AuthorityEpoch=object, ExecutionGeneration=object))
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_rows":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in _WANT_CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()

_COMPACTION_DEFAULTS = dict(model_config_patch=None, watermark=None, lock_holder=None,
                            tail_count=0, carried_messages=(), covered_ids=None,
                            unresolved_held=None, held_for_cas=None)
_PUBLICATION_DEFAULTS = dict(model=None, model_config=None, system_prompt=None, cwd=None,
                             profile_name=None, compression_lock_holder=None,
                             require_compression_lease=True, require_lease_refresh=False,
                             lease_ttl_seconds=300.0, watermark=None, watermark_ceiling=None)


def run_case(r):
    f = r.get("func")
    if f == "rows":
        return G["_rows"](r.get("rows"))
    if f == "compaction_payload":
        kw = dict(_COMPACTION_DEFAULTS)
        kw["messages"] = r.get("messages") or []
        for k in _COMPACTION_DEFAULTS:
            if k in r:
                kw[k] = r[k]
        return G["WorkerCompaction"](**kw).payload()
    if f == "publication_payload":
        kw = dict(_PUBLICATION_DEFAULTS)
        kw["child_session_id"] = r.get("child_session_id") or ""
        kw["source"] = r.get("source") or ""
        kw["messages"] = r.get("messages") or []
        for k in _PUBLICATION_DEFAULTS:
            if k in r:
                kw[k] = r[k]
        return G["WorkerCompressionPublication"](**kw).payload()
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in {"rows", "compaction_payload", "publication_payload"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if isinstance(py, tuple):
            py = list(py)
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
