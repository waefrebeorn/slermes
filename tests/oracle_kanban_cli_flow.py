#!/usr/bin/env python3
"""Oracle for kanban_cli_flow.c — the UNCLAIMED hermes_cli/kanban.py FLOW
cores + leaves (LIVE authority): the flag-parsing leaves (_none_profile,
_parse_metadata_flag), the text/args leaves (_joined_words, _stripped_or_none,
_ok_or_err, _bulk_ids, _require_ids), the human-render leaves
(_print_diagnostics, _print_section), the row grouper (_rows_by_task), the
goal-judge gate (_goal_gate_error), the comment-wrapping closure (_commented),
the notify identity leaf (_caller_identity_for_target), the specify/decompose
driver (_run_triage_sweep), the outcome-line leaves (_retitled_suffix,
_decompose_ok_line) and the two verb handlers whose sibling PoP lines credit
the underscore-less names cmd_log / cmd_request_changes: _cmd_log and
_cmd_request_changes.

Drives the LIVE defs with SCRIPTED I/O: defs are AST-extracted from the quarry
(no heavy hermes_cli import), and every claimed sibling (_profile_author,
_worker_run_id_for, _goal_mode_handoff_rejection) plus every foreign
collaborator (kanban_output._err/_obj_dict, kanban_db kb.*, kanban_db_connect
kbc.connect_closing, gateway.session_context.get_session_env, json.loads /
json.dumps, builtin print, sys.stdout.write, the mod/run_one/human_ok/op
callables) is replaced by recording stubs returning the SAME scripted results
the C harness feeds its universal seam. Ported-to-ported calls compose REAL on
both sides (_decompose_ok_line -> _retitled_suffix), so no seam event fires for
the internal call on either side.

Namespace/outcome/diag doubles are SimpleNamespace (missing attributes raise
the py3.14 module-qualified AttributeError the C port models). conn is an
opaque fixture marker; _rows_by_task's conn double fires "conn.execute".
kbc.connect_closing() is ONE seam call returning a silent context manager.
_commented's closure is driven through run(tid) exactly as the C harness drives
kc_commented_run.

Fail-closed: a missing/empty seam script RETURNS a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Uncaught
raises (incl. the BaseException trio) are caught into a ret spec; the
`except (ValueError, json.JSONDecodeError)` site in _parse_metadata_flag
swallows the full ValueError family and nothing else.

Input: stdin JSON lines from t_port_kanban_cli_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE kanban_cli_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
"""
import ast
import builtins
import json
import os
import sys
import types
import __future__
from pathlib import Path
from typing import Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not QUARRY.is_dir():
    print("ORACLE kanban_cli_flow: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "kanban_cli_flow_home"))

SRC = str(QUARRY / "hermes_cli" / "kanban.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __future__.annotations.compiler_flag

# harness dispatch name -> live def name
PY = {
    "none_profile": "_none_profile",
    "parse_metadata_flag": "_parse_metadata_flag",
    "joined_words": "_joined_words",
    "stripped_or_none": "_stripped_or_none",
    "ok_or_err": "_ok_or_err",
    "bulk_ids": "_bulk_ids",
    "require_ids": "_require_ids",
    "print_diagnostics": "_print_diagnostics",
    "print_section": "_print_section",
    "rows_by_task": "_rows_by_task",
    "goal_gate_error": "_goal_gate_error",
    "commented": "_commented",
    "caller_identity_for_target": "_caller_identity_for_target",
    "run_triage_sweep": "_run_triage_sweep",
    "retitled_suffix": "_retitled_suffix",
    "decompose_ok_line": "_decompose_ok_line",
    "cmd_log": "_cmd_log",
    "cmd_request_changes": "_cmd_request_changes",
}
TOP_TARGETS = list(PY.values())

CUR = {"scripts": {}, "idx": {}, "trace": []}

BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
# __str__ reads ctor attributes the bypass does not set -> str(e) == "".
_UNICODE_EXC = (UnicodeDecodeError, UnicodeEncodeError, UnicodeTranslateError)


def make_exc(kind, msg):
    cls = BASE_EXC.get(kind)
    if cls is None:
        b = getattr(builtins, kind, None)
        if isinstance(b, type) and issubclass(b, BaseException):
            cls = b
    if cls is None and kind == "JSONDecodeError":
        cls = json.JSONDecodeError
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    if cls in _UNICODE_EXC or cls is json.JSONDecodeError:
        # object.__new__ is unsafe on py3.14 for __init__-overriding classes;
        # Cls.__new__ + Exception.__init__ keeps str(e) == msg (except the
        # Unicode* trio, whose __str__ ignores args -> "").
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r["__raise__"], r.get("msg", ""))
    return r


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


def ns_to_dict(o):
    if isinstance(o, types.SimpleNamespace):
        return {k: ns_to_dict(v) for k, v in vars(o).items()}
    if isinstance(o, dict):
        return {k: ns_to_dict(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [ns_to_dict(v) for v in o]
    return o


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, types.SimpleNamespace):
        return {str(k): norm(v) for k, v in vars(o).items()}
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False,
                                 default=str))


def ns_double(o):
    """dict -> SimpleNamespace recursively (attribute-access doubles)."""
    if isinstance(o, dict):
        return types.SimpleNamespace(**{k: ns_double(v) for k, v in o.items()})
    if isinstance(o, list):
        return [ns_double(v) for v in o]
    return o


def diag_double(d):
    """Diagnostic double: the diag and its actions are attribute-access
    namespaces, but .data stays a plain dict (live calls d.data.items())."""
    if not isinstance(d, dict):
        return d
    kw = {k: (v if k == "data" else ns_double(v)) for k, v in d.items()}
    return types.SimpleNamespace(**kw)


# ── recording stubs ──

def print_rec(*a, **k):
    args = {"args": list(a)}
    if k.get("file") is sys.stderr:
        args["file"] = "stderr"
    if "end" in k:
        args["end"] = k["end"]
    if "sep" in k:
        args["sep"] = k["sep"]
    return seam("print", args)


def write_rec(text):
    return seam("sys.stdout.write", {"text": text})


def err_rec(msg, rc=1):
    return seam("_err", {"msg": msg, "rc": rc})


def obj_dict_rec(obj, fields):
    return seam("_obj_dict", {"obj": ns_to_dict(obj), "fields": list(fields)})


def json_loads_rec(raw):
    return seam("json.loads", {"raw": raw})


def json_dumps_rec(obj):
    return seam("json.dumps", {"obj": obj})


def profile_author_rec():
    return seam("_profile_author", {})


def worker_run_id_rec(task_id):
    return seam("_worker_run_id_for", {"task_id": task_id})


def handoff_rejection_rec(task, evidence):
    return seam("_goal_mode_handoff_rejection", {"task": task, "evidence": evidence})


def kb_get_task_rec(conn, tid):
    return seam("kb.get_task", {"conn": conn, "tid": tid})


def kb_read_worker_log_rec(task_id, tail_bytes=None):
    return seam("kb.read_worker_log", {"task_id": task_id, "tail_bytes": tail_bytes})


def kb_request_changes_rec(conn, tid, reason=None, expected_run_id=None):
    return seam("kb.request_changes", {"conn": conn, "tid": tid, "reason": reason,
                                       "expected_run_id": expected_run_id})


def kb_add_comment_rec(conn, tid, author, body):
    return seam("kb.add_comment", {"conn": conn, "tid": tid, "author": author,
                                   "body": body})


class ConnCM:
    """Silent connect_closing context manager: __enter__ returns the scripted
    conn marker, __exit__ returns False (propagate). No enter/exit events."""
    def __init__(self, conn):
        self._conn = conn

    def __enter__(self):
        return self._conn

    def __exit__(self, *a):
        return False


def connect_closing_rec():
    return ConnCM(seam("kbc.connect_closing", {}))


class ConnExecDouble:
    """_rows_by_task's conn: .execute fires the seam with {sql, params}."""
    def execute(self, sql, params):
        return seam("conn.execute", {"sql": sql, "params": list(params)})


def get_session_env_rec(key, default=""):
    return seam("get_session_env", {"key": key, "default": default})


def list_triage_ids_rec(tenant=None):
    return seam("mod.list_triage_ids", {"tenant": tenant})


def run_one_rec(tid, author=None):
    r = seam("run_one", {"tid": tid, "author": author})
    # outcome doubles are SimpleNamespace (live reads outcome.ok/.task_id/...);
    # the trace records the RAW script, the flow sees the materialized double.
    return ns_double(r) if isinstance(r, dict) else r


def human_ok_rec(outcome):
    return seam("human_ok", {"outcome": ns_to_dict(outcome)})


def op_rec(tid):
    return seam("op", {"tid": tid})


SYS_DOUBLE = types.SimpleNamespace(
    stdout=types.SimpleNamespace(write=write_rec),
    stderr=sys.stderr,
)
JSON_DOUBLE = types.SimpleNamespace(
    loads=json_loads_rec, dumps=json_dumps_rec,
    JSONDecodeError=json.JSONDecodeError,
)
KB_DOUBLE = types.SimpleNamespace(
    get_task=kb_get_task_rec,
    read_worker_log=kb_read_worker_log_rec,
    request_changes=kb_request_changes_rec,
    add_comment=kb_add_comment_rec,
)
KBC_DOUBLE = types.SimpleNamespace(connect_closing=connect_closing_rec)


def install_module_stubs():
    """sys.modules stubs for every in-function import the targets execute."""
    gw = types.ModuleType("gateway")
    gw.__path__ = []
    sys.modules["gateway"] = gw
    sc = types.ModuleType("gateway.session_context")
    sc.get_session_env = get_session_env_rec
    sys.modules["gateway.session_context"] = sc
    gw.session_context = sc


# ── extraction ──

def extract():
    modname = "hermes_cli.kanban_cli_flow_extracted"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": modname,
        "__builtins__": builtins.__dict__,
        "os": os, "sys": SYS_DOUBLE, "json": JSON_DOUBLE,
        "Optional": Optional,
        "kb": KB_DOUBLE, "kbc": KBC_DOUBLE,
        "print": print_rec,
        "_err": err_rec,
        "_obj_dict": obj_dict_rec,
        "_profile_author": profile_author_rec,
        "_worker_run_id_for": worker_run_id_rec,
        "_goal_mode_handoff_rejection": handoff_rejection_rec,
    })
    tree = ast.parse(SRC_TEXT)

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN_FLAG), ns)

    def seed_consts():
        for node in tree.body:
            if isinstance(node, ast.Assign):
                if not all(isinstance(t, ast.Name) for t in node.targets):
                    continue
            elif isinstance(node, ast.AnnAssign):
                if not isinstance(node.target, ast.Name):
                    continue
            else:
                continue
            try:
                exec_node(node)
            except Exception:
                pass

    seed_consts()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_TARGETS:
            exec_node(node)
    seed_consts()

    missing = [n for n in TOP_TARGETS if n not in ns]
    if missing:
        print("ORACLE kanban_cli_flow: FATAL missing defs: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    return ns


# ── drive ──

def drive(ns, func, inp):
    g = inp.get

    if func == "none_profile":
        return ns["_none_profile"](g("value")), None
    if func == "parse_metadata_flag":
        r = ns["_parse_metadata_flag"](g("raw"))
        return list(r), None
    if func == "joined_words":
        return ns["_joined_words"](g("words")), None
    if func == "stripped_or_none":
        return ns["_stripped_or_none"](g("value")), None
    if func == "ok_or_err":
        return ns["_ok_or_err"](g("ok"), g("fail", ""), g("done", "")), None
    if func == "bulk_ids":
        args = types.SimpleNamespace(**(g("args") or {}))
        return ns["_bulk_ids"](args), None
    if func == "require_ids":
        args = types.SimpleNamespace(**(g("args") or {}))
        r = ns["_require_ids"](args)
        return list(r), None
    if func == "print_diagnostics":
        diags = g("diags")
        if isinstance(diags, list):
            diags = [diag_double(d) for d in diags]
        ns["_print_diagnostics"](diags, g("indent", ""),
                                 with_kind=g("with_kind", False))
        return None, None
    if func == "print_section":
        ns["_print_section"](g("title", ""), g("lines"))
        return None, None
    if func == "rows_by_task":
        return ns["_rows_by_task"](ConnExecDouble(), g("table", ""), g("ids")), None
    if func == "goal_gate_error":
        return ns["_goal_gate_error"](g("conn", "CONN"), g("tid", ""),
                                      g("evidence", ""), g("handoff", ""),
                                      g("blocked_hint", ""), g("continue_hint", "")), None
    if func == "commented":
        run = ns["_commented"](g("conn", "CONN"), g("reason"), g("author"),
                               g("prefix", ""), op_rec)
        tids = g("tids")
        if not tids:
            tids = [g("tid", "")]
        r = None
        for tid in tids:
            r = run(tid)
        return r, None
    if func == "caller_identity_for_target":
        r = ns["_caller_identity_for_target"](g("platform"), g("chat_id"),
                                              g("thread_id"), g("user_id"),
                                              g("user_id_alt"))
        return list(r), None
    if func == "run_triage_sweep":
        args = types.SimpleNamespace(**(g("args") or {}))
        mod = types.SimpleNamespace(list_triage_ids=list_triage_ids_rec)
        return ns["_run_triage_sweep"](args, g("verb", ""), mod, run_one_rec,
                                       g("json_key", ""),
                                       tuple(g("json_fields") or ()), human_ok_rec), None
    if func == "retitled_suffix":
        return ns["_retitled_suffix"](ns_double(g("outcome"))), None
    if func == "decompose_ok_line":
        return ns["_decompose_ok_line"](ns_double(g("outcome"))), None
    if func == "cmd_log":
        args = types.SimpleNamespace(**(g("args") or {}))
        return ns["_cmd_log"](args), None
    if func == "cmd_request_changes":
        args = types.SimpleNamespace(**(g("args") or {}))
        return ns["_cmd_request_changes"](args), None
    raise ValueError("unknown func: %s" % func)


def main():
    ns = extract()
    install_module_stubs()
    REAL = {nm: ns[nm] for nm in TOP_TARGETS}
    cases = 0
    mism = 0
    per_func = {}
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        per_func[func] = per_func.get(func, 0) + 1
        # restore every target def to real before each case (a prior flow may
        # have seamed a name that is also a target elsewhere)
        for nm, fn in REAL.items():
            ns[nm] = fn
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        if func not in PY:
            mism += 1
            print("MISMATCH %s: unknown func (not in PY map)" % func)
            continue
        try:
            ret, state = drive(ns, func, inp)
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
            state = None
        want = {"ret": norm(ret), "trace": norm(CUR["trace"])}
        if state is not None:
            want["state"] = norm(state)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE kanban_cli_flow: 0 cases (fail-closed)")
        sys.exit(2)
    missing_funcs = [f for f in PY if per_func.get(f, 0) < 5]
    if missing_funcs:
        print("ORACLE kanban_cli_flow: WARN funcs with <5 cases: %s"
              % sorted(missing_funcs), file=sys.stderr)
    print("ORACLE kanban_cli_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
