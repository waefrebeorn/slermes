#!/usr/bin/env python3
"""Oracle for the tools/fuzzy_match.py UNCLAIMED-gap FLOW port
(src/tools/fuzzy_match_flow2.c).

AST-extracts the 17 unclaimed live defs (plus the two inline claimed pure
leaves _unicode_normalize/_calculate_line_positions) with NO heavy import,
seeds constants, and replaces every claimed sibling / callable collaborator
with a recorder driven by the SAME in.io scripts the C harness feeds its
universal seam. Compares ret + ordered trace + mutated correction_notes.

Script shapes (both sides): plain value reused; {"__seq__":[...]} per call
(last repeats, empty -> None); {"__raise__":kind,"msg":text} raises a real
exception; a scripted null -> None; ABSENT key -> fail-closed RuntimeError.
The C port carries the FULL builtin exception hierarchy so _note_edit_match's
`except Exception` swallows every Exception subclass but not the
SystemExit/KeyboardInterrupt/GeneratorExit trio.

Input: stdin JSON lines from t_port_fuzzy_match_flow2.
Summary: ORACLE fuzzy_match_flow2: N cases, M mismatches. Fails on 0 cases.
"""
import ast
import bisect
import __future__
import json
import os
import re
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"

CUR = {"func": None, "in": {}, "trace": []}


def rec(ev):
    CUR["trace"].append(ev)


def script(name, args):
    io = CUR["in"].get("io") or {}
    if name not in io:
        spec = {"__raise__": "RuntimeError", "msg": "no script for %s" % name}
        rec({"ev": name, "args": args, "ret": spec})
        return spec
    sc = io[name]
    val = sc
    if isinstance(sc, dict) and "__seq__" in sc:
        seq = sc["__seq__"]
        idx = CUR.get("seqpos", {}).get(name, 0)
        if not seq:
            val = None
        else:
            if idx >= len(seq):
                idx = len(seq) - 1
            val = seq[idx]
            CUR["seqpos"][name] = idx + 1
    rec({"ev": name, "args": args, "ret": val})
    return val


# Full builtin exception hierarchy (mirrors fm2_exc_parent in the C port).
_BUILTINS = {
    "BaseException": BaseException, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError,
    "ValueError": ValueError, "UnicodeError": UnicodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError,
    "TypeError": TypeError, "AttributeError": AttributeError,
    "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "LookupError": LookupError, "IndexError": IndexError, "KeyError": KeyError,
    "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
    "ZeroDivisionError": ZeroDivisionError, "OverflowError": OverflowError,
    "AssertionError": AssertionError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "ReferenceError": ReferenceError,
    "MemoryError": MemoryError, "BufferError": BufferError, "EOFError": EOFError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "FileExistsError": FileExistsError,
    "InterruptedError": InterruptedError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ProcessLookupError": ProcessLookupError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "TimeoutError": TimeoutError,
    "Warning": Warning, "UserWarning": UserWarning,
    "DeprecationWarning": DeprecationWarning, "FutureWarning": FutureWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "ResourceWarning": ResourceWarning, "ImportWarning": ImportWarning,
    "UnicodeWarning": UnicodeWarning, "BytesWarning": BytesWarning,
    "EncodingWarning": EncodingWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
}


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    cls = _BUILTINS.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(msg)


def materialize(v):
    if isinstance(v, dict) and "__raise__" in v:
        raise make_exc(v)
    return v


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


def norm_nums(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


# ── live-def extraction ───────────────────────────────────────────────────
TARGETS = {
    "window_spans": "_window_spans",
    "match_transformed_lines": "_match_transformed_lines",
    "strip_boundary": "_strip_boundary",
    "invert_norm_map": "_invert_norm_map",
    "norm_end_to_orig": "_norm_end_to_orig",
    "boundary_note": "boundary_note",
    "matched_regions": "_matched_regions",
    "run_strategy_chain": "_run_strategy_chain",
    "strip_line_number_gutter": "_strip_line_number_gutter",
    "gutter_stripped_old": "_gutter_stripped_old",
    "gutter_recovery": "_gutter_recovery",
    "note_edit_match": "_note_edit_match",
    "detect_newline_literal_drift": "_detect_newline_literal_drift",
    "backslash_runs": "_backslash_runs",
    "detect_backslash_doubling": "_detect_backslash_doubling",
    "match_starts_mid_line": "_match_starts_mid_line",
    "unicode_edit_plan": "_unicode_edit_plan",
}
# two claimed pure leaves transcribed inline in C (so run REAL in the oracle)
INLINE = ("_unicode_normalize", "_calculate_line_positions")
NEED = set(TARGETS.values()) | set(INLINE)
CONSTS = ("IDENTICAL_STRINGS_ERROR", "GUTTER_STRIPPED_NOTE",
          "GUTTER_NEW_STRING_ERROR", "GUTTER_MISS_SUFFIX",
          "UNICODE_MAP", "SIMILARITY_STRATEGIES")
PRECISE_NAMES = ["exact", "line_trimmed", "whitespace_normalized",
                 "indentation_flexible", "escape_normalized", "trimmed_boundary",
                 "unicode_normalized"]


def extract():
    from typing import Callable, Iterable, Optional
    path = os.path.join(QUARRY, "tools", "fuzzy_match.py")
    src = open(path, encoding="utf-8").read()
    ns = {
        "__name__": "fm2_oracle_extracted", "__file__": path,
        "re": re, "bisect": bisect, "Span": tuple,
        "Callable": Callable, "Iterable": Iterable, "Optional": Optional,
    }
    flags = 0
    for name in CONSTS:
        try:
            flags = __future__.annotations.compiler_flag
        except Exception:
            pass
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in CONSTS for t in node.targets):
            try:
                val = ast.literal_eval(node.value)
            except Exception:
                # frozenset(...) / re.compile(...) etc.
                try:
                    val = eval(compile(ast.Expression(node.value), path, "eval"),
                               {"frozenset": frozenset, "re": re})
                except Exception:
                    val = None
            for t in node.targets:
                if isinstance(t, ast.Name):
                    ns[t.id] = val
    ns["_GUTTER_LINE"] = re.compile(r"^(\d+)\|", re.MULTILINE)
    if ns.get("SIMILARITY_STRATEGIES") is None:
        ns["SIMILARITY_STRATEGIES"] = frozenset({"block_anchor", "context_aware"})
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in NEED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
    missing = (NEED | set(CONSTS)) - set(ns)
    if missing:
        raise RuntimeError("extraction missing: %s" % sorted(missing))
    return ns


NS = extract()


def seamfn(name, argfn):
    def f(*a, **k):
        return materialize(script(name, argfn(*a, **k)))
    f.__sp_name__ = name
    return f


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def install_globals():
    # claimed siblings -> universal seams (arg names match the C args object)
    NS["_format_match_locations"] = seamfn(
        "format_match_locations",
        lambda content, matches, cap=5: {"content": content, "matches": matches, "cap": cap})
    NS["_detect_escape_drift"] = seamfn(
        "detect_escape_drift",
        lambda content, matches, old_string, new_string: {
            "content": content, "matches": matches,
            "old_string": old_string, "new_string": new_string})
    NS["_maybe_unescape_new_string"] = seamfn(
        "maybe_unescape_new_string",
        lambda new_string, content, matches: {
            "new_string": new_string, "content": content, "matches": matches})
    NS["_apply_replacements"] = seamfn(
        "apply_replacements",
        lambda content, matches, new_string, old_string=None, unicode_plan=None: {
            "content": content, "matches": matches, "new_string": new_string,
            "old_string": old_string, "unicode_plan": unicode_plan})

    # strategy list constants returned by _gutter_stripped_old (fn -> SeamFn)
    def pair(name):
        return (name, seamfn(name, lambda content, old_string: {
            "content": content, "old_string": old_string}))
    NS["_WHOLE_LINE_STRATEGIES"] = [pair("line_trimmed")]
    NS["_PRECISE_STRATEGIES"] = [pair(n) for n in PRECISE_NAMES]

    # difflib.SequenceMatcher.get_opcodes -> seam "opcodes"
    class SeqStub:
        def __init__(self, isjunk, a, b):
            self.a, self.b = a, b

        def get_opcodes(self):
            return materialize(script("opcodes", {"a": self.a, "b": self.b}))
    NS["SequenceMatcher"] = SeqStub

    # in-function import in _note_edit_match: module __getattr__ fires the
    # "import_note_edit_match" seam (may raise -> except Exception gate).
    def note_recorder(strategy, miss):
        return materialize(script("note_edit_match",
                                  {"strategy": strategy, "miss": miss}))

    class MetricsModule(types.ModuleType):
        def __getattr__(self, name):
            if name == "note_edit_match":
                materialize(script("import_note_edit_match", {}))
                return note_recorder
            raise AttributeError(name)

    pkg("hermes_cli")
    pkg("hermes_cli.observability")
    sys.modules["hermes_cli.observability.shared_metrics_harness"] = MetricsModule(
        "hermes_cli.observability.shared_metrics_harness")


install_globals()


# ── drive ─────────────────────────────────────────────────────────────────
def spans(inp):
    return [tuple(p) for p in (inp or [])]


def strat_pairs(names):
    out = []
    for n in (names or []):
        out.append((n, seamfn(n, lambda content, old_string: {
            "content": content, "old_string": old_string})))
    return out


def run_flow(fn, notes):
    try:
        ret = fn()
    except BaseException as e:
        ret = raise_spec(e)
    return {"ret": ret, "trace": CUR["trace"],
            "state": {"correction_notes": notes}}


def drive(func, inp):
    f = NS[TARGETS[func]]
    has_notes = isinstance(inp.get("correction_notes"), list)
    notes = list(inp["correction_notes"]) if has_notes else None
    content = inp.get("content", "")
    old = inp.get("old_string", "")
    new = inp.get("new_string", "")

    if func == "window_spans":
        acc = seamfn("accept", lambda i: {"i": i})
        return run_flow(lambda: f(content, spans(inp.get("content_lines")),
                                  inp.get("n", 1), acc), notes)
    if func == "match_transformed_lines":
        tf = seamfn("transform", lambda lines: {"lines": lines})
        return run_flow(lambda: f(content, inp.get("pattern", ""), tf), notes)
    if func == "strip_boundary":
        return run_flow(lambda: f(inp.get("lines") or []), notes)
    if func == "invert_norm_map":
        return run_flow(lambda: f(inp.get("orig_to_norm") or []), notes)
    if func == "norm_end_to_orig":
        return run_flow(lambda: f(inp.get("orig_to_norm") or [],
                                  inp.get("orig_start", 0), inp.get("norm_end", 0)), notes)
    if func == "boundary_note":
        kw = {}
        if "target" in inp:
            kw["target"] = inp["target"]
        if "where" in inp:
            kw["where"] = inp["where"]
        return run_flow(lambda: f(inp.get("strategies") or [], **kw), notes)
    if func == "matched_regions":
        return run_flow(lambda: f(content, spans(inp.get("matches"))), notes)
    if func == "run_strategy_chain":
        return run_flow(lambda: f(content, old, new, inp.get("replace_all", False),
                                  strat_pairs(inp.get("strategies"))), notes)
    if func == "strip_line_number_gutter":
        if "min_numbered" in inp:
            return run_flow(lambda: f(inp.get("text", ""), inp["min_numbered"]), notes)
        return run_flow(lambda: f(inp.get("text", "")), notes)
    if func == "gutter_stripped_old":
        return run_flow(lambda: f(content, old), notes)
    if func == "gutter_recovery":
        return run_flow(lambda: f(content, inp.get("stripped_old", ""),
                                  strat_pairs(inp.get("strategies")), new,
                                  inp.get("replace_all", False), notes), notes)
    if func == "note_edit_match":
        return run_flow(lambda: f(inp.get("strategy"), inp.get("miss")), notes)
    if func == "detect_newline_literal_drift":
        return run_flow(lambda: f(content, spans(inp.get("matches")), old, new), notes)
    if func == "backslash_runs":
        return run_flow(lambda: f(inp.get("s", "")), notes)
    if func == "detect_backslash_doubling":
        return run_flow(lambda: f(inp.get("matched_regions", ""), old, new), notes)
    if func == "match_starts_mid_line":
        return run_flow(lambda: f(content, inp.get("start", 0), inp.get("end", 0)), notes)
    if func == "unicode_edit_plan":
        return run_flow(lambda: f(old, new), notes)
    raise ValueError("unknown func: %s" % func)


def canon(x):
    if isinstance(x, BaseException):
        return canon(raise_spec(x))
    if getattr(x, "__sp_name__", None):
        return ("str", x.__sp_name__)
    if isinstance(x, tuple) and len(x) == 2 and getattr(x[1], "__sp_name__", None):
        return canon(x[0])  # strategy (name, SeamFn) pair -> the name
    if isinstance(x, dict):
        return ("obj", sorted(((str(k), canon(v)) for k, v in x.items()),
                              key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = norm_nums(record.get("in") or {})
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["seqpos"] = {}
        try:
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": raise_spec(e), "trace": CUR["trace"],
                    "state": {"correction_notes": None}}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False, default=str))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE fuzzy_match_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
