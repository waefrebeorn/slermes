#!/usr/bin/env python3
"""Oracle for the config.py second-wave UNCLAIMED FLOW port
(src/cli/config_flow2b.c, harness t_port_config_flow2b.c).

Drives the LIVE config.py functions with SCRIPTED I/O: every collaborator is a
recording stub fed the same scripts the C harness feeds its seams. Compares the
DECISION SEQUENCE (ordered seam names in "trace"), the return ("ret"), the
mutated state mirror (restricted to seeded keys) and captured print output.
Live defs are AST-extracted (module Assign/AnnAssign + FunctionDef exec); the
heavy quarry import chain is never imported. The env-write-lock contextmanager
is extracted with its decorator STRIPPED and driven as a raw generator so the
single `yield` is materialized as [None]. Fail-closed: a missing seam script
raises RuntimeError("unscripted seam: <name>"); 0 cases => exit 1.
"""
import ast
import builtins
import copy
import io as _io
import json
import os
import stat as _stat
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "config.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "config.py")

PY = {
    "warn_invalid_platform_toolsets": "_warn_invalid_platform_toolsets",
    "offer_new_optional_env_vars": "_offer_new_optional_env_vars",
    "offer_skill_config_vars": "_offer_skill_config_vars",
    "note_policy": "_note_policy",
    "read_env_lines": "_read_env_lines",
    "env_write_lock_holder": "_env_write_lock_holder",
    "env_write_lock": "_env_write_lock",
    "write_env_lines": "_write_env_lines",
    "cmd_config_migrate": "_cmd_config_migrate",
    "cmd_config_check": "_cmd_config_check",
    "platform_manifest_paths": "_platform_manifest_paths",
    "manifest_stamp": "platform_manifest_stamp",
    "platform_plugin_manifests": "_platform_plugin_manifests",
}


def make_exc(spec):
    import builtins as _b
    spec = spec or {}
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if t == "SystemExit":
        return SystemExit(spec.get("code", 1))
    cls = getattr(_b, t, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        e = cls(msg)
    else:
        e = Exception(msg)
    if t == "ModuleNotFoundError":
        e.name = spec.get("name")
    return e


def exc_spec(e):
    spec = {"type": type(e).__name__, "message": str(e)}
    if isinstance(e, SystemExit):
        spec["code"] = e.code
    if isinstance(e, ModuleNotFoundError):
        spec["name"] = getattr(e, "name", None)
    return spec


class FakePath:
    """Posix path double; / and .parent/.name real, stat() is a seam."""
    def __init__(self, s, ctx=None):
        self._s = str(s)
        self._ctx = ctx

    def __truediv__(self, other):
        base = self._s.rstrip("/") if self._s != "/" else ""
        return FakePath(base + "/" + str(other), self._ctx)

    def with_name(self, name):
        s = self._s.rstrip("/")
        if "/" in s:
            head = s.rsplit("/", 1)[0]
            return FakePath((head + "/" + str(name)) if head else "/" + str(name), self._ctx)
        return FakePath(str(name), self._ctx)

    @property
    def parent(self):
        s = self._s.rstrip("/")
        if "/" not in s:
            return FakePath(".", self._ctx)
        head = s.rsplit("/", 1)[0]
        return FakePath(head if head else "/", self._ctx)

    @property
    def name(self):
        return self._s.rstrip("/").rsplit("/", 1)[-1]

    def stat(self):
        return CURRENT["ctx"].path_stat(self)

    def __fspath__(self):
        return self._s

    def __str__(self):
        return self._s

    def __repr__(self):
        return f"FakePath({self._s!r})"


class StatDouble:
    def __init__(self, mode):
        self.st_mode = mode


class NullCM:
    def __enter__(self):
        return self
    def __exit__(self, *a):
        return False


class FileDouble:
    """os.fdopen() result: writelines/flush are seams, fileno is real."""
    def __init__(self, fd, ctx):
        self._fd = fd
        self.writelines = ctx.recorder("writelines")
        self.flush = ctx.recorder("flush")
    def fileno(self):
        return self._fd
    def __enter__(self):
        return self
    def __exit__(self, *a):
        return False


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.stdout = []
        self.stderr = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state", {}) or {})

    def peek(self, name):
        entry = self.io.get(name)
        if entry is None:
            return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            return entry[idx] if idx < len(entry) else None
        return entry

    def consume(self, name):
        entry = self.io.get(name)
        if isinstance(entry, list):
            self.cursors[name] = self.cursors.get(name, 0) + 1

    def invoke(self, name):
        """Trace (unless import_raise), consume, apply state effs, raise/ret."""
        entry = self.peek(name)
        if isinstance(entry, dict) and isinstance(entry.get("import_raise"), dict):
            self.consume(name)
            raise make_exc(entry["import_raise"])
        self.trace.append(name)
        if entry is None:
            raise RuntimeError("unscripted seam: " + name)
        self.consume(name)
        if isinstance(entry, dict):
            eff = entry.get("state")
            if isinstance(eff, dict):
                self.state.update(copy.deepcopy(eff))
            r = entry.get("raise")
            if isinstance(r, dict):
                raise make_exc(r)
            return entry.get("ret")
        return entry

    def recorder(self, name):
        ctx = self
        def call(*a, **k):
            v = ctx.invoke(name)
            if name in ("get_project_root", "get_hermes_home") and isinstance(v, str):
                return FakePath(v, ctx)
            return v
        return call

    def fdopen(self, fd, mode="r", *a, **k):
        self.invoke("fdopen")
        return FileDouble(fd, self)

    def path_stat(self, node):
        raw = self.invoke("path_stat")
        if isinstance(raw, dict) and "mode" in raw:
            return StatDouble(raw["mode"])
        return raw

    def file_lock(self, *a, **k):
        self.invoke("file_lock")
        return NullCM()


CURRENT = {"ctx": None}
REAL = {}
NS = {}


def _ensure_pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def proj(v):
    if isinstance(v, dict):
        return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)):
        return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(v)
    if isinstance(v, FakePath):
        return str(v)
    if isinstance(v, Path):
        return v.as_posix()
    if isinstance(v, StatDouble):
        return {"__stat__": {"st_mode": v.st_mode}}
    if isinstance(v, BaseException):
        return exc_spec(v)
    return v


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {
        "__name__": "hermes_cli.config",
        "os": os, "sys": sys, "json": json, "copy": copy, "stat": _stat,
        "threading": __import__("threading"),
        "Path": FakePath, "Any": object, "Dict": dict, "List": list,
        "Optional": object, "Literal": object, "Tuple": tuple, "Set": set,
    }
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            nd = node
            if node.name == "_env_write_lock" and node.decorator_list:
                nd = copy.copy(node)
                nd.decorator_list = []
            try:
                exec(compile(ast.Module(body=[nd], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # Colors (ImportFrom, not Assign) — bind the real ANSI constant class.
    try:
        col_ns = {"os": os, "sys": sys}
        exec(compile(open(str(QUARRY / "hermes_cli" / "colors.py"), encoding="utf-8").read(),
                     "colors.py", "exec"), col_ns)
        ns["Colors"] = col_ns["Colors"]
    except Exception:
        class Colors:
            BOLD = DIM = RED = GREEN = YELLOW = RESET = ""
        ns["Colors"] = Colors
    missing = [v for v in PY.values() if v not in ns]
    for sib in ("_offer_list", "_prompt_and_save_env", "_set_nested",
                "_persist_migration", "_platform_manifest_paths",
                "_env_write_lock_holder", "Colors", "ENV_VARS_BY_VERSION"):
        if sib not in ns:
            missing.append(sib)
    # OPTIONAL_ENV_VARS / REQUIRED_ENV_VARS arrive via ImportFrom; seed_state
    # overrides them from the fixture constants on every case.
    ns.setdefault("OPTIONAL_ENV_VARS", {})
    ns.setdefault("REQUIRED_ENV_VARS", {})
    if missing:
        print("FATAL config_flow2b: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    for k in ("_env_write_lock_holder", "_platform_manifest_paths"):
        REAL[k] = ns[k]
    return ns


def install_stubs(ctx, inp):
    ns = NS
    rec = ctx.recorder

    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    _ensure_pkg("hermes_cli")
    _ensure_pkg("agent")
    _ensure_pkg("utils")

    # AUTO cosmetic seams.
    def print_stub(*a, file=None, **k):
        ctx.trace.append("print")
        text = a[0] if a else ""
        (ctx.stderr if file is sys.stderr else ctx.stdout).append(text)
        return None

    def color_stub(*a, **k):
        ctx.trace.append("color")
        return a[0] if a else ""

    ns["print"] = print_stub
    ns["color"] = color_stub
    ns["input"] = rec("input")

    for name in (
        "read_raw_config", "read_raw_config_readonly", "get_project_root",
        "get_hermes_home", "get_missing_env_vars", "get_missing_config_fields",
        "check_config_version", "migrate_config", "get_env_value", "_offer_list",
        "_prompt_and_save_env", "_set_nested", "_persist_migration",
        "file_signature", "fast_safe_load", "mkstemp_beside", "atomic_replace",
        "_secure_file", "line_input", "lock_holder",
    ):
        ns[name] = rec(name)
    # Claimed pure siblings are seamed under the C seam name (no leading '_').
    ns["_print_banner"] = rec("print_banner")
    ns["_tools_suffix"] = rec("tools_suffix")
    ns["_sanitize_env_lines"] = rec("sanitize_env_lines")
    ns["_secure_file"] = rec("secure_file")

    # Composed siblings that are themselves ported: stub for composition;
    # their direct cases drive REAL[...] instead.
    ns["_env_write_lock_holder"] = rec("lock_holder")

    # logger (the module Assign would otherwise overwrite this).
    ns["logger"] = types.SimpleNamespace(
        debug=rec("logger.debug"), warning=rec("logger.warning"))

    # I/O-returning seams needing materialization.
    def open_stub(path, mode="r", *a, **k):
        raw = ctx.invoke("open")
        if isinstance(raw, dict) and "raise" in raw:  # already handled by invoke
            pass
        if isinstance(raw, list):
            raw = "".join(str(x) for x in raw)
        return _io.StringIO(raw if isinstance(raw, str) else "")

    ns["open"] = open_stub

    # os double (filesystem probes are seams; path algebra stays real).
    class ScanCM:
        def __init__(self, entries):
            self.entries = entries
        def __enter__(self):
            return self.entries
        def __exit__(self, *a):
            return False

    def scandir_stub(path):
        raw = ctx.invoke("os.scandir") or []
        entries = []
        for d in raw:
            def mk(d=d):
                def is_dir():
                    if d.get("is_dir_raise"):
                        raise OSError("is_dir denied")
                    return bool(d.get("is_dir", False))
                return types.SimpleNamespace(name=d.get("name", ""),
                                             path=d.get("path", ""), is_dir=is_dir)
            entries.append(mk())
        return ScanCM(entries)

    ns["os"] = types.SimpleNamespace(
        scandir=scandir_stub, stat=ctx.path_stat,
        fdopen=ctx.fdopen, fsync=rec("fsync"),
        unlink=rec("unlink"), chmod=rec("chmod"),
        environ=types.SimpleNamespace(get=rec("os.environ.get")))

    # in-function imports.
    sys.modules["hermes_cli.auth"] = mod("hermes_cli.auth", _file_lock=ctx.file_lock)
    sys.modules["hermes_cli.config_check_diagnostics"] = mod(
        "hermes_cli.config_check_diagnostics",
        config_check_diagnostics=rec("config_check_diagnostics"))
    sys.modules["hermes_cli.toolset_validation"] = mod(
        "hermes_cli.toolset_validation",
        saved_toolset_resolver=rec("saved_toolset_resolver"),
        validate_platform_toolsets=rec("validate_platform_toolsets"))
    sys.modules["agent.safe_worker_policy"] = mod(
        "agent.safe_worker_policy", safe_worker_enabled=rec("safe_worker_enabled"))
    sys.modules["hermes_constants"] = mod(
        "hermes_constants", hermes_home_key=rec("hermes_home_key"))

    redact = inp.get("redact") or {}
    rmod = types.ModuleType("agent.redact")
    if redact.get("has_setter"):
        setattr(rmod, inp.get("setter", ""), rec("note"))
    sys.modules["agent.redact"] = rmod

    # skill_config_prefix import gate.
    c = inp.get("constants") or {}
    smod = types.ModuleType("agent.skill_utils")
    if not c.get("skill_config_prefix_import_raise"):
        smod.SKILL_CONFIG_PREFIX = c.get("skill_config_prefix", "skills.config")
    sys.modules["agent.skill_utils"] = smod


def seed_state(ctx, inp):
    ns = NS
    st = ctx.state
    c = inp.get("constants") or {}
    if "env_vars_by_version" in c:
        ns["ENV_VARS_BY_VERSION"] = {int(k): v for k, v in c["env_vars_by_version"].items()}
    if "optional_env_vars" in c:
        ns["OPTIONAL_ENV_VARS"] = copy.deepcopy(c["optional_env_vars"])
    if "required_env_vars" in c:
        ns["REQUIRED_ENV_VARS"] = copy.deepcopy(c["required_env_vars"])
    if not isinstance(st.get("results"), dict):
        st["results"] = {"env_added": [], "config_added": [], "warnings": []}
    if "skipped" not in st:
        st["skipped"] = []


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    install_stubs(ctx, inp)
    seed_state(ctx, inp)
    ns = NS
    out = {"ret": None, "trace": ctx.trace}
    try:
        if func == "warn_invalid_platform_toolsets":
            out["ret"] = ns["_warn_invalid_platform_toolsets"](
                ctx.state["results"], bool(inp.get("quiet", False)))
        elif func == "offer_new_optional_env_vars":
            out["ret"] = ns["_offer_new_optional_env_vars"](
                inp.get("current_ver", 0), inp.get("latest_ver", 0), ctx.state["results"])
        elif func == "offer_skill_config_vars":
            out["ret"] = ns["_offer_skill_config_vars"](
                copy.deepcopy(inp.get("missing_skill_config") or []), ctx.state["results"])
        elif func == "note_policy":
            out["ret"] = ns["_note_policy"](inp.get("setter", ""), inp.get("raw"))
        elif func == "read_env_lines":
            out["ret"] = ns["_read_env_lines"](FakePath(inp.get("env_path", ""), ctx))
        elif func == "env_write_lock_holder":
            out["ret"] = ns["_env_write_lock_holder"](FakePath(inp.get("env_path", ""), ctx))
        elif func == "env_write_lock":
            # file_lock is scripted via the lock_holder / file_lock seams in the
            # real holder call; drive the raw generator to completion.
            out["ret"] = list(ns["_env_write_lock"](FakePath(inp.get("env_path", ""), ctx)))
        elif func == "write_env_lines":
            out["ret"] = ns["_write_env_lines"](
                FakePath(inp.get("env_path", ""), ctx),
                copy.deepcopy(inp.get("lines") or []),
                preserve_mode=bool(inp.get("preserve_mode", False)))
        elif func == "cmd_config_migrate":
            out["ret"] = ns["_cmd_config_migrate"](None)
        elif func == "cmd_config_check":
            out["ret"] = ns["_cmd_config_check"](None)
        elif func == "platform_manifest_paths":
            home = inp.get("home")
            out["ret"] = list(ns["_platform_manifest_paths"](
                FakePath(home, ctx) if home else None, inp.get("source", "all")))
        elif func == "manifest_stamp":
            home = inp.get("home")
            hp = FakePath(home, ctx) if home else None
            # Materialize the unclaimed generator sibling first (it is a seam
            # for this composition); the C port likewise emits file_signature
            # only after the whole path scan, matching the trace order.
            real_paths = list(ns["_platform_manifest_paths"](hp, "user"))
            saved_paths = ns["_platform_manifest_paths"]
            ns["_platform_manifest_paths"] = lambda h, src: iter(real_paths)
            try:
                out["ret"] = ns["platform_manifest_stamp"](hp)
            finally:
                ns["_platform_manifest_paths"] = saved_paths
        elif func == "platform_plugin_manifests":
            home = inp.get("home")
            hp = FakePath(home, ctx) if home else None
            src = inp.get("source", "all")
            sw = ctx.recorder("safe_worker_enabled")()  # live checks this first
            if sw:
                out["ret"] = []
            else:
                real_paths = list(ns["_platform_manifest_paths"](hp, src))
                saved_paths = ns["_platform_manifest_paths"]
                ns["_platform_manifest_paths"] = lambda h, s: iter(real_paths)
                swmod = sys.modules["agent.safe_worker_policy"]
                saved_sw = swmod.safe_worker_enabled
                swmod.safe_worker_enabled = lambda: sw
                try:
                    out["ret"] = list(ns["_platform_plugin_manifests"](
                        hp, src, strict=bool(inp.get("strict", False)),
                        skipped=ctx.state["skipped"]))
                finally:
                    ns["_platform_manifest_paths"] = saved_paths
                    swmod.safe_worker_enabled = saved_sw
        else:
            raise KeyError(func)
    except BaseException as e:
        out["ret"] = {"__raise__": exc_spec(e)}
    out["ret"] = proj(out["ret"])
    out["state"] = {k: proj(ctx.state[k]) for k in (inp.get("state") or {}) if k in ctx.state}
    out["stdout"] = ctx.stdout
    out["stderr"] = ctx.stderr
    return out


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        func = c.get("func", "")
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            want = run_case(func, c.get("in") or {})
        except Exception as e:
            mism += 1
            print(f"PYERR {func}: {type(e).__name__}: {e}")
            continue
        got = c.get("out") or {}
        ok = (got.get("trace") == want.get("trace")
              and got.get("ret") == want.get("ret")
              and got.get("state") == want.get("state")
              and got.get("stdout") == want.get("stdout")
              and got.get("stderr") == want.get("stderr"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE config_flow2b: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    NS = extract()
    sys.exit(main())
