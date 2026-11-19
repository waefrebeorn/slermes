#!/usr/bin/env python3
"""Oracle for curator_flow.c — the UNCLAIMED hermes_cli/curator.py FLOW cores +
leaves (LIVE authority): the timestamp-parse leaf (_parse_ts), the confirm
prompt (_confirm), the skill-row / config print leaves (_print_skill_rows,
_print_curator_config), the pause/pin/lock setters (_set_paused, _set_pin,
_set_lock, _cmd_lock, _cmd_unlock), the ledger-actor wrapper (_as_user), the
ledger / purge / rollback orchestrators (_cmd_ledger, _cmd_purge,
_rollback_ledger_entry, _report_rollback), the argparse helper (_arg), the
nested closures (_freq, _archived_ts) and the three _USAGE_SORTS key lambdas.

Drives the LIVE defs with SCRIPTED I/O: defs are AST-extracted from the quarry
(no heavy hermes_cli import), module constants seeded (_PIN_MESSAGES,
_USAGE_SORTS), and every claimed sibling (_fmt_ts) plus every foreign
collaborator (agent.curator, tools.skill_usage, tools.skill_ledger,
tools.skill_manager_tool, hermes_cli.config, shutil, time, builtin print/input,
pathlib.Path) replaced by recording stubs returning the SAME scripted results
the C harness feeds its universal seam. Ported-to-ported calls compose REAL on
both sides (_cmd_lock/_cmd_unlock -> _set_lock, _cmd_purge -> _confirm +
_archived_ts, _rollback_ledger_entry -> _confirm + _report_rollback), so their
internal print/input seam events appear in both traces.

Path-valued collaborators materialize as CurPath doubles (POSIX strings; .name
/ __truediv__ are pure, .exists/.is_dir/.iterdir/.stat fire seams).
_parse_iso_timestamp results materialize as DTDouble (.timestamp() is pure).
_parse_ts returns a datetime; both sides project it to .isoformat().

Fail-closed: a missing/empty seam script RETURNS a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Uncaught
raises (incl. the BaseException trio) are caught into a ret spec; the
`except (EOFError, KeyboardInterrupt)` / `except OSError` / try-finally sites
behave exactly as live.

Input: stdin JSON lines from t_port_curator_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE curator_flow: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses defs or zero cases were compared.
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
import __future__
from datetime import datetime, timezone, UTC
from pathlib import Path, PurePath, PurePosixPath
from typing import Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not QUARRY.is_dir():
    print("ORACLE curator_flow: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "curator_flow_home"))

SRC = str(QUARRY / "hermes_cli" / "curator.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __future__.annotations.compiler_flag

# harness dispatch name -> live def name
PY = {
    "parse_ts": "_parse_ts",
    "confirm": "_confirm",
    "print_skill_rows": "_print_skill_rows",
    "print_curator_config": "_print_curator_config",
    "set_paused": "_set_paused",
    "set_pin": "_set_pin",
    "set_lock": "_set_lock",
    "cmd_lock": "_cmd_lock",
    "cmd_unlock": "_cmd_unlock",
    "as_user": "_as_user",
    "cmd_ledger": "_cmd_ledger",
    "cmd_purge": "_cmd_purge",
    "report_rollback": "_report_rollback",
    "rollback_ledger_entry": "_rollback_ledger_entry",
    "arg": "_arg",
    "freq": "_freq",
    "archived_ts": "_archived_ts",
    "usage_key_name": "_USAGE_SORTS.name",
    "usage_key_recent": "_USAGE_SORTS.recent",
    "usage_key_activity": "_USAGE_SORTS.activity",
}
TOP_TARGETS = [
    "_parse_ts", "_confirm", "_print_skill_rows", "_print_curator_config",
    "_set_paused", "_set_pin", "_set_lock", "_cmd_lock", "_cmd_unlock",
    "_as_user", "_cmd_ledger", "_cmd_purge", "_report_rollback",
    "_rollback_ledger_entry", "_arg",
]
# nested closure -> parent def
NESTED = {"_freq": "_cmd_status", "_archived_ts": "_cmd_purge"}

CUR = {"scripts": {}, "idx": {}, "trace": []}


class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}


def make_exc(kind, msg):
    cls = BASE_EXC.get(kind)
    if cls is None:
        b = getattr(builtins, kind, None)
        if isinstance(b, type) and issubclass(b, BaseException):
            cls = b
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    if cls is KeyError:
        return cls(msg)
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


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, datetime):
        return o.isoformat()
    if isinstance(o, CurPath):
        return str(o)
    if isinstance(o, PurePath):
        return o.as_posix()
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
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


# ── Path / datetime doubles ──

class CurPath:
    """POSIX path double: .name / / are pure; exists/is_dir/iterdir/stat seamed."""
    def __init__(self, s):
        self._s = str(s)

    @property
    def name(self):
        return PurePosixPath(self._s).name

    def __truediv__(self, other):
        return CurPath(str(PurePosixPath(self._s) / other))

    def __str__(self):
        return self._s

    def __fspath__(self):
        return self._s

    def __repr__(self):
        return "CurPath(%r)" % self._s

    def __lt__(self, other):
        return self._s < other._s

    def __eq__(self, other):
        return isinstance(other, CurPath) and self._s == other._s

    def __hash__(self):
        return hash(self._s)

    def exists(self):
        return seam("exists", {"path": self._s})

    def is_dir(self):
        return seam("is_dir", {"path": self._s})

    def iterdir(self):
        r = seam("iterdir", {"path": self._s})
        return [CurPath(x) for x in (r or [])]

    def stat(self):
        r = seam("stat", {"path": self._s})
        return types.SimpleNamespace(**(r or {}))


class DTDouble:
    def __init__(self, ts):
        self._ts = ts

    def timestamp(self):
        return self._ts


# ── recording stubs ──

def mk(name, proj):
    def f(*a, **k):
        return seam(name, proj(a, k))
    f.__name__ = name
    return f


def print_rec(*a, **k):
    args = {"args": list(a)}
    if "end" in k:
        args["end"] = k["end"]
    if "sep" in k:
        args["sep"] = k["sep"]
    return seam("print", args)


def input_rec(prompt=""):
    return seam("input", {"prompt": prompt})


def find_skill_rec(skill):
    r = seam("_find_skill", {"skill": skill})
    if isinstance(r, dict) and isinstance(r.get("path"), str):
        return {"path": CurPath(r["path"])}
    return r


def archive_dir_rec():
    r = seam("_archive_dir", {})
    return CurPath(r) if isinstance(r, str) else r


def parse_iso_rec(value):
    r = seam("_parse_iso_timestamp", {"value": value})
    if isinstance(r, dict) and "ts" in r:
        return DTDouble(r["ts"])
    return r


def read_skill_name_rec(path, fallback=None):
    return seam("read_skill_name", {"path": str(path), "fallback": fallback})


def rmtree_rec(path):
    return seam("rmtree", {"path": str(path)})


def capture_before_rec(path, complete_package=None, skill=None):
    return seam("capture_before", {"path": str(path),
                                   "complete_package": complete_package, "skill": skill})


def append_entry_rec(action, skill, before=None, after=None, actor=None, evidence=None):
    return seam("append_entry", {"action": action, "skill": skill, "before": before,
                                 "after": after, "actor": actor, "evidence": evidence})


def cfg_get_rec(config, *keys, default=None):
    return seam("cfg_get", {"config": config, "keys": list(keys), "default": default})


def list_entries_rec(skill=None, limit=None):
    return seam("list_entries", {"skill": skill, "limit": limit})


def fn_rec(skill):
    return seam("fn", {"skill": skill})


def seam_bindings():
    return {
        "print": print_rec,
        "input": input_rec,
        "_fmt_ts": mk("_fmt_ts", lambda a, k: {"ts": a[0] if a else None}),
    }


def install_module_stubs():
    """sys.modules stubs for every in-function import the targets execute."""
    def pkg(name):
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
        return m

    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    agent = pkg("agent")
    ac = mod("agent.curator",
             load_state=mk("load_state", lambda a, k: {}),
             is_enabled=mk("is_enabled", lambda a, k: {}),
             set_paused=mk("set_paused", lambda a, k: {"paused": a[0]}),
             get_interval_hours=mk("get_interval_hours", lambda a, k: {}),
             get_stale_after_days=mk("get_stale_after_days", lambda a, k: {}),
             get_archive_after_days=mk("get_archive_after_days", lambda a, k: {}),
             get_consolidate=mk("get_consolidate", lambda a, k: {}))
    agent.curator = ac

    tools = pkg("tools")
    su = mod("tools.skill_usage",
             is_agent_created=mk("is_agent_created", lambda a, k: {"skill": a[0]}),
             set_pinned=mk("set_pinned", lambda a, k: {"skill": a[0], "pinned": a[1]}),
             is_curator_managed=mk("is_curator_managed", lambda a, k: {"skill": a[0]}),
             set_locked=mk("set_locked", lambda a, k: {"key": a[0], "locked": a[1]}),
             _archive_dir=archive_dir_rec,
             load_usage=mk("load_usage", lambda a, k: {}),
             read_skill_name=read_skill_name_rec,
             _parse_iso_timestamp=parse_iso_rec,
             STATE_ARCHIVED="archived")
    tools.skill_usage = su
    sl = mod("tools.skill_ledger",
             set_ledger_actor=mk("set_ledger_actor", lambda a, k: {"actor": a[0]}),
             reset_ledger_actor=mk("reset_ledger_actor", lambda a, k: {"token": a[0]}),
             compact_ledger=mk("compact_ledger", lambda a, k: {}),
             gc_blobs=mk("gc_blobs", lambda a, k: {}),
             list_entries=list_entries_rec,
             get_entry=mk("get_entry", lambda a, k: {"entry_id": a[0]}),
             rollback_entry=mk("rollback_entry", lambda a, k: {"entry_id": a[0]}),
             capture_before=capture_before_rec,
             append_entry=append_entry_rec)
    tools.skill_ledger = sl
    smt = mod("tools.skill_manager_tool", _find_skill=find_skill_rec)
    tools.skill_manager_tool = smt

    hcli = pkg("hermes_cli")
    cfg = mod("hermes_cli.config",
              cfg_get=cfg_get_rec,
              load_config=mk("load_config", lambda a, k: {}))
    hcli.config = cfg

    mod("shutil", rmtree=rmtree_rec)
    mod("time", time=mk("time", lambda a, k: {}))


# ── extraction ──

def extract():
    ns = {
        "__name__": "hermes_cli.curator_flow_extracted",
        "__builtins__": builtins.__dict__,
        "copy": copy, "os": os, "sys": sys,
        "datetime": datetime, "timezone": timezone, "UTC": UTC,
        "Path": CurPath, "Optional": Optional,
        "logger": RecLogger(),
    }
    ns.update(seam_bindings())
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
    ns["logger"] = RecLogger()

    missing = [n for n in TOP_TARGETS if n not in ns]
    if missing:
        print("ORACLE curator_flow: FATAL missing defs: %s" % sorted(missing), file=sys.stderr)
        sys.exit(2)
    if "_PIN_MESSAGES" not in ns or "_USAGE_SORTS" not in ns:
        print("ORACLE curator_flow: FATAL missing constants", file=sys.stderr)
        sys.exit(2)
    return ns


def extract_nested(parent_name, nested_name, globals_dict):
    tree = ast.parse(SRC_TEXT)
    parent = next(n for n in tree.body
                  if isinstance(n, ast.FunctionDef) and n.name == parent_name)
    node = next(n for n in parent.body
                if isinstance(n, ast.FunctionDef) and n.name == nested_name)
    ns = {"__builtins__": builtins.__dict__, "copy": copy}
    ns.update(globals_dict)
    exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                 flags=ANN_FLAG), ns)
    return ns[nested_name]


# ── drive ──

def drive(ns, func, inp):
    env = inp.get("env") or {}
    sys.modules["tools.skill_usage"].STATE_ARCHIVED = env.get("STATE_ARCHIVED", "archived")
    g = inp.get

    if func == "parse_ts":
        r = ns["_parse_ts"](g("ts"))
        return (r.isoformat() if r is not None else None), None
    if func == "arg":
        flags, kw = ns["_arg"](*(g("flags") or []), **(g("kwargs") or {}))
        return [list(flags), kw], None
    if func == "freq":
        f = extract_nested("_cmd_status", "_freq", {})
        return list(f(g("r") or {})), None
    if func in ("usage_key_name", "usage_key_recent", "usage_key_activity"):
        key = {"usage_key_name": "name", "usage_key_recent": "recent",
               "usage_key_activity": "activity"}[func]
        return ns["_USAGE_SORTS"][key][0](g("r") or {}), None
    if func == "report_rollback":
        return ns["_report_rollback"](g("ok", False), g("msg", "")), None
    if func == "confirm":
        kw = {}
        if "cancel" in inp:
            kw["cancel"] = inp["cancel"]
        if "eof_prefix" in inp:
            kw["eof_prefix"] = inp["eof_prefix"]
        return ns["_confirm"](g("prompt", ""), **kw), None
    if func == "print_skill_rows":
        ns["_print_skill_rows"](g("title", ""), g("rows") or [])
        return None, None
    if func == "print_curator_config":
        ns["_print_curator_config"](sys.modules["agent.curator"])
        return None, None
    if func == "set_paused":
        return ns["_set_paused"](g("paused", False)), None
    if func == "set_pin":
        args = types.SimpleNamespace(skill=g("skill", ""))
        return ns["_set_pin"](args, g("pinned", True)), None
    if func == "set_lock":
        args = types.SimpleNamespace(skill=g("skill", ""))
        return ns["_set_lock"](args, g("locked", True)), None
    if func == "cmd_lock":
        args = types.SimpleNamespace(skill=g("skill", ""))
        return ns["_cmd_lock"](args), None
    if func == "cmd_unlock":
        args = types.SimpleNamespace(skill=g("skill", ""))
        return ns["_cmd_unlock"](args), None
    if func == "as_user":
        return ns["_as_user"](fn_rec, g("skill", "")), None
    if func == "cmd_ledger":
        args = types.SimpleNamespace(**(g("args") or {}))
        if "compact" in inp:
            args.compact = inp["compact"]
        if "skill" in inp:
            args.skill = inp["skill"]
        if "limit" in inp:
            args.limit = inp["limit"]
        return ns["_cmd_ledger"](args), None
    if func == "cmd_purge":
        ad = {}
        for k in ("days", "dry_run", "yes"):
            if k in inp:
                ad[k] = inp[k]
        args = types.SimpleNamespace(**ad)
        return ns["_cmd_purge"](args), None
    if func == "rollback_ledger_entry":
        ad = {}
        if "yes" in inp:
            ad["yes"] = inp["yes"]
        args = types.SimpleNamespace(**ad)
        return ns["_rollback_ledger_entry"](args, g("entry_id", "")), None
    if func == "archived_ts":
        f = extract_nested("_cmd_purge", "_archived_ts",
                           {"usage": g("usage") or {},
                            "skill_usage": sys.modules["tools.skill_usage"]})
        return f(CurPath(g("path", ""))), None
    raise ValueError("unknown func: %s" % func)


def main():
    ns = extract()
    install_module_stubs()
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
        print("ORACLE curator_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE curator_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()