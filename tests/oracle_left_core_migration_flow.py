#!/usr/bin/env python3
"""Oracle for left_core_migration_flow.c — hermes_cli/left_core_migration.py
FLOW cores + leaves (LIVE authority: the quarry left_core_migration.py).

Drives the LIVE functions with SCRIPTED I/O: the feature-table hint builders,
the read-only in-use predicates, the notice/scope leaves and the post-split say
flow are AST-extracted from the quarry (no heavy hermes_cli.left_core_migration
import). The LeftCoreFeature frozen dataclass and the LEFT_CORE table are seeded
(module constants Assign AND AnnAssign); each row's injected in_use / notice
callables are replaced by recording seam stubs keyed by the row's plugin name.
Every config/auth/env read, path probe, hermes_constants home/key helper, the
providers registry, the memory_provider_migration collaborators, logger.warning,
the injected say sink and the CLAIMED siblings (_toolset_listed, _at_host,
_marked) plus unported collaborators (_merged_platform_block, routes_at_hosts,
_queue_notice) are replaced by recording stubs returning the SAME scripted
results the C harness feeds its universal seam. Internal composition between
PORTED functions runs REAL on both sides. Compares the ordered DECISION SEQUENCE
+ return value (+ mutated config for _mark) with ordered-dict comparison.

Per-case BaseException catch: SystemExit / KeyboardInterrupt / GeneratorExit /
CancelledError escape every `except Exception` site and are compared as raise
specs; scripted raises round-trip bare. Fail-closed: a missing/empty seam script
returns a RuntimeError raise spec (recorded, then raised), matching the C
harness; extraction misses / zero cases exit 2.

Input: stdin JSON lines from t_port_left_core_migration_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "config"?:...}}
Summary: ORACLE left_core_migration_flow: N cases, M mismatches.

Script semantics (identical on both sides): a seams[name] ARRAY is a per-call
queue (last value repeats); a single value is a constant; a MISSING/empty script
is a fail-closed RuntimeError. {"__raise__": cls, "msg": ...} scripts an exception.

Fixture domain (kept identical on both sides): ASCII text; POSIX lexical paths
(home non-empty); integral JSON numbers read as int; a PlatformConfig desc is
canonicalized to {enabled,token,extra} (defaults false/null/{}); frozenset/set/
tuple returns normalize to sorted/ordered arrays; _provider_feature returns the
row's plugin name or null; logger.debug/info silent; str()-reached seam values
are scalars; .get/.strip on a scripted null/non-str models the live AttributeError.
"""
import ast
import builtins
import contextvars
import dataclasses
import json
import os
import sys
import types
from dataclasses import dataclass
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env runner: derive home from __file__ (repo is parent.parent).
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "lcm_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "left_core_migration.py")

# func key == live python name (module-level).
PY = {
    "_no_notice": "_no_notice",
    "profile_env_names": "profile_env_names",
    "launch_env_names": "launch_env_names",
    "legacy_plugin_alias": "legacy_plugin_alias",
    "legacy_plugin_keys": "legacy_plugin_keys",
    "_provider_feature": "_provider_feature",
    "_install_sentence": "_install_sentence",
    "platform_install_hint": "platform_install_hint",
    "terminal_backend_install_hint": "terminal_backend_install_hint",
    "moved_plugin_hint": "moved_plugin_hint",
    "_load_error": "_load_error",
    "provider_left_core": "provider_left_core",
    "provider_install_hint": "provider_install_hint",
    "feature_in_use": "feature_in_use",
    "host_install_hint": "host_install_hint",
    "moved_command_hint": "moved_command_hint",
    "_noticed": "_noticed",
    "_read_config": "_read_config",
    "_auth_store": "_auth_store",
    "plugin_present": "plugin_present",
    "agent_imports_in_use": "agent_imports_in_use",
    "_plugins_listed": "_plugins_listed",
    "_plugin_lists": "_plugin_lists",
    "_bundled_plugin_selected": "_bundled_plugin_selected",
    "langfuse_in_use": "langfuse_in_use",
    "spotify_in_use": "spotify_in_use",
    "_home_env": "_home_env",
    "camofox_in_use": "camofox_in_use",
    "singularity_in_use": "singularity_in_use",
    "homeassistant_in_use": "homeassistant_in_use",
    "platform_configured_on": "platform_configured_on",
    "_platform_config": "_platform_config",
    "_yuanbao_notice": "_yuanbao_notice",
    "_notice_file": "_notice_file",
    "_scope_kept": "_scope_kept",
    "_mark": "_mark",
    "defer_startup_notice": "defer_startup_notice",
    "_say_post_split": "_say_post_split",
}
WANTED = set(PY.values())
# Functions taking a LeftCoreFeature row directly (materialized from inp.feature).
FEAT_FUNCS = {"_install_sentence", "_load_error", "feature_in_use", "_noticed",
              "_scope_kept", "_mark", "_say_post_split"}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": [], "out_config": None}
FEATURE_CLS = None


class CancelledError(BaseException):
    pass


def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls == "SystemExit":
        return SystemExit(msg)
    if cls == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if cls == "GeneratorExit":
        return GeneratorExit(msg)
    if cls == "CancelledError":
        return CancelledError(msg)
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        return b(msg)
    return type(cls, (Exception,), {})(msg)


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    args = json.loads(json.dumps(args))  # snapshot: live mutation cannot corrupt trace
    scripts = CUR["scripts"]
    # Distinguish a MISSING script (fail-closed) from a scripted null (None).
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
    r = json.loads(json.dumps(r))  # deep copy: live mutation cannot corrupt script/trace
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


# ── normalization / canonical comparison ──

def posix(p):
    if p is None:
        return None
    if isinstance(p, (PathDouble, Path)):
        return p.as_posix()
    return str(p).replace("\\", "/")


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PCDouble):
        return {"enabled": o.enabled, "token": o.token, "extra": norm(o.extra)}
    if FEATURE_CLS is not None and isinstance(o, FEATURE_CLS):
        return o.plugin
    if isinstance(o, (Path, PathDouble)):
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


# ── doubles ──

def _join_lex(a, b):
    if not b:
        return a
    if b.startswith("/"):
        return b
    if not a:
        return b
    if a.endswith("/"):
        return a + b
    return a + "/" + b


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

    def __truediv__(self, other):
        return PathDouble(_join_lex(self.s, str(other)))

    def is_dir(self):
        return seam("path_is_dir", {"path": self.s})

    def is_file(self):
        return seam("path_is_file", {"path": self.s})

    def as_posix(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __str__(self):
        return self.s


class PCDouble:
    def __init__(self, enabled=False, token=None, extra=None):
        self.enabled = enabled
        self.token = token
        self.extra = extra if isinstance(extra, dict) else {}

    @classmethod
    def from_dict(cls, data):
        r = seam("platform_config_from_dict", {"block": data})
        if not isinstance(r, dict):
            r = {}
        return cls(enabled=bool(r.get("enabled", False)), token=r.get("token"),
                   extra=r.get("extra"))


class EnvironDouble:
    def get(self, name, default=None):
        return seam("os_environ_get", {"name": name})


class RecLogger:
    def debug(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass

    def warning(self, message, *a, **k):
        seam("logger_warning", {"message": message})

    def error(self, *a, **k):
        pass


# ── extraction ──

def extract():
    import __future__
    global FEATURE_CLS
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("left_core_migration_extracted")
    sys.modules["left_core_migration_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "left_core_migration_extracted",
        "json": json, "os": types.SimpleNamespace(environ=EnvironDouble()), "sys": sys,
        "contextvars": contextvars, "dataclass": dataclass,
        "Path": PathDouble, "Callable": object, "Optional": object,
        "logging": types.SimpleNamespace(getLogger=lambda name: RecLogger()),
    })
    # Placeholder in_use/notice callables referenced by the LEFT_CORE Assign, and
    # the module-level _core_platforms_except filesystem scan (out of domain).
    for nm in ("homeassistant_in_use", "spotify_in_use", "langfuse_in_use",
               "camofox_in_use", "singularity_in_use", "agent_imports_in_use",
               "copilot_in_use", "yuanbao_in_use"):
        ns[nm] = lambda *a, **k: False
    for nm in ("_no_notice", "copilot_relogin_notice", "_yuanbao_notice"):
        ns[nm] = lambda *a, **k: ""
    ns["_core_platforms_except"] = lambda platform, *others: ()
    tree = ast.parse(src)
    fut = __future__.annotations.compiler_flag
    # The frozen LeftCoreFeature dataclass must exist before the LEFT_CORE Assign.
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "LeftCoreFeature":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=fut), ns)
    # Module constants (plain-Name Assign/AnnAssign targets), in source order.
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
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=fut), ns)
        except Exception:
            pass
    # Target defs.
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=fut), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for k in ("LEFT_CORE", "_COPILOT_API", "_COPILOT_ACP", "_YUANBAO_NOTICE",
              "_SCOPED_KEY", "_INSTALLED_KEY", "_USED_KEY", "LeftCoreFeature"):
        if k not in ns:
            print("FATAL missing constant/class: %s" % k)
            sys.exit(2)
    # logger is an Assign (logging.getLogger) — re-set the recorder AFTER seeding.
    ns["logger"] = RecLogger()
    FEATURE_CLS = ns["LeftCoreFeature"]
    # Replace each row's injected in_use / notice callables with recording stubs.
    def make_in_use(plugin):
        def stub(home, *, process_env=False):
            return seam("in_use", {"plugin": plugin, "home": posix(home),
                                   "process_env": process_env})
        return stub

    def make_notice(plugin):
        def stub(home, *, process_env=False):
            return seam("notice", {"plugin": plugin, "home": posix(home),
                                   "process_env": process_env})
        return stub

    ns["LEFT_CORE"] = tuple(
        dataclasses.replace(f, in_use=make_in_use(f.plugin), notice=make_notice(f.plugin))
        for f in ns["LEFT_CORE"])
    return ns


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m


def _submod(pkg, name):
    m = types.ModuleType(name)
    sys.modules[name] = m
    setattr(pkg, name.rsplit(".", 1)[-1], m)
    return m


def install_stubs(ns):
    # ── claimed siblings + unported collaborators (seamed, never re-ported) ──
    ns["_toolset_listed"] = lambda config, names, *, own_platform="": seam(
        "toolset_listed", {"config": config, "names": sorted(names),
                           "own_platform": own_platform})
    ns["_at_host"] = lambda url, host: seam("at_host", {"url": url, "host": host})
    ns["_marked"] = lambda config, key, feature: seam(
        "marked", {"config": json.loads(json.dumps(config)), "key": key,
                   "plugin": feature.plugin})
    ns["_merged_platform_block"] = lambda home, name: seam(
        "merged_platform_block", {"home": posix(home), "name": name})
    ns["routes_at_hosts"] = lambda home, hosts: seam(
        "routes_at_hosts", {"home": posix(home), "hosts": list(hosts)})
    ns["_queue_notice"] = lambda home, message: seam(
        "queue_notice", {"home": posix(home), "message": message})
    ns["_install_command"] = lambda name, home: seam(
        "install_command", {"plugin": name, "home": posix(home)})
    ns["_failed_recently"] = lambda home, plugin: seam(
        "failed_recently", {"home": posix(home), "plugin": plugin})
    ns["_note_failure"] = lambda home, plugin: seam(
        "note_failure", {"home": posix(home), "plugin": plugin})

    # ── in-function imports via sys.modules stubs ──
    utils = types.ModuleType("utils")
    utils.read_json_or_empty = lambda path: seam("read_json_or_empty", {"path": posix(path)})
    utils.is_truthy_value = lambda value, default=False: seam("is_truthy_value", {"value": value})
    sys.modules["utils"] = utils

    hc = types.ModuleType("hermes_constants")
    hc.get_hermes_home = lambda: PathDouble(seam("get_hermes_home", {}))
    hc.hermes_home_key = lambda path=None: seam("hermes_home_key", {"home": posix(path)})
    hc.get_routing_process_hermes_home = lambda: PathDouble(
        seam("get_routing_process_hermes_home", {}))
    sys.modules["hermes_constants"] = hc

    providers = types.ModuleType("providers")
    providers.plugin_load_error = lambda plugin: seam("plugin_load_error", {"plugin": plugin})
    providers.get_provider_profile = lambda name: seam("get_provider_profile", {"name": name})
    sys.modules["providers"] = providers

    agent = _pkg("agent")
    ss = _submod(agent, "agent.secret_scope")
    ss.load_env_file = lambda env_path: seam("load_env_file", {"path": posix(env_path)})
    ss.get_secret = lambda name, default=None: seam("get_secret", {"name": name, "default": default})
    ss.current_secret_scope = lambda: seam("current_secret_scope", {})
    ss.current_secret_scope_home = lambda: seam("current_secret_scope_home", {})

    hcli = _pkg("hermes_cli")
    cb = _submod(hcli, "hermes_cli.config_backend")
    cb.read_config_doc = lambda path: seam("read_config_doc", {"path": posix(path)})
    el = _submod(hcli, "hermes_cli.env_loader")
    el.resolve_home_dotenv = lambda home: seam("resolve_home_dotenv", {"home": posix(home)})

    gw = _pkg("gateway")
    cl = _submod(gw, "gateway.config_loader")
    cl.read_yaml_layers = lambda home: seam("read_yaml_layers", {"home": posix(home)})
    gc = _submod(gw, "gateway.config")
    gc.PlatformConfig = PCDouble


# ── drive ──

def _feat(ns, inp):
    return next(f for f in ns["LEFT_CORE"] if f.plugin == inp.get("feature"))


def drive(ns, func, inp):
    CUR["out_config"] = None
    home = PathDouble(inp.get("home", ""))
    pe = bool(inp.get("process_env", False))
    try:
        if func == "_mark":
            # The harness copies config BEFORE the feature lookup, so an unknown
            # feature (StopIteration) still echoes the unchanged config copy.
            config = json.loads(json.dumps(inp.get("config") or {}))
            CUR["out_config"] = config
            feat = _feat(ns, inp)
            ns[func](config, inp.get("key", ""), feat)
            ret = None
        elif func in FEAT_FUNCS:
            feat = _feat(ns, inp)
            if func == "_install_sentence":
                ret = ns[func](feat)
            elif func == "_load_error":
                ret = ns[func](feat)
            elif func == "feature_in_use":
                ret = ns[func](feat, home, process_env=pe)
            elif func == "_noticed":
                ret = ns[func](inp.get("message", ""), home, feat, pe)
            elif func == "_scope_kept":
                ret = ns[func](inp.get("config") or {}, feat)
            elif func == "_say_post_split":
                ret = ns[func](home, feat, say=lambda m: seam("say", {"message": m}),
                               backoff=bool(inp.get("backoff", False)), process_env=pe)
            else:
                raise ValueError("unhandled feat func: %s" % func)
        elif func in ("profile_env_names", "launch_env_names"):
            ret = ns[func]()
        elif func in ("legacy_plugin_alias", "moved_plugin_hint"):
            ret = ns[func](inp.get("name", ""))
        elif func == "legacy_plugin_keys":
            ret = ns[func](inp.get("plugin", ""))
        elif func in ("_provider_feature", "provider_left_core", "provider_install_hint"):
            ret = ns[func](inp.get("name"))
        elif func == "platform_install_hint":
            ret = ns[func](inp.get("platform", ""))
        elif func == "terminal_backend_install_hint":
            ret = ns[func](inp.get("backend", ""))
        elif func == "host_install_hint":
            ret = ns[func](inp.get("base_url"))
        elif func == "moved_command_hint":
            ret = ns[func](inp.get("prog", ""), inp.get("value", ""), inp.get("action", ""))
        elif func in ("_read_config", "_auth_store", "_notice_file"):
            ret = ns[func](home)
        elif func == "plugin_present":
            ret = ns[func](inp.get("plugin", ""), home)
        elif func in ("agent_imports_in_use", "langfuse_in_use", "spotify_in_use",
                      "camofox_in_use", "singularity_in_use", "homeassistant_in_use",
                      "_yuanbao_notice", "_no_notice"):
            ret = ns[func](home, process_env=pe)
        elif func == "_plugins_listed":
            ret = ns[func](home, tuple(inp.get("names", [])))
        elif func == "_plugin_lists":
            ret = ns[func](home, inp.get("plugin", ""), also=tuple(inp.get("also", [])))
        elif func == "_bundled_plugin_selected":
            ret = ns[func](home, inp.get("plugin", ""))
        elif func == "_home_env":
            ret = ns[func](home, inp.get("name", ""), process_env=pe)
        elif func == "platform_configured_on":
            ret = ns[func](home, inp.get("name", ""))
        elif func == "_platform_config":
            ret = ns[func](inp.get("block") or {})
        elif func == "defer_startup_notice":
            ret = ns[func](inp.get("message", ""), log=bool(inp.get("log", True)))
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
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        if func not in PY:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        ret = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if func == "_mark":
            want["config"] = norm(CUR["out_config"])
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE left_core_migration_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE left_core_migration_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()