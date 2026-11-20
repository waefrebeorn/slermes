#!/usr/bin/env python3
"""Oracle for gateway_multiplex_flow2.c — the UNCLAIMED remainder of the
gateway multiplex flow family (LIVE authority: the quarry
hermes_cli/gateway_multiplex_served.py (7 defs) and
hermes_cli/gateway_runtime_multiplex.py (3 defs)).

Drives the LIVE functions with SCRIPTED I/O: all 10 targets are AST-extracted
from the two quarry modules, and every foreign-module call (hermes_constants,
gateway.status, gateway.control_socket, the claimed
gateway_multiplex_mode.host_serves_named_profile sibling, profiles and
logging) is replaced by a recording stub returning the SAME scripted results
the C harness feeds its universal seam. Internal composition (the ported
targets calling each other) runs REAL on both sides. Compares the ordered
trace + return + state (all targets are state-free -> {}). Per-case
BaseException catch: scripted raise specs round-trip VERBATIM by identity.

Input: stdin JSON lines from t_port_gateway_multiplex_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":...}}
Summary: ORACLE gateway_multiplex_flow2: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.

Fixture domain notes (kept identical on both sides):
- paths are POSIX strings or null; PathDouble implements the SAME PurePosixPath
  model as the C port: split on '/', drop '' and '.' parts, keep '..', preserve
  an exactly-two-slash root ('//h'), collapse 3+ slashes to one;
- root scripts (get_default_hermes_root) are str|null only; home is str|null
  (null home -> Path(None) TypeError on both sides);
- str(p) comprehensions take SCALAR elements (str/int/bool/null) only — floats
  are excluded because libjson numbers carry no int/float identity;
- served_profiles in served_profile_ingress_urls iterates WITHOUT an
  isinstance guard: list -> elements, str -> ASCII chars, dict -> keys,
  falsy -> [], other truthy scalar -> TypeError not iterable;
- profile / profile_name / indent are str|null; timeout is a number or absent;
- a MISSING seam script returns None on both sides (harness yields a NULL
  pointer); the oracle is fail-closed at the summary level.
"""
import ast
import builtins
import json
import os
import sys
import types

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path = None  # placeholder, replaced below with real pathlib Path
import pathlib as _pathlib
Path = _pathlib.Path
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "gm2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC_SERVED = os.path.join(QUARRY, "hermes_cli", "gateway_multiplex_served.py")
SRC_RUNTIME = os.path.join(QUARRY, "hermes_cli", "gateway_runtime_multiplex.py")

# fixture func name -> live def name
PY = {
    "live_default_gateway_pid": "live_default_gateway_pid",
    "recorded_served_profiles": "recorded_served_profiles",
    "multiplexer_served_secondaries": "multiplexer_served_secondaries",
    "served_profile_unserved_platforms": "served_profile_unserved_platforms",
    "served_profile_ingress_urls": "served_profile_ingress_urls",
    "format_ingress_url_lines": "format_ingress_url_lines",
    "notify_multiplexer_profiles_changed": "notify_multiplexer_profiles_changed",
    "multiplexer_root_for": "multiplexer_root_for",
    "implied_host_root": "implied_host_root",
    "multiplexer_serves_home": "multiplexer_serves_home",
}
WANTED = set(PY.values())

CUR = {"trace": [], "scripts": {}, "idx": [], "raised": [],
       "calls": {}}  # calls: seam-name -> list of call arg dicts (self-test aid)


def _exc_class(name):
    cls = getattr(builtins, name, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls
    return type(name, (Exception,), {})


def make_exc(spec):
    cls = _exc_class(spec["__raise__"])
    msg = spec.get("msg", "")
    if cls in (BaseExceptionGroup, ExceptionGroup):
        # group constructors need a non-empty exceptions sequence. A
        # BaseExceptionGroup with only Exception members auto-derives into an
        # ExceptionGroup (which `except Exception` catches) — keep a
        # BaseException member so it stays a true BaseExceptionGroup.
        inner = (KeyboardInterrupt("x") if cls is BaseExceptionGroup
                 else ValueError("x"))
        return cls(msg, [inner])
    if cls in (UnicodeDecodeError, UnicodeEncodeError, UnicodeTranslateError):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


class Raise:
    def __init__(self, spec):
        self.spec = spec
        self.exc = make_exc(spec)


# ── PurePosixPath model (mirrors C gm2_path_*) ──

def _parse_path(s):
    kind = 0
    if s.startswith("/"):
        kind = 2 if (s.startswith("//") and not s.startswith("///")) else 1
    parts = [seg for seg in s.split("/") if seg not in ("", ".")]
    return kind, parts


def _render_path(kind, parts):
    root = "//" if kind == 2 else ("/" if kind == 1 else "")
    if not parts:
        return root if root else "."
    return root + "/".join(parts)


class PathDouble:
    def __init__(self, p):
        if isinstance(p, PathDouble):
            self.kind, self.parts = p.kind, list(p.parts)
        elif isinstance(p, str):
            self.kind, self.parts = _parse_path(p)
        elif p is None:
            raise TypeError("argument should be a str or an os.PathLike object "
                            "where __fspath__ returns a str, not 'NoneType'")
        else:
            raise TypeError("argument should be a str or an os.PathLike object "
                            "where __fspath__ returns a str, not '%s'"
                            % type(p).__name__)

    @classmethod
    def _mk(cls, kind, parts):
        o = object.__new__(cls)
        o.kind, o.parts = kind, list(parts)
        return o

    @property
    def parent(self):
        if not self.parts:
            return PathDouble._mk(self.kind, [])
        if self.kind != 0 and len(self.parts) == 1:
            return PathDouble._mk(self.kind, [])
        return PathDouble._mk(self.kind, self.parts[:-1])

    @property
    def name(self):
        return self.parts[-1] if self.parts else ""

    def __truediv__(self, name):
        return PathDouble._mk(self.kind, self.parts + [str(name)])

    def __str__(self):
        return _render_path(self.kind, self.parts)

    def __repr__(self):
        return "PathDouble(%r)" % str(self)

    def __fspath__(self):
        return str(self)

    def as_posix(self):
        return str(self)

    def __eq__(self, other):
        if not isinstance(other, PathDouble):
            return NotImplemented
        return self.kind == other.kind and self.parts == other.parts

    def __hash__(self):
        return hash((self.kind, tuple(self.parts)))


def posix(p):
    if p is None:
        return None
    if isinstance(p, (PathDouble, Path)):
        return p.as_posix()
    return str(p).replace("\\", "/")


def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (Path, PathDouble)):
        return o.as_posix()
    if isinstance(o, BaseException):
        return str(o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
    return str(o)


def seam(name, args):
    args = {k: norm(v) for k, v in args.items() if v is not None}
    CUR["calls"].setdefault(name, []).append(args)
    sc = CUR["scripts"].get(name)
    if sc is None:
        r = None
    elif isinstance(sc, list):
        i = CUR["idx"].get(name, 0)
        CUR["idx"][name] = i + 1
        r = sc[min(i, len(sc) - 1)] if sc else None
    else:
        r = sc
    r = json.loads(json.dumps(r))          # deep copy (C harness json_copy)
    CUR["trace"].append({"ev": name, "args": args,
                         "ret": json.loads(json.dumps(r))})
    if isinstance(r, dict) and "__raise__" in r:
        rr = Raise(r)
        CUR["raised"].append(rr)
        raise rr.exc
    return r


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
    import logging
    from typing import Optional

    mod = types.ModuleType("gateway_multiplex_flow2_extracted")
    sys.modules["gateway_multiplex_flow2_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "gateway_multiplex_flow2_extracted",
        "json": json, "os": os, "sys": sys, "logging": logging, "types": types,
        "Path": PathDouble, "Optional": Optional,
    })
    import __future__
    flag = __future__.annotations.compiler_flag
    trees = []
    for src in (SRC_SERVED, SRC_RUNTIME):
        trees.append((ast.parse(open(src, encoding="utf-8").read()), src))
    for tree, src in trees:
        for node in tree.body:
            if isinstance(node, ast.FunctionDef) and node.name in WANTED:
                exec(compile(ast.Module(body=[node], type_ignores=[]), src,
                             "exec", flags=flag), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    return ns


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m


def _mod(name):
    m = types.ModuleType(name)
    sys.modules[name] = m
    return m


def install_stubs(ns):
    # logger.debug(fmt, name, exc_info=True) -> {msg, args, exc_info}
    def _log_debug(msg, *args, exc_info=False):
        d = {"msg": msg, "args": list(args)}
        if exc_info:
            d["exc_info"] = True
        return seam("logger_debug", d)

    ns["logger"] = types.SimpleNamespace(debug=_log_debug)

    hc = _mod("hermes_constants")

    def _default_root():
        r = seam("get_default_hermes_root", {})
        return PathDouble(r) if isinstance(r, str) else r

    hc.get_default_hermes_root = _default_root
    hc.named_profile_home = lambda home: seam(
        "named_profile_home", {"home": posix(home)})

    gateway = _pkg("gateway")
    gstatus = _mod("gateway.status")
    gstatus.live_gateway_pid_for_home = lambda home: seam(
        "live_gateway_pid_for_home", {"home": posix(home)})
    gstatus.read_runtime_status = lambda path=None: seam(
        "read_runtime_status", {"state_path": posix(path)})
    gstatus.shared_listener_mirror_platforms = lambda runtime, name: seam(
        "shared_listener_mirror_platforms",
        {"runtime": norm(runtime), "name": posix(name)})
    gateway.status = gstatus

    csock = _mod("gateway.control_socket")
    csock.rescan_gateway_profiles = lambda root, timeout=8.0: seam(
        "rescan_gateway_profiles", {"root": posix(root), "timeout": timeout})
    gateway.control_socket = csock

    hcli = _pkg("hermes_cli")
    gmm = _mod("hermes_cli.gateway_multiplex_mode")
    gmm.host_serves_named_profile = lambda default_home, home: seam(
        "host_serves_named_profile",
        {"default_home": posix(default_home), "home": posix(home)})
    hcli.gateway_multiplex_mode = gmm
    profiles = _mod("hermes_cli.profiles")
    profiles.normalize_profile_name = lambda name: seam(
        "normalize_profile_name", {"name": posix(name)})
    hcli.profiles = profiles


# ── drive ──

def drive(ns, func, inp):
    """Returns (ret, trace); ret is a raise spec on ANY BaseException."""
    name = PY[func]
    try:
        if func == "live_default_gateway_pid":
            ret = ns[name]()
        elif func == "recorded_served_profiles":
            dr = inp.get("default_root")
            root = PathDouble(dr) if isinstance(dr, str) and dr else None
            ret = ns[name](root)
        elif func == "multiplexer_served_secondaries":
            ret = ns[name]()
        elif func == "served_profile_unserved_platforms":
            ret = ns[name](inp.get("profile"))
        elif func == "served_profile_ingress_urls":
            ret = ns[name](inp.get("profile"))
        elif func == "format_ingress_url_lines":
            ret = ns[name](inp.get("urls"), inp.get("indent", "  "))
        elif func == "notify_multiplexer_profiles_changed":
            ret = ns[name](inp.get("profile_name"),
                           timeout=inp.get("timeout", 8.0))
        elif func == "multiplexer_root_for":
            ret = ns[name](PathDouble(inp.get("home")))
        elif func == "implied_host_root":
            ret = ns[name](PathDouble(inp.get("home")))
        elif func == "multiplexer_serves_home":
            ret = ns[name](PathDouble(inp.get("home")))
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:
        spec = None
        for rr in CUR["raised"]:
            if e is rr.exc:
                spec = rr.spec
                break
        ret = spec if spec is not None else {
            "__raise__": type(e).__name__, "msg": str(e)}
    return norm(ret), CUR["trace"]


def main():
    ns = extract()
    install_stubs(ns)
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
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        CUR["calls"] = {}
        if func not in PY:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        ret, trace = drive(ns, func, inp)
        want = {"ret": ret, "trace": trace, "state": {}}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE gateway_multiplex_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_multiplex_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


def selftest():
    """Drive every fixture case live; flag uniform AttributeError families
    naming a ported fn / seam stub, and report per-func coverage."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    ns = extract()
    install_stubs(ns)
    fixtures = os.path.join(os.path.dirname(__file__), "fixtures",
                            "gateway_multiplex_flow2.jsonl")
    names = set(WANTED) | {
        "get_default_hermes_root", "live_gateway_pid_for_home",
        "read_runtime_status", "shared_listener_mirror_platforms",
        "rescan_gateway_profiles", "named_profile_home",
        "normalize_profile_name", "host_serves_named_profile", "logger_debug"}
    bad = 0
    n = 0
    raises = {}
    per_func = {}
    for line in open(fixtures, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("{"):
            continue
        rec = json.loads(line)
        func = rec["func"]
        inp = rec.get("in") or {}
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        CUR["calls"] = {}
        n += 1
        per_func[func] = per_func.get(func, 0) + 1
        ret, _trace = drive(ns, func, inp)
        if isinstance(ret, dict) and "__raise__" in ret:
            kind = ret["__raise__"]
            raises[kind] = raises.get(kind, 0) + 1
            msg = ret.get("msg", "")
            if kind == "AttributeError" and any(
                    ("'%s'" % nm) in msg for nm in names):
                bad += 1
                print("SELFTEST binding-suspect %s: %s" % (func, msg))
    thin = [f for f in PY if per_func.get(f, 0) < 5]
    print("SELFTEST %d cases, %d binding-suspects, raises=%s" % (n, bad, raises))
    print("SELFTEST per-func counts: %s" % json.dumps(per_func, sort_keys=True))
    if thin:
        print("SELFTEST funcs with <5 cases: %s" % thin)
        bad += 1
    faulthandler.cancel_dump_traceback_later()
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        selftest()
    else:
        main()
