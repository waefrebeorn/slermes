#!/usr/bin/env python3
"""Oracle for the turns.py REMAINDER beyond turns_flow3 (turns_flow4.c).

Defining module: session_rules/turns.py. A CRLF-aware PoP grep over src/+include/
shows 56 turns.py defs already claimed (turns_flow2/flow3 + sr/sr2/si/flow ports).
This module claims a coherent, self-contained slice of the 23 remaining UNCLAIMED
defs: the NUL-sentinel codec (decode_content), the skill-scaffold describer
cluster (extract_user_instruction_from_skill_message,
_describe_auto_loaded_skill_turn, describe_skill_invocation, _cut_after) and the
global-mutable gateway auto-load registry (register_auto_load_scaffold,
_registered_auto_load_tail, _auto_load_typed_text).

The 8 defs in KINDS are all UNCLAIMED at authoring time. The multi-pass
sequence-repair orchestrators (repair_message_sequence + its 4 passes + the
absorbed-row helpers) and the regex scrubber (sanitize_context / view projection)
stay out of scope for this libjson-only FLOW shape; see the C header for the
scope note. Every PoP in the C port names an unclaimed def.

The live defs are AST-extracted from turns.py into a registered sys.modules
namespace; module constants are seeded by executing top-level assignments (so
the compile()-time regex objects resolve). The heavy quarry import chain is
never imported. Claimed collaborators this cluster needs (_json_or) are extracted
as private dependencies but are NOT in KINDS, so no claim is made here.

Compares the decision SEQUENCE (scripted seams, empty for this cluster), the
final return ("ret") and the mutated registry ("state.scaffolds"). Fail-closed:
missing defs or 0 cases => exit 2. Per-case wrapper catches BaseException.
Full builtin exception hierarchy is carried (EXC_CLASSES) and audited.
Summary: 'ORACLE turns_flow4: N cases, M mismatches'.

--self-test drives every fixture through the LIVE defs.
--shadow-test runs a literal Python mirror of the C port over every fixture and
diffs it against the live drive (expect 0 shadow diffs).
"""
import ast
import copy
import json
import logging
import os
import re
import sys
import textwrap
import threading
import types
from collections import OrderedDict

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from pathlib import Path
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
TURNS_SRC = os.path.join(Q, "session_rules", "turns.py")


# 8 UNCLAIMED defs (KINDS) + 1 claimed private dep (extracted, NOT claimed).
KINDS = [
    "decode_content", "extract_user_instruction_from_skill_message",
    "_describe_auto_loaded_skill_turn", "describe_skill_invocation",
    "_cut_after", "register_auto_load_scaffold",
    "_registered_auto_load_tail", "_auto_load_typed_text",
]
PRIVATE_DEPS = ["_json_or"]


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    # turns.py has `from __future__ import annotations`; compile with the flag so
    # annotations are not evaluated at exec time.
    import __future__ as _fut
    exec(compile(text, path, "exec", flags=_fut.annotations.compiler_flag,
                 dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            if x.get("__float__") is True:
                f = float(x.get("v", 0))
                return int(f) if f.is_integer() else f
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
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

_CATCH_ALIASES = {
    "IOError": OSError, "EnvironmentError": OSError, "WindowsError": OSError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind) or _CATCH_ALIASES.get(kind) or type(kind, (Exception,), {})
    msg = spec.get("message", "")
    en = spec.get("errno")
    try:
        if en is not None and issubclass(cls, OSError):
            return cls(en, msg)
        return cls(msg)
    except TypeError:
        # Some builtins (e.g. UnicodeDecodeError) need >1 ctor arg; the audit
        # only needs a deterministic instance, so fall back to a bare base.
        try:
            return cls()
        except Exception:
            return Exception(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


def caught(spec, catch_name):
    base = EXC_CLASSES.get(catch_name) or _CATCH_ALIASES.get(catch_name)
    if base is None:
        return False
    return isinstance(make_exc(spec), base)


def audit_exceptions():
    """Pairwise audit over all builtins, normalizing alias catch targets."""
    bad = []
    for kn in EXC_CLASSES:
        spec = {"type": kn, "message": "x"}
        for cn in list(EXC_CLASSES) + list(_CATCH_ALIASES):
            real = EXC_CLASSES.get(cn) or _CATCH_ALIASES.get(cn)
            want = isinstance(make_exc(spec), real)
            if caught(spec, cn) != want:
                bad.append((kn, cn))
    return bad


# ── per-case script backend (mirrors C tf4_call; no seams here) ──
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
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            entry = entry[idx]
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                self._record_raise(name, args, make_exc(entry["raise"]))
            ret = entry.get("ret")
        else:
            ret = entry
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


# ── value materialization / transport ──
def deep_mat(v):
    if isinstance(v, dict):
        return {k: deep_mat(x) for k, x in v.items()}
    if isinstance(v, list):
        return [deep_mat(i) for i in v]
    return v


# ── seam registry (empty for this pure cluster; carried for contract) ──
SEAM_NAMES = []


# ── turns.py namespace (live defs + seeded constants) ──
class _NullLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def extract_turns():
    mod = new_module("tf4_oracle_turns")
    ns = mod.__dict__
    ns.update({
        "Any": object, "Optional": object, "Mapping": dict,
        "MutableMapping": dict, "re": re, "json": json, "logging": logging,
        "copy": copy, "threading": threading, "OrderedDict": OrderedDict,
        "Counter": dict,
        "CONTENT_JSON_PREFIX": "\x00json:",
        "CONTENT_REFUSED_DISPLAY_KIND": "refused",
    })
    src = open(TURNS_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    import __future__ as _fut
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]),
                             "<turns-const>", "exec", flags=_fut.annotations.compiler_flag,
                             dont_inherit=True), ns)
            except Exception:
                pass
    quiet = logging.getLogger("tf4_oracle_quiet")
    quiet.addHandler(logging.NullHandler())
    quiet.propagate = False
    ns["logger"] = quiet
    want = set(KINDS) | set(PRIVATE_DEPS)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in want:
            exec_node(src, node, ns, TURNS_SRC)
    missing = [k for k in want if k not in ns]
    if missing:
        print(f"FATAL oracle_turns_flow4: turns defs missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_turns()


def _scaffolds_from(inp):
    st = inp.get("state") or {}
    sc = st.get("scaffolds") or []
    return [str(x) for x in sc] if isinstance(sc, list) else []


def drive(func, inp, ctx=None):
    args = inp.get("args") or {}
    if ctx is None:
        ctx = Ctx(inp.get("io"))
    sc = _scaffolds_from(inp)
    NS["_AUTO_LOAD_SCAFFOLDS"] = OrderedDict.fromkeys(sc)
    NS["_AUTO_LOAD_SCAFFOLDS_LOCK"] = _NullLock()

    def mat(k, default=None):
        return deep_mat(args.get(k, default))

    try:
        if func == "decode_content":
            ret = NS["decode_content"](mat("content"))
        elif func == "extract_user_instruction_from_skill_message":
            ret = NS["extract_user_instruction_from_skill_message"](mat("content"))
        elif func == "_describe_auto_loaded_skill_turn":
            ret = NS["_describe_auto_loaded_skill_turn"](mat("content"))
        elif func == "describe_skill_invocation":
            ret = NS["describe_skill_invocation"](mat("content"), mat("separator", " \u2014 "))
        elif func == "_cut_after":
            msg = mat("message")
            findfn = msg.find if mat("find", "find") == "find" else msg.rfind
            ret = NS["_cut_after"](msg, mat("marker"), mat("stop_marker"), findfn)
        elif func == "register_auto_load_scaffold":
            ret = NS["register_auto_load_scaffold"](mat("scaffold"))
        elif func == "_registered_auto_load_tail":
            ret = NS["_registered_auto_load_tail"](mat("content"))
        elif func == "_auto_load_typed_text":
            ret = NS["_auto_load_typed_text"](mat("content"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state = {"scaffolds": list(NS["_AUTO_LOAD_SCAFFOLDS"].keys())}
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ─────────────────────────────────────────────────────────────────────────────
# SHADOW MIRROR: literal transcription of the C tf4_* helpers / entry points.
# Operates on transported values; diffed vs the live drive over every fixture.
# Zero diffs proves the C transcription.
# ─────────────────────────────────────────────────────────────────────────────
PFX = "\x00json:"
AL_PREFIX = '[IMPORTANT: The "'
AL_SUFFIX = '" skill is auto-loaded. Follow its instructions for this session.]'
SKILL_INV_PREFIX = "[IMPORTANT: The user has invoked the "
BUNDLE_MARKER = " skill bundle,"
BUNDLE_USER_INSTR = "\nUser instruction: "
BUNDLE_FIRST_BLOCK = "\n\n[Loaded as part of the "
SINGLE_MARKER = "The full skill content is loaded below.]"
SINGLE_INSTR = "The user has provided the following instruction alongside the skill invocation: "
RUNTIME_NOTE = "\n\n[Runtime note:"
DIR_NOTE_END = "then run them with the terminal tool using the absolute path."
EXCERPT_JOINT = "\x1e"
TRAILING_BLOCK_ENDS = (
    ("[Skill config (", "\n]"),
    ("[Skill setup note: ", "]\n\n"),
    ("[This skill has supporting files", "/scripts/foo.js`)."),
)
MAX_SCAFFOLDS = 32


def _strip(s):
    return s.strip() if isinstance(s, str) else None


def _lstrip(s):
    return s.lstrip() if isinstance(s, str) else None


def _wsjoin(s):
    return " ".join(s.split()) if isinstance(s, str) else None


def _startswith(s, p):
    return isinstance(s, str) and s.startswith(p)


def _find(s, p):
    return s.find(p) if isinstance(s, str) else -1


def _rfind(s, p):
    return s.rfind(p) if isinstance(s, str) else -1


def _contains(s, p):
    return isinstance(s, str) and p in s


# -- auto-load header/name recognition (hand-ported fixed regexes) --
def _al_header_match(s):
    if not _startswith(s, AL_PREFIX):
        return False
    j = s.find('"', len(AL_PREFIX))
    return j >= 0 and s[j:].startswith(AL_SUFFIX)


def _al_name(s):
    if not _startswith(s, AL_PREFIX):
        return None
    i = len(AL_PREFIX)
    j = s.find('"', i)
    return None if j < 0 else s[i:j]


def _skill_name(s, prefix):
    if not _startswith(s, prefix):
        return None
    i = len(prefix)
    if i >= len(s) or s[i] != '"':
        return None
    j = s.find('"', i + 1)
    return None if j < 0 else s[i + 1:j]


# -- registry (mirrors C tf4_reg_*) --
def _reg_tail(scaffolds, content):
    best = None
    if isinstance(content, str):
        for sc in scaffolds:
            if content.startswith(sc):
                if best is None or len(sc) > len(best):
                    best = sc
    return None if best is None else content[len(best):]


def _reg_register(scaffolds, scaffold):
    if not isinstance(scaffold, str) or scaffold == "":
        return scaffolds
    out = list(scaffolds)
    if scaffold in out:
        out.remove(scaffold)
    out.append(scaffold)
    while len(out) > MAX_SCAFFOLDS:
        out.pop(0)
    return out


def _auto_load_typed_text(scaffolds, content):
    if not isinstance(content, str):
        return ""
    registered = _reg_tail(scaffolds, content)
    if registered is not None:
        return registered.strip()
    fe = content.rfind(DIR_NOTE_END)
    if fe == -1:
        return ""
    tail = content[fe + len(DIR_NOTE_END):].lstrip()
    while True:
        block_end = None
        for start, end in TRAILING_BLOCK_ENDS:
            if tail.startswith(start):
                block_end = end
                break
        if block_end is None:
            break
        cut = tail.find(block_end)
        tail = tail[cut + len(block_end):].lstrip() if cut >= 0 else ""
    return tail.strip()


def _cut_after_impl(message, marker, stop, rfind):
    if not isinstance(message, str):
        return None
    idx = message.rfind(marker) if rfind else message.find(marker)
    if idx < 0:
        return None
    seg = message[idx + len(marker):]
    head = seg.split(stop, 1)[0]
    return head.strip() or None


def _extract_instruction(scaffolds, content):
    if not isinstance(content, str):
        return None
    if _al_header_match(content):
        return _auto_load_typed_text(scaffolds, content) or None
    if not content.startswith(SKILL_INV_PREFIX):
        return content
    if BUNDLE_MARKER in content:
        return _cut_after_impl(content, BUNDLE_USER_INSTR, BUNDLE_FIRST_BLOCK, False)
    if SINGLE_MARKER in content:
        return _cut_after_impl(content, SINGLE_INSTR, RUNTIME_NOTE, True)
    return None


def _describe_auto(scaffolds, content):
    if not isinstance(content, str) or not content.startswith(AL_PREFIX):
        return None
    name = _al_name(content) or ""
    tail = _auto_load_typed_text(scaffolds, content)
    if not tail:
        return ("/" + name) if name else None
    return " ".join(tail.split())


def _describe_skill(scaffolds, content, separator):
    if not isinstance(content, str):
        return None
    if content.startswith(AL_PREFIX):
        return _describe_auto(scaffolds, content)
    if not content.startswith(SKILL_INV_PREFIX):
        return None
    nm = _skill_name(content, SKILL_INV_PREFIX) or ""
    name = nm.strip()
    label = name if name.startswith("/") else "/" + name
    instruction = _extract_instruction(scaffolds, content)
    if instruction and instruction is not content:
        instruction = " ".join(instruction.split(EXCERPT_JOINT)[0].split())
        if instruction:
            return f"{label}{separator}{instruction}" if name else instruction
    return label if name else None


def shadow(func, args, scaffolds):
    sc = list(scaffolds)
    g = args.get
    if func == "decode_content":
        content = g("content")
        ret = content
        if isinstance(content, str) and content.startswith(PFX):
            raw = content[len(PFX):]
            try:
                ret = json.loads(raw)
            except Exception:
                ret = content
        return ret, sc
    if func == "extract_user_instruction_from_skill_message":
        return _extract_instruction(sc, g("content")), sc
    if func == "_describe_auto_loaded_skill_turn":
        return _describe_auto(sc, g("content")), sc
    if func == "describe_skill_invocation":
        return _describe_skill(sc, g("content"), g("separator", " \u2014 ")), sc
    if func == "_cut_after":
        return _cut_after_impl(g("message"), g("marker"), g("stop_marker"),
                               g("find", "find") == "rfind"), sc
    if func == "register_auto_load_scaffold":
        sc2 = _reg_register(sc, g("scaffold"))
        return None, sc2
    if func == "_registered_auto_load_tail":
        return _reg_tail(sc, g("content")), sc
    if func == "_auto_load_typed_text":
        return _auto_load_typed_text(sc, g("content")), sc
    raise RuntimeError(f"unknown shadow func {func}")


# ── fixture / cross-check ──
def fixpath():
    return _REPO / "tests" / "fixtures" / "turns_flow4.jsonl"


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
    csrc = open(_REPO / "src" / "session_rules" / "turns_flow4.c",
                encoding="utf-8").read()
    pop_missing = [k for k in KINDS if f"tf4_{k.lstrip('_')}" not in csrc]
    if pop_missing:
        print(f"FATAL oracle_turns_flow4: PoP funcs absent from C port: {pop_missing}")
        sys.exit(2)
    bad = audit_exceptions()
    if bad:
        print(f"FATAL oracle_turns_flow4: exception hierarchy drift: {bad[:8]}")
        sys.exit(2)
    fix = fixpath()
    if fix.exists():
        counts, bad_io = {}, set()
        for case in each_fixture():
            f = case.get("func", "")
            counts[f] = counts.get(f, 0) + 1
            bad_io |= set((case.get("in") or {}).get("io") or {})
        if bad_io - set(SEAM_NAMES):
            print(f"FATAL oracle_turns_flow4: fixture io keys without a seam: {sorted(bad_io)}")
            sys.exit(2)
        unknown = set(counts) - set(KINDS)
        if unknown:
            print(f"FATAL oracle_turns_flow4: fixture funcs without a drive: {sorted(unknown)}")
            sys.exit(2)
        untested = set(KINDS) - set(counts)
        if untested:
            print(f"FATAL oracle_turns_flow4: drive funcs without a fixture case: {sorted(untested)}")
            sys.exit(2)
        thin = {f: c for f, c in counts.items() if c < 5}
        if thin:
            print(f"FATAL oracle_turns_flow4: funcs with <5 fixture cases: {thin}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        gg = gt[i] if i < len(gt) else None
        if w != gg:
            return i, w, gg
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, gg = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(gg, ensure_ascii=False)[:400]}")
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
        gg = canon(got)
        if gg != want:
            m += 1
            report_mismatch(func, want, gg, shown)
            shown += 1
    print(f"ORACLE turns_flow4: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    n = bad = 0
    counts = {}
    for case in each_fixture():
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
    print(f"SELFTEST turns_flow4: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


def shadow_test():
    n = bad = 0
    for case in each_fixture():
        n += 1
        func = case["func"]
        inp = case.get("in") or {}
        args = inp.get("args") or {}
        want = drive(func, inp)
        ret, sc2 = shadow(func, copy.deepcopy(args), _scaffolds_from(inp))
        got = canon({"ret": ret, "trace": [], "state": {"scaffolds": sc2}})
        if got != want:
            bad += 1
            print(f"SHADOW DIFF {func} case {n}:")
            print(f"  live={json.dumps(want, ensure_ascii=False)[:400]}")
            print(f"  shad={json.dumps(got, ensure_ascii=False)[:400]}")
    print(f"SHADOW turns_flow4: {n} cases, {bad} shadow diffs")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    if len(sys.argv) > 1 and sys.argv[1] == "--shadow-test":
        sys.exit(shadow_test())
    main()
