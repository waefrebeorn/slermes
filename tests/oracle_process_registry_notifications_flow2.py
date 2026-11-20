#!/usr/bin/env python3
"""Oracle for process_registry_notifications_flow2.c — tools/process_registry_notifications.py
UNCLAIMED remainder (LIVE authority: the quarry tools/process_registry_notifications.py).

Drives the LIVE defs with SCRIPTED I/O. The thirteen ported functions
(is_child_process_notification, should_surface_notification, _is_truncated,
delegation_wakes_parent, completion_resumes_parent, group_process_notifications,
_short_title, _short_command, process_completion_display_text, _completion_status,
heartbeat_output_tail, heartbeat_output_text, join_heartbeat_outputs) are AST-extracted
from the quarry (no heavy tools.process_registry_notifications import). The one CLAIMED
collaborator (ProcessRegistry._surface_child_process_notifications) is replaced by a
recording stub returning the SAME scripted result the C harness feeds its universal
seam. Internal composition (should_surface_notification -> is_child_process_notification,
process_completion_display_text -> _short_command, join_heartbeat_outputs ->
heartbeat_output_tail) runs REAL on both sides. Compares the ordered DECISION SEQUENCE
(trace) + return value with ordered-dict comparison (insertion order is semantic).
Per-case BaseException catch (SystemExit / KeyboardInterrupt / GeneratorExit escape
every except Exception and propagate; scripted raises round-trip bare via identity).
build_exc constructs the FULL builtin exception hierarchy (subclasses like
FileNotFoundError are real OSError children; SystemExit et al stay off Exception).

Input: stdin JSON lines from t_port_process_registry_notifications_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE process_registry_notifications_flow2: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam: <name>"};
a seams[name] ARRAY is a per-call queue (last value repeats, empty array -> None); a
single value is a constant; a scripted null is None (NULL in C);
{"__raise__": cls, "msg": ...} scripts an exception.

Fixture domain (identical on both sides): ASCII text (len==strlen); integral JSON
numbers are Python ints (written without a decimal point); int() inputs are ASCII
numeric strings or numbers/bools; status/reason values are strings or absent/null;
task_index is an int; whitespace is the ASCII set.
"""
import ast
import builtins
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tools", "process_registry_notifications.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __import__("__future__").annotations.compiler_flag

# func key -> live name
PY = {
    "is_child_process_notification": "is_child_process_notification",
    "should_surface_notification": "should_surface_notification",
    "is_truncated": "_is_truncated",
    "delegation_wakes_parent": "delegation_wakes_parent",
    "completion_resumes_parent": "completion_resumes_parent",
    "group_process_notifications": "group_process_notifications",
    "short_title": "_short_title",
    "short_command": "_short_command",
    "process_completion_display_text": "process_completion_display_text",
    "completion_status": "_completion_status",
    "heartbeat_output_tail": "heartbeat_output_tail",
    "heartbeat_output_text": "heartbeat_output_text",
    "join_heartbeat_outputs": "join_heartbeat_outputs",
}
ALL_FUNCS = set(PY)
TOP_DEFS = list(PY.values())

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}
EXC_CLASSES = {}
REAL = {}


# ── exception construction (FULL builtin hierarchy) ──
def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls in EXC_CLASSES:
        return EXC_CLASSES[cls](msg)
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        if cls in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            e = b.__new__(b)
            Exception.__init__(e, msg)
            return e
        return b(msg)
    return type(cls, (Exception,), {})(msg)


def seam(name, args):
    args = norm({k: v for k, v in args.items() if v is not None})
    scripts = CUR["scripts"]
    if name not in scripts:
        # FAIL-CLOSED: presence test, NOT .get() is None — a scripted null is valid.
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = json.loads(json.dumps(r))  # deep copy: live mutation cannot corrupt script/trace
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


# ── normalization / canonical comparison ──
def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", o)


# ── extraction ──
def extract():
    mod = types.ModuleType("process_registry_notifications_flow2_extracted")
    sys.modules["process_registry_notifications_flow2_extracted"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "process_registry_notifications_flow2_extracted",
               "__file__": SRC, "json": json})
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
                if not (node.value and isinstance(node.target, ast.Name)):
                    continue
            else:
                continue
            try:
                exec_node(node)
            except Exception:
                pass

    seed_consts()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_DEFS:
            exec_node(node)
    seed_consts()
    missing = [n for n in TOP_DEFS if n not in ns]
    if missing:
        print("ORACLE process_registry_notifications_flow2: FATAL missing defs: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    for k in ("_DONE", "_STOPPED", "_REASON_STATUS", "HEARTBEAT_OUTPUT_CHARS"):
        if k not in ns:
            print("ORACLE process_registry_notifications_flow2: FATAL missing constant: %s" % k,
                  file=sys.stderr)
            sys.exit(2)
    for nm in TOP_DEFS:
        REAL[nm] = ns[nm]
    return ns


# ── per-case stub installation ──
def install_stubs(ns, inp, func):
    # CLAIMED sibling, seamed (never re-ported).
    tools = types.ModuleType("tools")
    tools.__path__ = []
    pr = types.ModuleType("tools.process_registry")

    class ProcessRegistry:
        @staticmethod
        def _surface_child_process_notifications():
            return seam("surface_child", {})

    pr.ProcessRegistry = ProcessRegistry
    tools.process_registry = pr
    sys.modules["tools"] = tools
    sys.modules["tools.process_registry"] = pr


# ── drive ──
def drive(ns, func, inp):
    try:
        if func == "is_child_process_notification":
            ret = ns["is_child_process_notification"](inp.get("evt"))
        elif func == "should_surface_notification":
            if "surface_child" in inp:
                ret = ns["should_surface_notification"](inp.get("evt"),
                                                        surface_child=inp["surface_child"])
            else:
                ret = ns["should_surface_notification"](inp.get("evt"))
        elif func == "is_truncated":
            ret = ns["_is_truncated"](inp.get("entry"))
        elif func == "delegation_wakes_parent":
            ret = ns["delegation_wakes_parent"](inp.get("evt"))
        elif func == "completion_resumes_parent":
            ret = ns["completion_resumes_parent"](inp.get("evt"))
        elif func == "group_process_notifications":
            ret = list(ns["group_process_notifications"](inp.get("notifications") or []))
        elif func == "short_title":
            ret = ns["_short_title"](inp.get("title") or "")
        elif func == "short_command":
            ret = ns["_short_command"](inp.get("command"))
        elif func == "process_completion_display_text":
            ret = ns["process_completion_display_text"](inp.get("events") or [])
        elif func == "completion_status":
            ret = ns["_completion_status"](inp.get("evt"))
        elif func == "heartbeat_output_tail":
            omit = inp.get("omitted")
            if omit is None:
                omit = 0
            ret = ns["heartbeat_output_tail"](inp.get("output") or "", omit)
        elif func == "heartbeat_output_text":
            ret = ns["heartbeat_output_text"](inp.get("evt"))
        elif func == "join_heartbeat_outputs":
            ret = ns["join_heartbeat_outputs"](inp.get("earlier") or {}, inp.get("later") or {})
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:  # SystemExit included — fail-closed per case
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec  # scripted raise round-trips bare
                break
        if ret is None:
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret


def main():
    ns = extract()
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
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        if func not in ALL_FUNCS:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        install_stubs(ns, inp, func)
        ret = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE process_registry_notifications_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE process_registry_notifications_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
