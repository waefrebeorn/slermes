#!/usr/bin/env python3
"""Oracle for the agent/verification_evidence.py UNCLAIMED-gap FLOW port
(src/agent/verif_evidence_flow2.c).

AST-extracts the 17 unclaimed live defs plus 2 re-derived claimed sibling
helpers (_is_temp_script_path / _ad_hoc_script_args, NO PoP claim in the port)
-- no heavy import -- seeds module constants, and replaces every claimed
sibling / stdlib collaborator with a recorder driven by the SAME in.io scripts
the C harness feeds its universal seam. Compares ret + ordered trace + mutated
unattributed state.

Script shapes (both sides): plain value reused; {"__seq__":[...]} per-call
(last repeats, empty -> None); {"__raise__":kind,"msg":text} raises a real
exception; a scripted null -> None; ABSENT key -> fail-closed RuntimeError.
The C port carries the FULL builtin exception hierarchy so the `except
ValueError` / `except (TypeError,ValueError)` / `except Exception` sites match
Python exactly (BaseException trio passes every site).

Input: stdin JSON lines from t_port_verif_evidence_flow2.
Summary: ORACLE verif_evidence_flow2: N cases, M mismatches. Fails on 0 cases.
`--selftest <fixture.jsonl>`: gate-proxy mode -- drive every fixture, emit
{func,in,out} envelopes into main()'s comparison and assert 0 mismatches.
"""
import ast
import __future__
import json
import os
import re
import sys
import types
from dataclasses import dataclass

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"

CUR = {"func": None, "in": {}, "trace": [], "seqpos": {}}


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


# Full builtin exception hierarchy (mirrors ve2_exc_parent in the C port).
_BUILTINS = {
    "BaseException": BaseException, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError,
    "ValueError": ValueError, "UnicodeError": UnicodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError, "TypeError": TypeError,
    "AttributeError": AttributeError, "NameError": NameError,
    "UnboundLocalError": UnboundLocalError, "LookupError": LookupError,
    "IndexError": IndexError, "KeyError": KeyError,
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


# ── containers (mirror the frozen dataclasses; no heavy import) ──────────
@dataclass(frozen=True)
class _ShellSegment:
    tokens: list
    following_operator: object = None


@dataclass(frozen=True)
class VerificationEvidence:
    command: str
    canonical_command: str
    kind: str
    scope: str
    status: str
    exit_code: int
    cwd: str
    root: str
    session_id: str
    output_summary: str = ""


# ── live-def extraction ───────────────────────────────────────────────────
TARGETS = {
    "ledger_enabled": "_ledger_enabled",
    "split_shell_segments": "_split_shell_segments",
    "segment_status": "_segment_status",
    "wrapper_option_width": "_wrapper_option_width",
    "skip_wrapper": "_skip_wrapper",
    "interpreter_family": "_interpreter_family",
    "is_harmless_interpreter_flag": "_is_harmless_interpreter_flag",
    "normalize_tokens": "_normalize_tokens",
    "is_under": "_is_under",
    "is_temp_script_path": "_is_temp_script_path",
    "is_interpreter_token": "_is_interpreter_token",
    "ad_hoc_script_args": "_ad_hoc_script_args",
    "root_for": "_root_for",
    "load_changed_paths": "_load_changed_paths",
    "classify_in_workspace": "_classify_in_workspace",
    "last_unattributed_run": "last_unattributed_run",
    "record_verify_run": "record_verify_run",
    "insert_evidence": "_insert_evidence",
    "newest_workspace_verify": "_newest_workspace_verify",
}
CONSTS = (
    "_AD_HOC_SCRIPT_NAME_PREFIXES", "_INTERPRETERS",
    "_WINDOWS_EXE_SUFFIX_RE", "_INTERPRETER_NAME_RE", "_INTERPRETER_FLAG_RE",
    "_WRAPPER_OPTIONS", "_TIMEOUT_DURATION_RE",
)


def extract():
    path = os.path.join(QUARRY, "agent", "verification_evidence.py")
    src = open(path, encoding="utf-8").read()
    ns = {
        "__name__": "ve2_oracle_extracted", "__file__": path,
        "re": re, "json": json, "Optional": object, "Any": object,
        "_ShellSegment": _ShellSegment, "VerificationEvidence": VerificationEvidence,
    }
    flags = 0
    try:
        flags = __future__.annotations.compiler_flag
    except Exception:
        pass
    tree = ast.parse(src)
    # seed module constants (Assign + AnnAssign)
    for node in tree.body:
        names = []
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            names = [node.target.id]
        if not any(n in CONSTS for n in names):
            continue
        seg = ast.get_source_segment(src, node)
        try:
            exec(compile(seg, path, "exec"), {"re": re, "frozenset": frozenset, "json": json}, ns)
        except Exception:
            pass
    missing = [c for c in CONSTS if c not in ns]
    if missing:
        raise RuntimeError("extraction missing constants: %s" % missing)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in set(TARGETS.values()):
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
    missing = [n for n in set(TARGETS.values()) if n not in ns]
    if missing:
        raise RuntimeError("extraction missing funcs: %s" % missing)
    return ns


NS = extract()


# ── seam doubles ──────────────────────────────────────────────────────────
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


class PathDouble:
    def __init__(self, p):
        self.p = str(p)

    def __str__(self):
        return self.p

    def __eq__(self, other):
        if isinstance(other, PathDouble):
            return self.p == other.p
        return NotImplemented

    def __hash__(self):
        return hash(self.p)

    @property
    def parents(self):
        parts = self.p.rstrip('/').split('/')
        return [PathDouble('/'.join(parts[:i]) or '/') for i in range(1, len(parts))]

    def expanduser(self):
        return self  # fixture domain has no '~'

    @property
    def name(self):
        return materialize(script("path.name", {"path": self.p}))

    def is_absolute(self):
        return bool(materialize(script("path.is_absolute", {"path": self.p})))

    def resolve(self):
        return PathDouble(materialize(script("path.resolve", {"path": self.p})))


class _Cur:
    def __init__(self, v):
        self.v = v
        self.lastrowid = v.get("lastrowid") if isinstance(v, dict) else None

    def fetchone(self):
        return self.v


def _sql_tag(sql):
    s = " ".join((sql or "").split())
    if "INSERT INTO verification_events" in s:
        return "insert_event"
    if "INSERT INTO verification_state" in s:
        return "upsert_state"
    if "UPDATE verification_state" in s:
        return "update_other_sessions"
    if "AND created_at >=" in s:
        return "select_newest_since"
    if "SELECT * FROM verification_events" in s:
        return "select_newest"
    return "execute"


class _Conn:
    def execute(self, sql, params=()):
        return _Cur(materialize(script("db.execute", {
            "sql": _sql_tag(sql), "params": list(params)})))

    def commit(self):
        return materialize(script("db.commit", {}))


class _Tx:
    def __enter__(self):
        materialize(script("transaction", {}))
        return _Conn()

    def __exit__(self, *a):
        return False


import contextlib

NS["Path"] = PathDouble
NS["json"] = types.SimpleNamespace(
    loads=lambda s: materialize(script("json.loads", {"s": s})))
NS["_DB_LOCK"] = contextlib.nullcontext()
NS["_transaction"] = lambda: _Tx()
NS["shlex"] = types.SimpleNamespace(
    split=lambda s, posix=True: materialize(script(
        "shlex.split", {"raw": s, "posix": posix})))
NS["tempfile"] = types.SimpleNamespace(
    gettempdir=lambda: materialize(script("tempfile.gettempdir", {})))

# claimed siblings -> universal seams (arg names match the C args objects)
NS["_strip_command_prefix"] = seamfn(
    "strip_command_prefix", lambda tokens: {"tokens": tokens})
NS["_find_canonical_match"] = seamfn(
    "find_canonical_match",
    lambda command, canonical, exit_code, pipeline: {
        "command": command, "canonical_commands": canonical,
        "exit_code": exit_code, "pipeline_status": pipeline})
NS["_kind_for_command"] = seamfn(
    "kind_for_command", lambda canonical: {"canonical": canonical})
NS["_looks_like_target"] = seamfn(
    "looks_like_target", lambda arg: {"arg": arg})
NS["_find_ad_hoc_match"] = seamfn(
    "find_ad_hoc_match",
    lambda command, root, exit_code: {
        "command": command, "root": root, "exit_code": exit_code})
NS["_summarize_output"] = seamfn(
    "summarize_output", lambda output: {"output": output})
NS["_project_facts"] = seamfn("project_facts", lambda root: {"root": root})
NS["_utc_now"] = seamfn("utc_now", lambda: {})
NS["_prune_old_events"] = seamfn(
    "prune_old_events", lambda conn, session_id, root: {
        "session_id": session_id, "root": root})

# function-level imports in _ledger_enabled -> fake modules, no trace event
pkg("agent")
pkg("tools")
agent_stop = pkg("agent.verification_stop")
agent_stop.verify_on_stop_enabled = seamfn("verify_on_stop_enabled", lambda: {})
hosted = pkg("tools.hosted_tool_policy")
hosted.host_feature_enabled = seamfn(
    "host_feature_enabled", lambda name: {"feature": name})


# ── drive ─────────────────────────────────────────────────────────────────
def to_jsonable(x):
    if isinstance(x, (VerificationEvidence, _ShellSegment)):
        return {k: to_jsonable(v) for k, v in x.__dict__.items()}
    if isinstance(x, dict):
        return {k: to_jsonable(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [to_jsonable(v) for v in x]
    return x


def run_flow(fn):
    try:
        ret = fn()
    except BaseException as e:
        ret = raise_spec(e)
    unattr = [{"session_id": k[0], "root": k[1], "command": v[0], "at": v[1]}
              for k, v in NS["_UNATTRIBUTED_RUNS"].items()]
    return {"ret": to_jsonable(ret), "trace": CUR["trace"],
            "state": {"unattributed": unattr}}


def g(inp, k, d=None):
    return inp.get(k, d)


def drive(func, inp):
    f = NS[TARGETS[func]]
    NS["_UNATTRIBUTED_RUNS"] = {
        (r.get("session_id", ""), r.get("root", "")):
            (r.get("command", ""), r.get("at", ""))
        for r in (inp.get("unattributed") or [])
    }
    if func == "ledger_enabled":
        return run_flow(lambda: f())
    if func == "split_shell_segments":
        return run_flow(lambda: f(g(inp, "command", ""), posix=g(inp, "posix", True)))
    if func == "segment_status":
        segs = [_ShellSegment(tokens=s.get("tokens", []),
                              following_operator=s.get("following_operator"))
                for s in (g(inp, "segments") or [])]
        return run_flow(lambda: f(segs, g(inp, "match_index", 0),
                                  g(inp, "exit_code", 0), g(inp, "pipeline_status")))
    if func == "wrapper_option_width":
        return run_flow(lambda: f(g(inp, "token", ""), frozenset(g(inp, "flags") or []),
                                  frozenset(g(inp, "valued") or [])))
    if func == "skip_wrapper":
        return run_flow(lambda: f(g(inp, "tokens") or [], g(inp, "i", 0)))
    if func == "interpreter_family":
        return run_flow(lambda: f(g(inp, "token", "")))
    if func == "is_harmless_interpreter_flag":
        return run_flow(lambda: f(g(inp, "token", "")))
    if func == "normalize_tokens":
        return run_flow(lambda: f(g(inp, "tokens") or []))
    if func == "is_under":
        return run_flow(lambda: f(g(inp, "token", ""), g(inp, "base")))
    if func == "is_temp_script_path":
        return run_flow(lambda: f(g(inp, "token", ""), g(inp, "root")))
    if func == "is_interpreter_token":
        return run_flow(lambda: f(g(inp, "token", "")))
    if func == "ad_hoc_script_args":
        return run_flow(lambda: f(g(inp, "tokens") or [], g(inp, "root")))
    if func == "root_for":
        return run_flow(lambda: f(g(inp, "facts"), g(inp, "cwd")))
    if func == "load_changed_paths":
        return run_flow(lambda: f(g(inp, "raw")))
    if func == "classify_in_workspace":
        return run_flow(lambda: f(
            g(inp, "command", ""), g(inp, "facts") or {},
            cwd=g(inp, "cwd"), session_id=g(inp, "session_id"),
            exit_code=g(inp, "exit_code", 0), output=g(inp, "output", ""),
            pipeline_status=g(inp, "pipeline_status")))
    if func == "last_unattributed_run":
        return run_flow(lambda: f(session_id=g(inp, "session_id"),
                                  root=g(inp, "root"), since=g(inp, "since")))
    if func == "insert_evidence":
        fields = {k: g(inp, k) for k in (
            "command", "canonical_command", "kind", "scope", "status",
            "exit_code", "cwd", "root", "session_id", "output_summary")}
        fields["exit_code"] = int(fields["exit_code"] or 0)
        return run_flow(lambda: f(VerificationEvidence(**fields)))
    if func == "record_verify_run":
        return run_flow(lambda: f(
            root=g(inp, "root", ""), session_id=g(inp, "session_id"),
            ok=g(inp, "ok", True), command=g(inp, "command", "hermes verify"),
            scope=g(inp, "scope", "full"), output=g(inp, "output", "")))
    if func == "newest_workspace_verify":
        return run_flow(lambda: f(_Conn(), root=g(inp, "root", ""), since=g(inp, "since")))
    raise ValueError("unknown func: %s" % func)


def canon(x):
    if isinstance(x, BaseException):
        return canon(raise_spec(x))
    if getattr(x, "__sp_name__", None):
        return ("str", x.__sp_name__)
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


def compare(record):
    func = record.get("func")
    inp = norm_nums(record.get("in") or {})
    got = record.get("out") or {}
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["seqpos"] = {}
    try:
        want = drive(func, inp)
    except BaseException as e:
        want = {"ret": raise_spec(e), "trace": CUR["trace"],
                "state": {"unattributed": []}}
    return func, got, want


def check_stream(stream):
    cases = 0
    mism = 0
    for line in stream:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        func, got, want = compare(record)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False, default=str))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE verif_evidence_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


def selftest(path):
    """Gate proxy: drive every fixture, serialize mirror {func,in,out}
    envelopes, and pipe them into the real comparison path."""
    import io
    lines = []
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("{"):
            continue
        case = json.loads(line)
        func, _, want = compare(case)
        lines.append(json.dumps({"func": func, "in": norm_nums(case.get("in") or {}),
                                 "out": want}, ensure_ascii=False, default=str))
    rc = check_stream(io.StringIO("\n".join(lines) + "\n"))
    print("SELFTEST verif_evidence_flow2: shadow-diff %d" % (0 if rc == 0 else -1))
    return rc


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "--selftest":
        return selftest(sys.argv[2])
    return check_stream(sys.stdin)


if __name__ == "__main__":
    sys.exit(main())
