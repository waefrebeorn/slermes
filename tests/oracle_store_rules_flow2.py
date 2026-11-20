#!/usr/bin/env python3
"""Oracle for the unclaimed turn-policy leaves FLOW port
(src/persistence/store_rules_flow2.c).

Defining module: session_rules/turns.py. rules.py itself has all 11 defs claimed
by store_rules_flow.c; the 10 defs driven here are the remaining UNCLAIMED pure
turn-policy leaves: _field, _part_text, _with_part_text,
_content_text_for_contains, is_content_blank, drop_stale_api_content,
_display_metadata_str, withheld_prompt_text, typed_prompt_content,
retryable_user_text.

The live defs are AST-extracted (FunctionDef nodes) from turns.py into a
registered sys.modules namespace; annotations stay lazy under the future-
annotations compiler flag. Seeded constants: WITHHELD_PROMPT and
PRE_LLM_BLOCK_WITHHELD_PROMPT. The heavy quarry import chain is never imported.

SCRIPTED seams: carried for contract parity (Ctx/make_exc/transport) but this
cluster has no external collaborators -- leaves call each other directly, so
the recorded trace is empty on both sides. Compares the DECISION SEQUENCE,
the final return ("ret") and the mutated dict params ("state").

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE store_rules_flow2: N cases, M mismatches'.

--self-test drives every fixture through the LIVE defs and reports drive/script
failures. --shadow-test runs a literal Python mirror of the C port over every
fixture and diffs it against the live drive (expect 0 shadow diffs).
"""
import ast
import json
import math
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from collections.abc import Mapping, MutableMapping
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from pathlib import Path
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
TURNS_SRC = os.path.join(Q, "session_rules", "turns.py")

_FLAGS = _fut.annotations.compiler_flag

WITHHELD_PROMPT = "withheld_prompt"
PRE_LLM_BLOCK_WITHHELD_PROMPT = "[Message withheld: a pre_llm_call policy hook blocked it.]"


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


KINDS = ["_field", "_part_text", "_with_part_text", "_content_text_for_contains",
         "is_content_blank", "drop_stale_api_content", "_display_metadata_str",
         "withheld_prompt_text", "typed_prompt_content", "retryable_user_text"]
MUT_KINDS = {"drop_stale_api_content": ("msg",)}

# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            if x.get("__float__") is True:
                f = float(x.get("v", 0))
                return int(f) if math.isfinite(f) and f.is_integer() else f
            if x.get("__tuple__") is True:
                return [conv(i) for i in x.get("v", [])]
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float):
            return int(x) if math.isfinite(x) and x.is_integer() else x
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection (FULL builtin hierarchy) ──
EXC_CLASSES = {
    "BaseException": BaseException, "Exception": Exception,
    "TypeError": TypeError, "StopIteration": StopIteration,
    "StopAsyncIteration": StopAsyncIteration, "ArithmeticError": ArithmeticError,
    "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "AttributeError": AttributeError, "BufferError": BufferError,
    "EOFError": EOFError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "LookupError": LookupError,
    "IndexError": IndexError, "KeyError": KeyError, "MemoryError": MemoryError,
    "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError, "Warning": Warning,
    "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning, "ExceptionGroup": ExceptionGroup,
    "BaseExceptionGroup": BaseExceptionGroup, "GeneratorExit": GeneratorExit,
    "KeyboardInterrupt": KeyboardInterrupt, "SystemExit": SystemExit,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    en = spec.get("errno")
    if en is not None and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


# ── per-case script backend (mirrors the C sr2_call exactly; no seams here) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


# ── value materialization / transport ──
def deep_mat(v, ctx=None):
    if isinstance(v, dict):
        if v.get("__float__") is True:
            return float(v.get("v", 0))
        if v.get("__tuple__") is True:
            return tuple(deep_mat(i, ctx) for i in v.get("v", []))
        if v.get("__dt__") is True:
            return types.SimpleNamespace(tzinfo=v.get("tzinfo"),
                                          microsecond=v.get("microsecond", 0))
        return {k: deep_mat(x, ctx) for k, x in v.items()}
    if isinstance(v, list):
        return [deep_mat(i, ctx) for i in v]
    return v


def transport(v):
    if isinstance(v, float):
        return {"__float__": True, "v": v}
    if isinstance(v, tuple):
        return {"__tuple__": True, "v": [transport(i) for i in v]}
    if isinstance(v, dict):
        return {k: transport(x) for k, x in v.items()}
    if isinstance(v, list):
        return [transport(i) for i in v]
    return v


# ── seam registry (empty for this pure cluster; carried for contract) ──
SEAM_NAMES = []


# ── turns.py leaf namespace (live defs + seeded constants) ──
def extract_turns():
    mod = new_module("sr2_oracle_turns")
    ns = mod.__dict__
    ns.update({"Any": Any, "Optional": Optional, "Mapping": Mapping,
               "MutableMapping": MutableMapping,
               "WITHHELD_PROMPT": WITHHELD_PROMPT,
               "PRE_LLM_BLOCK_WITHHELD_PROMPT": PRE_LLM_BLOCK_WITHHELD_PROMPT})
    src = open(TURNS_SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in KINDS:
            exec_node(src, node, ns, TURNS_SRC)
    missing = [k for k in KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_store_rules_flow2: turns defs missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_turns()


# ── per-case drive ──
def drive(func, inp, ctx=None):
    args = inp.get("args") or {}
    if ctx is None:
        ctx = Ctx(inp.get("io"))
    mut = {}

    def mat(k, default=None):
        return deep_mat(args.get(k, default), ctx)

    try:
        if func == "_field":
            ret = NS["_field"](mat("value"), args.get("key", ""))
        elif func == "_part_text":
            ret = NS["_part_text"](mat("item"))
        elif func == "_with_part_text":
            ret = NS["_with_part_text"](mat("item"), mat("text"))
        elif func == "_content_text_for_contains":
            ret = NS["_content_text_for_contains"](mat("content"))
        elif func == "is_content_blank":
            ret = NS["is_content_blank"](mat("content"))
        elif func == "drop_stale_api_content":
            mut["msg"] = mat("msg")
            ret = NS["drop_stale_api_content"](mut["msg"])
        elif func == "_display_metadata_str":
            ret = NS["_display_metadata_str"](mat("message"), args.get("key", ""))
        elif func == "withheld_prompt_text":
            ret = NS["withheld_prompt_text"](mat("message"))
        elif func == "typed_prompt_content":
            ret = NS["typed_prompt_content"](mat("content"), mat("message"))
        elif func == "retryable_user_text":
            ret = NS["retryable_user_text"](mat("content"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state = {k: transport(v) for k, v in mut.items()}
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ─────────────────────────────────────────────────────────────────────────────
# SHADOW MIRROR: a literal Python transcription of the C port sr2_* functions.
# Operates on transported args and emits transported values; diffed vs live
# drive() over every fixture. Zero diffs proves the C transcription faithful.
# ─────────────────────────────────────────────────────────────────────────────
def _sdict(v):
    return isinstance(v, dict) and not ({k for k in ("__float__", "__tuple__", "__dt__")} & set(v))


def _sstr(v):
    return isinstance(v, str)


def _snull(v):
    return v is None


def _sjoin(items, sep=""):
    return sep.join(items)


def shadow(field, args):
    g = args.get

    def part_text(item):
        if _sstr(item):
            return item
        if _sdict(item):
            return item.get("text")
        return None

    if field == "_field":
        v = g("value")
        return v.get(g("key", "")) if _sdict(v) else None
    if field == "_part_text":
        return part_text(g("item"))
    if field == "_with_part_text":
        item, text = g("item"), g("text")
        if _sdict(item):
            d = dict(item)
            d["text"] = text
            return d
        return text
    if field == "_content_text_for_contains":
        c = g("content")
        if isinstance(c, list):
            return _sjoin([t for t in (part_text(x) for x in c)
                           if _sstr(t) and t], "\n")
        if _snull(c):
            return ""
        if _sstr(c):
            return c
        return "<object>"
    if field == "is_content_blank":
        c = g("content")
        if _snull(c):
            return True
        if _sstr(c):
            return not c.strip()
        if isinstance(c, list):
            joined = "".join(p.get("text", "")
                             for p in c
                             if _sdict(p) and p.get("type") == "text")
            return not joined.strip()
        return False
    if field == "drop_stale_api_content":
        m = g("msg")
        if not _sdict(m):
            return {"__raise__": "AttributeError",
                    "message": f"'{type(m).__name__}' object has no attribute 'pop'"}
        m.pop("api_content", None)
        return None
    if field == "_display_metadata_str":
        m = g("message")
        md = m.get("display_metadata") if _sdict(m) else None
        v = md.get(g("key", "")) if _sdict(md) else None
        return v if _sstr(v) else None
    if field == "withheld_prompt_text":
        m = g("message")
        md = m.get("display_metadata") if _sdict(m) else None
        v = md.get(WITHHELD_PROMPT) if _sdict(md) else None
        return v if _sstr(v) else None
    if field == "typed_prompt_content":
        c = g("content")
        m = g("message")
        md = m.get("display_metadata") if _sdict(m) else None
        typed = md.get(WITHHELD_PROMPT) if _sdict(md) else None
        typed = typed if _sstr(typed) else None
        if typed is None:
            return c
        if _sstr(c):
            return c.replace(PRE_LLM_BLOCK_WITHHELD_PROMPT, typed)
        if isinstance(c, list):
            out = []
            for part in c:
                if _sdict(part) and _sstr(part.get("text")):
                    d = dict(part)
                    d["text"] = part["text"].replace(PRE_LLM_BLOCK_WITHHELD_PROMPT, typed)
                    out.append(d)
                else:
                    out.append(part)
            return out
        return c
    if field == "retryable_user_text":
        c = g("content")
        if not isinstance(c, (str, list)):
            return {"__raise__": "ValueError", "message": "retry does not support non-text content"}
        parts = [c] if isinstance(c, str) else c
        chunks = []
        for part in parts:
            if isinstance(part, str):
                chunks.append(part)
                continue
            if not isinstance(part, dict):
                return {"__raise__": "ValueError", "message": "retry does not support non-text content"}
            if part.get("type") not in {"text", "input_text", "output_text"}:
                return {"__raise__": "ValueError",
                        "message": "retry does not support media or unknown content parts"}
            if set(part) - {"type", "text"}:
                return {"__raise__": "ValueError",
                        "message": "retry cannot losslessly flatten annotated text parts"}
            if not isinstance(part.get("text"), str):
                return {"__raise__": "ValueError", "message": "retry text parts must contain text"}
            chunks.append(part["text"])
        text = "".join(chunks)
        if not text.strip():
            return {"__raise__": "ValueError", "message": "retry found no text to send"}
        return text
    raise RuntimeError(f"unknown shadow func {field}")


# ── fixture / cross-check ──
def fixpath():
    return _REPO / "tests" / "fixtures" / "store_rules_flow2.jsonl"


def each_fixture():
    p = fixpath()
    if not p.exists():
        return []
    out = []
    for line in open(p, encoding="utf-8", errors="surrogatepass"):
        line = line.strip()
        if line:
            out.append(json.loads(line))
    return out


def cross_check():
    csrc = open(_REPO / "src" / "persistence" / "store_rules_flow2.c",
                encoding="utf-8").read()
    pop_missing = [k for k in KINDS
                   if f"sr2_{k.lstrip('_')} " not in csrc and f"sr2_{k.lstrip('_')}(" not in csrc]
    if pop_missing:
        print(f"FATAL oracle_store_rules_flow2: PoP funcs absent from C port: {pop_missing}")
        sys.exit(2)
    fix = fixpath()
    if fix.exists():
        counts = {}
        bad_io = set()
        for case in each_fixture():
            f = case.get("func", "")
            counts[f] = counts.get(f, 0) + 1
            bad_io |= set((case.get("in") or {}).get("io") or {})
        if bad_io - set(SEAM_NAMES):
            print(f"FATAL oracle_store_rules_flow2: fixture io keys without a seam: {sorted(bad_io)}")
            sys.exit(2)
        unknown = set(counts) - set(KINDS)
        if unknown:
            print(f"FATAL oracle_store_rules_flow2: fixture funcs without a drive: {sorted(unknown)}")
            sys.exit(2)
        untested = set(KINDS) - set(counts)
        if untested:
            print(f"FATAL oracle_store_rules_flow2: drive funcs without a fixture case: {sorted(untested)}")
            sys.exit(2)
        thin = {f: c for f, c in counts.items() if c < 5}
        if thin:
            print(f"FATAL oracle_store_rules_flow2: funcs with <5 fixture cases: {thin}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")


def main():
    cross_check()
    n = m = shown = 0
    data = sys.stdin.buffer.read().decode("utf-8", errors="surrogatepass")
    for line in data.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE store_rules_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    n = bad = 0
    counts = {}
    watch = KINDS + SEAM_NAMES
    for case in each_fixture():
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
    print(f"SELFTEST store_rules_flow2: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


def shadow_test():
    """Literal C-mirror vs live drive over every fixture; expect 0 diffs."""
    n = bad = 0
    for case in each_fixture():
        n += 1
        func = case["func"]
        args = (case.get("in") or {}).get("args") or {}
        want = drive(func, case.get("in") or {})
        import copy
        shadow_args = copy.deepcopy(args)
        raw = shadow(func, shadow_args)
        got = {"ret": canon(raw), "trace": [], "state": {}}
        if MUT_KINDS.get(func):
            got["state"] = canon({k: shadow_args.get(k) for k in MUT_KINDS[func]})
        g = canon(got)
        if g != want:
            bad += 1
            print(f"SHADOW DIFF {func} case {n}:")
            print(f"  live={json.dumps(want, ensure_ascii=False)[:400]}")
            print(f"  shad={json.dumps(g, ensure_ascii=False)[:400]}")
    print(f"SHADOW store_rules_flow2: {n} cases, {bad} shadow diffs")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    if len(sys.argv) > 1 and sys.argv[1] == "--shadow-test":
        sys.exit(shadow_test())
    main()
