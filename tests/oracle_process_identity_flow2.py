#!/usr/bin/env python3
"""Oracle for process_identity_flow2.c — hermes_cli/process_identity.py UNCLAIMED
remainder (LIVE authority: the quarry hermes_cli/process_identity.py).

Drives the LIVE defs with SCRIPTED I/O. The seventeen ported functions (boot_start,
_process_create_time, _live_boot_id, _entry_boot, _verified_boot_start,
_same_incarnation, spawned_by_this_process, _read_ledger_or_quarantine,
is_desktop_owned_backend, local_desktop_client, _desktop_spawner_identity, _new_entry,
entry_still_running, is_owner_death_supervisor, _terminate_then_kill,
_reparented_orphan, reap_orphaned_backend_owner) are AST-extracted from the quarry
(no heavy hermes_cli.process_identity import). The CLAIMED siblings (process_birth,
parse_spawn_tag, _read_ledger, _pid_alive_matches, install_id, ledger_entries,
spawner_is_dead) and the foreign-module imports (psutil, hermes_cli._startup_fast,
hermes_cli.dashboard_procs, time) are replaced by recording stubs returning the SAME
scripted results the C harness feeds its universal seam. Internal composition
(_verified_boot_start -> _same_incarnation -> boot_start; _new_entry ->
_verified_boot_start; entry_still_running -> _entry_boot; reap -> _reparented_orphan /
_terminate_then_kill / _entry_boot) runs REAL on both sides. _live_boot_id is SEAMED in
the composers and ported standalone (its lru_cache(1) is caller-held state). Compares
the ordered DECISION SEQUENCE + return value (+ memo state) with ordered-dict
comparison (insertion order is semantic). Per-case BaseException catch (SystemExit /
KeyboardInterrupt / GeneratorExit escape every except Exception / OSError / named
family and are compared as raise specs; scripted raises round-trip bare).

Input: stdin JSON lines from t_port_process_identity_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...](, "state":...)}}
Summary: ORACLE process_identity_flow2: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam: <name>"};
a seams[name] ARRAY is a per-call queue (last value repeats, empty array -> None); a
single value is a constant; a scripted null is None (NULL in C);
{"__raise__": cls, "msg": ...} scripts an exception.

Fixture domain (kept identical on both sides): ASCII text; POSIX lexical paths;
integral JSON numbers read as int (integral floats == ints); proc doubles are dicts or
null; create_time values are numbers or null; project_root is a POSIX string or null;
the in-function imports resolve silently (import failure outside the domain); psutil
kinds raise the stub-module class so except binds by identity; _live_boot_id fixtures
start with an empty memo and "calls" drives the lru_cache hit; kill_fn presence is a
truthy marker (true -> callable, null -> None).
"""
import ast
import builtins
import contextvars
import json
import os
import sys
import threading
import types
from dataclasses import asdict, dataclass, field
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env runner: derive home from __file__ (repo is parent.parent).
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "pi2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "process_identity.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __import__("__future__").annotations.compiler_flag

# func key -> live name.
PY = {
    "boot_start": "boot_start",
    "process_create_time": "_process_create_time",
    "live_boot_id": "_live_boot_id",
    "entry_boot": "_entry_boot",
    "verified_boot_start": "_verified_boot_start",
    "same_incarnation": "_same_incarnation",
    "spawned_by_this_process": "spawned_by_this_process",
    "read_ledger_or_quarantine": "_read_ledger_or_quarantine",
    "is_desktop_owned_backend": "is_desktop_owned_backend",
    "local_desktop_client": "local_desktop_client",
    "desktop_spawner_identity": "_desktop_spawner_identity",
    "new_entry": "_new_entry",
    "entry_still_running": "entry_still_running",
    "is_owner_death_supervisor": "is_owner_death_supervisor",
    "terminate_then_kill": "_terminate_then_kill",
    "reparented_orphan": "_reparented_orphan",
    "reap_orphaned_backend_owner": "reap_orphaned_backend_owner",
}
ALL_FUNCS = set(PY)
TOP_DEFS = list(PY.values())
CLASSES = ["LedgerEntry", "SpawnTag"]

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}
EXC_CLASSES = {}
REAL = {}

# ── psutil exception family (real classes; except binds by identity) ──
PsError = type("Error", (Exception,), {})
PsNoSuchProcess = type("NoSuchProcess", (PsError,), {})
PsAccessDenied = type("AccessDenied", (PsError,), {})
PsZombieProcess = type("ZombieProcess", (PsError,), {})
PsTimeoutExpired = type("TimeoutExpired", (PsError,), {})
for _c in (PsError, PsNoSuchProcess, PsAccessDenied, PsZombieProcess, PsTimeoutExpired):
    EXC_CLASSES[_c.__name__] = _c


# ── exception construction ──

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
        # FAIL-CLOSED: missing script -> RuntimeError raise spec (presence test,
        # NOT .get() is None — a scripted null is a valid None result).
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
    if isinstance(o, PathDouble):
        return o.as_posix()
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, types.SimpleNamespace) and hasattr(o, "_spec"):
        return norm(o._spec)
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


def pathstr(p):
    if isinstance(p, PathDouble):
        return p.as_posix()
    if p is None:
        return None
    return str(p)


# ── doubles ──

class PathDouble:
    def __init__(self, p):
        if isinstance(p, PathDouble):
            s = p.s
        elif p is None:
            s = ""
        else:
            s = str(p)
        s = s.replace("\\", "/")
        while len(s) > 1 and s.endswith("/"):
            s = s[:-1]
        self.s = s

    def as_posix(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __str__(self):
        return self.s

    @property
    def name(self):
        return self.s.rsplit("/", 1)[-1]

    @property
    def suffix(self):
        name = self.name
        dot = name.rfind(".")
        if dot <= 0:
            return ""
        return name[dot:]

    def with_suffix(self, new):
        suf = self.suffix
        stem = self.s[:len(self.s) - len(suf)] if suf else self.s
        return PathDouble(stem + new)

    def with_name(self, name):
        parent = self.s.rsplit("/", 1)[0] if "/" in self.s else ""
        return PathDouble((parent + "/" + name) if parent else name)

    def read_text(self, encoding=None):
        return seam("path.read_text", {"path": self.s, "encoding": encoding})


class EnvironDouble:
    def __init__(self, d):
        self._d = d or {}

    def get(self, k, default=None):
        return self._d.get(k, default)


class OsDouble:
    def __init__(self, env):
        self.environ = EnvironDouble(env)
        self.name = "posix"

    def getpid(self):
        return seam("os.getpid", {})

    def replace(self, src, dst):
        return seam("os.replace", {"src": pathstr(src), "dst": pathstr(dst)})


class TimeDouble:
    def time(self):
        return seam("time.time", {})

    def monotonic(self):
        return seam("time.monotonic", {})

    def sleep(self, s):
        return seam("time.sleep", {"s": s})


class LoggerDouble:
    def warning(self, fmt, *args, **kw):
        return seam("logger.warning", {"fmt": fmt, "args": list(args), **kw})

    def debug(self, fmt, *args, **kw):
        return seam("logger.debug", {"fmt": fmt, "args": list(args), **kw})

    def info(self, fmt, *args, **kw):
        return seam("logger.info", {"fmt": fmt, "args": list(args), **kw})


def make_proc(spec):
    if spec is None:
        return None
    if not isinstance(spec, dict):
        return spec
    p = types.SimpleNamespace()
    p._spec = spec
    p.create_time = lambda: seam("proc.create_time", {})
    p.terminate = lambda: seam("proc.terminate", {})
    p.wait = lambda timeout=None: seam("proc.wait", {"timeout": timeout})
    p.kill = lambda: seam("proc.kill", {})
    p.environ = lambda: seam("proc.environ", {})
    if "_proc" in spec:
        pp = spec["_proc"]
        if pp is None:
            p._proc = None
        else:
            plat = types.SimpleNamespace()
            plat.create_time = lambda monotonic=False: seam(
                "proc._proc.create_time", {"monotonic": monotonic})
            p._proc = plat
    return p


def proc_spec(proc):
    if isinstance(proc, types.SimpleNamespace) and hasattr(proc, "_spec"):
        return json.loads(json.dumps(proc._spec))
    return proc


# ── extraction ──

def extract():
    mod = types.ModuleType("process_identity_flow2_extracted")
    sys.modules["process_identity_flow2_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "process_identity_flow2_extracted",
        "__file__": SRC,
        "json": json, "os": OsDouble({}), "sys": types.SimpleNamespace(argv=[]),
        "threading": threading, "contextvars": contextvars,
        "Path": PathDouble, "dataclass": dataclass, "field": field,
        "Optional": object, "Sequence": object, "Iterator": object,
        "functools": __import__("functools"),
        "platform": types.SimpleNamespace(system=lambda: "Windows"),
        "time": TimeDouble(),
        "logger": LoggerDouble(),
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
        if isinstance(node, ast.ClassDef) and node.name in CLASSES:
            exec_node(node)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_DEFS:
            exec_node(node)
    seed_consts()
    # Re-set the logger AFTER constant seeding (the module's
    # `logger = logging.getLogger(__name__)` Assign would replace the double).
    ns["logger"] = LoggerDouble()

    missing = [n for n in TOP_DEFS + CLASSES if n not in ns]
    if missing:
        print("ORACLE process_identity_flow2: FATAL missing defs: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    for k in ("SPAWN_ENV_VAR", "_TAG_VERSION", "_EARLIER_BOOT", "_BACKEND_OWNER_PURPOSES"):
        if k not in ns:
            print("ORACLE process_identity_flow2: FATAL missing constant: %s" % k,
                  file=sys.stderr)
            sys.exit(2)
    for nm in TOP_DEFS:
        REAL[nm] = ns[nm]
    return ns


# ── per-case stub installation ──

def install_stubs(ns, inp, func):
    # CLAIMED siblings (seamed, never re-ported).
    ns["process_birth"] = lambda proc: seam("process_birth", {"proc": proc_spec(proc)})

    def pst(raw):
        r = seam("parse_spawn_tag", {"raw": raw})
        if r is None:
            return None
        return types.SimpleNamespace(**r)  # tag.spawner_pid / .spawner_create attribute reads
    ns["parse_spawn_tag"] = pst
    ns["_read_ledger"] = lambda path: seam("_read_ledger", {"path": pathstr(path)})

    def pam(pid, create_time, *, strict=False, boot=None):
        return seam("_pid_alive_matches",
                    {"pid": pid, "create_time": create_time, "boot": boot})
    ns["_pid_alive_matches"] = pam
    ns["install_id"] = lambda project_root=None: seam(
        "install_id", {"project_root": project_root})

    def le(*, project_root=None, all_installs=False, verified_only=False):
        return seam("ledger_entries", {})
    ns["ledger_entries"] = le
    ns["spawner_is_dead"] = lambda entry: seam("spawner_is_dead", {"entry": entry})

    # _live_boot_id: seamed in composers, REAL (cache cleared) standalone.
    if func == "live_boot_id":
        ns["_live_boot_id"] = REAL["_live_boot_id"]
        ns["_live_boot_id"].cache_clear()
    else:
        ns["_live_boot_id"] = lambda: seam("live_boot_id", {})

    # os / sys / time / logger doubles (env + argv are per-case data).
    ns["os"] = OsDouble(inp.get("env"))
    ns["sys"] = types.SimpleNamespace(argv=inp.get("sys_argv") or [])
    ns["logger"] = LoggerDouble()
    td = TimeDouble()
    ns["time"] = td
    sys.modules["time"] = td

    # psutil stub module (flags per case; Process seam returns a proc double).
    ps = types.ModuleType("psutil")
    flags = inp.get("psutil") or {}
    ps.LINUX = bool(flags.get("LINUX", False))
    ps.MACOS = bool(flags.get("MACOS", False))
    ps.Error = PsError
    ps.NoSuchProcess = PsNoSuchProcess
    ps.AccessDenied = PsAccessDenied
    ps.ZombieProcess = PsZombieProcess
    ps.TimeoutExpired = PsTimeoutExpired
    ps.STATUS_ZOMBIE = "zombie"

    def ps_process(pid):
        r = seam("psutil.Process", {"pid": pid})
        return make_proc(r)
    ps.Process = ps_process
    sys.modules["psutil"] = ps

    # hermes_cli package + foreign submodules (every intermediate package).
    hc = types.ModuleType("hermes_cli")
    hc.__path__ = []
    sf = types.ModuleType("hermes_cli._startup_fast")
    sf.is_desktop_ssh_backend_argv = lambda argv: seam(
        "is_desktop_ssh_backend_argv", {"argv": argv})
    dp = types.ModuleType("hermes_cli.dashboard_procs")
    dp._REAP_MIN_AGE_SECONDS = inp.get("reap_min_age", 180)
    dp._process_ppid = lambda pid: seam("_process_ppid", {"pid": pid})
    dp._lock_owned_serve_pids = lambda: seam("_lock_owned_serve_pids", {})
    hc._startup_fast = sf
    hc.dashboard_procs = dp
    sys.modules["hermes_cli"] = hc
    sys.modules["hermes_cli._startup_fast"] = sf
    sys.modules["hermes_cli.dashboard_procs"] = dp

    # _SUPERVISOR_SCRIPT for is_owner_death_supervisor (install-dependent constant).
    ns["_SUPERVISOR_SCRIPT"] = PathDouble(inp.get("script_path", ""))


# ── drive ──

def drive(ns, func, inp):
    try:
        if func == "boot_start":
            ret = ns["boot_start"](make_proc(inp.get("proc")))
        elif func == "process_create_time":
            ret = ns["_process_create_time"](inp.get("pid"))
        elif func == "live_boot_id":
            calls = inp.get("calls", 1)
            ret = None
            for _ in range(max(1, calls)):
                ret = ns["_live_boot_id"]()
            return ret  # state handled in main
        elif func == "entry_boot":
            ret = ns["_entry_boot"](inp.get("entry"), inp.get("key"))
        elif func == "verified_boot_start":
            ret = ns["_verified_boot_start"](inp.get("pid"), inp.get("create_time"))
        elif func == "same_incarnation":
            ret = ns["_same_incarnation"](make_proc(inp.get("proc")),
                                          inp.get("create_time"), inp.get("boot"))
        elif func == "spawned_by_this_process":
            ret = ns["spawned_by_this_process"](inp.get("pid"))
        elif func == "read_ledger_or_quarantine":
            ret = ns["_read_ledger_or_quarantine"](PathDouble(inp.get("path")))
        elif func == "is_desktop_owned_backend":
            ret = ns["is_desktop_owned_backend"](inp.get("argv"))
        elif func == "local_desktop_client":
            ret = ns["local_desktop_client"](inp.get("identity"), inp.get("declared"))
        elif func == "desktop_spawner_identity":
            ret = ns["_desktop_spawner_identity"]()
        elif func == "new_entry":
            e = ns["_new_entry"](inp.get("pid"), inp.get("create_time"),
                                 inp.get("purpose"), inp.get("project_root"),
                                 inp.get("spawner_pid"), inp.get("spawner_create"))
            ret = asdict(e)
        elif func == "entry_still_running":
            ret = ns["entry_still_running"](inp.get("entry"))
        elif func == "is_owner_death_supervisor":
            ret = ns["is_owner_death_supervisor"](inp.get("argv"))
        elif func == "terminate_then_kill":
            ret = ns["_terminate_then_kill"](inp.get("pid"), inp.get("create_time"),
                                             inp.get("boot"))
        elif func == "reparented_orphan":
            ret = ns["_reparented_orphan"](inp.get("pid"))
        elif func == "reap_orphaned_backend_owner":
            kf = (lambda pid: seam("kill_fn", {"pid": pid})) if inp.get("kill_fn") else None
            ret = ns["reap_orphaned_backend_owner"](inp.get("pid"), inp.get("create_time"),
                                                    kill_fn=kf)
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
        if func == "live_boot_id":
            # memo state mirrors the C caller-held lru_cache(1).
            if isinstance(ret, dict) and "__raise__" in ret:
                want["state"] = {"boot_id_cached": False}
            else:
                want["state"] = {"boot_id_cached": True, "boot_id_value": norm(ret)}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE process_identity_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE process_identity_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()