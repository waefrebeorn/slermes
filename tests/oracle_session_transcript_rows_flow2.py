#!/usr/bin/env python3
"""Oracle for the FLOW port of gateway/session_transcript_rows.py (7 defs).

AST-extracts the live module constants and all 7 defs from the quarry file.
Every collaborator (``compaction_summary``, ``store.messages.
transcript_handoff_rows``, ``row.as_wire_row``, the passed ``earlier`` and
``project`` callables) becomes a recording stub returning the SAME scripted
results the C harness feeds its universal seam. Compares the ordered seam
trace + the canonical return value, parsed from JSON on both sides.

Input: stdin JSON lines from t_port_session_transcript_rows_flow2.
Summary: ORACLE session_transcript_rows_flow2: N cases, M mismatches.
Fail-closed on 0 cases.
"""
import ast
import asyncio
import builtins
import contextlib
import json
import os
import sys
import types
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "session_transcript_rows.py")

WANTED = {
    "handoff_row", "_mark_index", "handoff_occurrence", "shown_occurrences",
    "handoff_rows", "page_handoffs", "project_entries",
}

CUR = {"trace": [], "io": {}, "pos": {}, "ns": None, "last_raise": None}
_NOEI = object()


def rec(ev):
    CUR["trace"].append(ev)


# ── script access (mirrors the harness stub semantics) ──────────────────────
def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    cls = getattr(builtins, kind, None)
    if cls is None and kind == "CancelledError":
        cls = asyncio.CancelledError
    if isinstance(cls, type) and issubclass(cls, BaseException):
        # OSError aliases (IOError/EnvironmentError/WindowsError) report their
        # __name__ as OSError; build a same-named subclass to keep the scripted
        # kind observable in the descriptor (the C side keeps the script kind).
        if cls.__name__ != kind and kind not in ("ExceptionGroup",
                                                 "BaseExceptionGroup"):
            cls = type(kind, (cls,), {})
        if kind in ("ExceptionGroup", "BaseExceptionGroup"):
            sub = ValueError("x") if kind == "ExceptionGroup" else SystemExit("x")
            return cls(msg, [sub])
        return cls(msg)
    return type(kind, (Exception,), {})(msg)


def next_script(name):
    io = CUR["io"]
    if name not in io:
        return None
    v = io[name]
    if isinstance(v, list):
        if not v:
            return None
        i = CUR["pos"].get(name, 0)
        if i < len(v):
            CUR["pos"][name] = i + 1
        return v[min(i, len(v) - 1)]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


def seam_rec(name, args):
    CUR["last_raise"] = None
    r = next_script(name)
    rec({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        CUR["last_raise"] = r
        raise make_exc(r)
    return r


def exc_desc(e):
    sp = CUR.get("last_raise")
    if sp is not None:
        return {"__raise__": sp.get("__raise__", "Exception"),
                "message": sp.get("message", "")}
    return {"__raise__": type(e).__name__, "message": str(e)}


# ── canonicalization: parsed-JSON comparison, never raw strings ─────────────
def canon(v):
    if isinstance(v, BaseException):
        return exc_desc(v)
    if isinstance(v, tuple):
        return [canon(x) for x in v]
    if isinstance(v, set):
        return sorted((canon(x) for x in v), key=lambda o: json.dumps(o))
    if isinstance(v, dict):
        return {(str(k) if not isinstance(k, str) else k): canon(x)
                for k, x in v.items()}
    return v


# ── extraction ──────────────────────────────────────────────────────────────
def _exec_nodes(nodes, src, ns, flags):
    for node in nodes:
        exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                     flags=flags), ns)


def install_stubs():
    """Function-local `from agent.compaction_display import compaction_summary`
    resolves via sys.modules at call time; register a recorder there."""
    def reg(name, attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        parts = name.split(".")
        for i in range(1, len(parts)):
            pname = ".".join(parts[:i])
            if pname not in sys.modules:
                sys.modules[pname] = types.ModuleType(pname)
            setattr(sys.modules[pname], parts[i], sys.modules[".".join(parts[:i + 1])])
        return m

    def compaction_summary(message):
        return seam_rec("compaction_summary", {"message": message})

    reg("agent.compaction_display", {"compaction_summary": compaction_summary})


def extract():
    import __future__
    flags = __future__.annotations.compiler_flag
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("session_transcript_rows_flow2_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({"__name__": "session_transcript_rows_flow2_extracted",
               "json": json})
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(
                isinstance(t, ast.Name) for t in node.targets):
            with contextlib.suppress(Exception):
                _exec_nodes([node], SRC, ns, flags)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            _exec_nodes([node], SRC, ns, flags)
    for name in sorted(WANTED) + ["ROW_KIND_MESSAGE", "ROW_KIND_COMPACTION",
                                  "_MARK_KEY", "_MARK_TEXT"]:
        assert name in ns, "extraction missed %s" % name
    CUR["ns"] = ns
    install_stubs()
    return ns


# ── live collaborators ──────────────────────────────────────────────────────
class RowDouble:
    """A row handle returned by transcript_handoff_rows; .as_wire_row fires a
    seam exactly like the C universal-seam call."""
    def __init__(self, handle):
        self._handle = handle

    def as_wire_row(self):
        return seam_rec("row.as_wire_row", {"row": self._handle})


def make_store():
    def transcript_handoff_rows(shown_ids, segments):
        r = seam_rec("messages.transcript_handoff_rows",
                     {"shown_ids": shown_ids, "segments": segments})
        # Live does .items() directly: a None seam result raises AttributeError
        # with the right type name, which the C port models too.
        return {k: [RowDouble(h) for h in v] for k, v in r.items()}
    return SimpleNamespace(
        messages=SimpleNamespace(transcript_handoff_rows=transcript_handoff_rows))


def make_earlier():
    def earlier(candidates):
        r = seam_rec("earlier", {"candidates": candidates})
        # set(...) needs hashable tuples; C emits occurrences as 2-arrays.
        if isinstance(r, list):
            return [tuple(x) if isinstance(x, list) else x for x in r]
        return r
    return earlier


def make_project():
    def project(marked):
        return seam_rec("project", {"marked": marked})
    return project


# ── drive one case against the LIVE extracted module ───────────────────────
def drive(func, p):
    ns = CUR["ns"]
    CUR["trace"] = []
    CUR["pos"] = {}
    CUR["io"] = p.get("io", {}) or {}
    CUR["last_raise"] = None
    try:
        if func == "handoff_row":
            ret = ns["handoff_row"](p["message"], p["summary"])
        elif func == "_mark_index":
            ret = ns["_mark_index"](p["row"])
        elif func == "handoff_occurrence":
            ret = ns["handoff_occurrence"](p["message"])
        elif func == "shown_occurrences":
            ret = ns["shown_occurrences"](p["messages"])
        elif func == "handoff_rows":
            ret = ns["handoff_rows"](make_store(), p["shown_ids"], p["segments"])
        elif func == "page_handoffs":
            earlier = make_earlier() if p.get("earlier_present") else None
            ret = ns["page_handoffs"](make_store(), p["rows"], p["segments"],
                                      earlier)
        elif func == "project_entries":
            ret = ns["project_entries"](p["entries"], make_project())
        else:
            return {"error": "unknown func", "ret": None, "trace": CUR["trace"]}
        return {"ret": canon(ret), "trace": CUR["trace"]}
    except BaseException as e:
        return {"ret": exc_desc(e), "trace": CUR["trace"]}


def main():
    extract()
    total = bad = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        case = json.loads(line)
        total += 1
        func = case.get("func", "")
        inp = case.get("in", {}) or {}
        try:
            expect = drive(func, inp)
        except BaseException as e:  # oracle-side failure: fail closed loudly
            print("ORACLE session_transcript_rows_flow2: FATAL in drive(%s): %r"
                  % (func, e), file=sys.stderr)
            bad += 1
            continue
        try:
            harness = json.loads(line)
            got = harness.get("out", {})
        except Exception as e:
            print("ORACLE session_transcript_rows_flow2: bad harness line %r" % e,
                  file=sys.stderr)
            bad += 1
            continue
        got_ret = got.get("ret")
        got_trace = got.get("trace", [])
        if got_ret != expect["ret"] or got_trace != expect["trace"]:
            bad += 1
            if bad <= 12:
                print("MISMATCH %s/%s" % (func, json.dumps(inp, ensure_ascii=True)))
                print("  ret   got=%r want=%r" % (got_ret, expect["ret"]))
                print("  trace got=%r want=%r" % (got_trace, expect["trace"]))
    print("ORACLE session_transcript_rows_flow2: %d cases, %d mismatches" %
          (total, bad))
    if total == 0:
        print("ORACLE session_transcript_rows_flow2: 0 cases (fail-closed)",
              file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
