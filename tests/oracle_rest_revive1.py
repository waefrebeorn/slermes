#!/usr/bin/env python3
"""oracle_rest_revive1.py — differential oracle for the deep-stub rest group-1 revival.

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the LIVE
quarry defs, seeds module constants (Assign + AnnAssign), drives each live
function under the SAME scripted seam results the C harness consumed, and
compares ordered trace + ret + post-state. BaseException is caught per case
(SystemExit included); missing/exhausted scripts fail closed on both sides.

Seam conventions (identical on both sides):
- In-function imports become "import_<name>" seams; a scripted ImportError
  models a failed import. The imported callable is the "<name>" seam.
- winreg is a module double: "import_winreg" fires on first attribute access;
  HKEY_CURRENT_USER/REG_EXPAND_SZ are pinned literals ("HKEY_CURRENT_USER", 2);
  OpenKey returns a scripted handle wrapped in a silent context manager.
- Path results normalize to POSIX (as_posix); os.path is PINNED to ntpath
  (Windows target) where the live code reads basenames/dirnames.
- Integral JSON numbers model Python ints, fractional ones floats.
- strip_cache_points projection: {"same": res is kwargs, "value": res} —
  the live identity return is part of the contract (callers retry on it).
- logger.* seams record {"msg": fmt, "args": [...]} verbatim (format
  specifiers apply only at emission); exception args carry str(exc).
- Fixture restrictions (documented, not silently dropped): str(detail) /
  str(modelId) slots use str/int/bool/None only (no container repr);
  max_chars is int-or-missing; self doubles always carry the attributes the
  live code reads OUTSIDE try/except (config, _profile_home_cache, _reads,
  _last_writer); scripted exception classes use plain str(exc) == message
  (no KeyError repr slot).

Gate:
  bash tests/run_one_oracle.sh rest_revive1 oracle_rest_revive1.py "" "lib/libjson/json.o"
"""
import ast
import builtins
import copy
import __future__
import json
import math
import ntpath
import os
import re
import sys
import textwrap
import types
from pathlib import Path, PurePosixPath, PureWindowsPath

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home_r1"))

FUT = __future__.annotations.compiler_flag
MISSING = object()


def norm(x):
    """Integral floats -> ints; tuples/sets -> lists (sets sorted); Path -> posix."""
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x) and abs(x) < 1e16:
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(norm(v) for v in x)
    if isinstance(x, Path):
        return x.as_posix()
    return x


def posix(x):
    return x.as_posix() if isinstance(x, Path) else x


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class Raise:
    def __init__(self, spec):
        ty = spec.get("type", "Exception")
        msg = spec.get("message", "")
        self.spec = {"__raise__": ty, "message": msg}
        cls = getattr(builtins, ty, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
            cls = type(ty, (Exception,), {})
        self.exc = cls(msg)


def build_exc(spec):
    return Raise(spec).exc


class Script:
    def __init__(self, io_map):
        self.io = io_map or {}
        self.cursors = {}

    def next(self, name):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            # Return a Raise (not a bare raise) so make_rec records the seam
            # event with the spec ret — exactly like the C dispatcher, which
            # returns the spec and lets the port's seam helper record it.
            return Raise({"type": "RuntimeError", "message": f"no script for {name}"})
        if isinstance(entry, list):
            i = self.cursors.get(name, 0)
            if i >= len(entry):
                return Raise({"type": "RuntimeError", "message": f"script exhausted for {name}"})
            self.cursors[name] = i + 1
            entry = entry[i]
        if isinstance(entry, dict) and entry.get("__script__"):
            if "raise" in entry:
                return Raise(entry["raise"])
            return entry.get("ret")
        return entry


# ── AST extraction ──────────────────────────────────────────────────────────

def compile_node(node, filename):
    mod = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, filename, "exec", flags=FUT, dont_inherit=True)


def load_ns(relpath, seeds, want_funcs=(), want_classes=(), require=()):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_r1_" + re.sub(r"\W", "_", relpath)
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seeds)
    ns["__name__"] = modname
    consts = [n for n in tree.body
              if isinstance(n, (ast.Assign, ast.AnnAssign))
              and n.value is not None
              and all(isinstance(t, ast.Name) for t in
                      (n.targets if isinstance(n, ast.Assign) else [n.target]))]

    def seed_consts(strict):
        for n in consts:
            try:
                exec(compile_node(n, relpath), ns)
            except Exception:
                if strict:
                    raise

    seed_consts(False)
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name in want_classes:
            exec(compile_node(n, relpath), ns)
    for n in ast.walk(tree):
        if isinstance(n, ast.FunctionDef) and n.name in want_funcs:
            # Strip decorators (@property/@abstractmethod) and dedent class
            # methods; compile the source segment so annotations stay strings.
            seg = textwrap.dedent(ast.get_source_segment(src, n))
            exec(compile(seg, relpath, "exec", flags=FUT, dont_inherit=True), ns)
    seed_consts(False)
    for name in list(want_funcs) + list(want_classes) + list(require):
        assert name in ns, f"seed failure: {relpath}:{name}"
    return ns


def base_seeds(modname):
    logging_double = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            warning=lambda *a, **k: None, debug=lambda *a, **k: None,
            info=lambda *a, **k: None, error=lambda *a, **k: None))
    return {
        "__name__": modname,
        "Any": object, "Callable": object, "Optional": object, "Dict": object,
        "Literal": object, "Mapping": object, "Protocol": object, "Tuple": object,
        "Awaitable": object, "Iterable": object, "Iterator": object, "Set": object,
        "List": object, "FrozenSet": object,
        "dataclass": __import__("dataclasses").dataclass,
        "field": __import__("dataclasses").field,
        "contextlib": __import__("contextlib"),
        "functools": __import__("functools"),
        "threading": types.SimpleNamespace(Lock=NoopLock, RLock=NoopLock),
        "logging": logging_double,
        "time": types.SimpleNamespace(monotonic=lambda: 0.0, time=lambda: 0.0),
        "math": math, "re": re, "json": json,
        "os": types.SimpleNamespace(environ={}, path=ntpath),
        "sys": sys,
        "io": __import__("io"),
        "Path": Path, "PurePosixPath": PurePosixPath, "PureWindowsPath": PureWindowsPath,
        "ntpath": ntpath,
    }


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


# ── module namespaces (built once) ──────────────────────────────────────────

NS_BA = load_ns(
    "agent/bedrock_adapter.py", base_seeds("quarry_r1_ba"),
    want_funcs=("strip_cache_points", "_without_cache_points",
                "cache_point_rejection_placement",
                "recover_from_cache_point_rejection",
                "resolve_bedrock_bearer_token"),
    require=("_CACHE_POINT_PATH_PATTERN",))

NS_SM = load_ns(
    "gateway/outbound/send.py", base_seeds("quarry_r1_sm"),
    want_funcs=("_bounded_send_error", "prepare_send_message_platforms"))

NS_SG = load_ns(
    "tools/skills_guard.py", base_seeds("quarry_r1_sg"),
    want_funcs=("_shell_write_re", "_prose_modify_re", "_content_contract_re"),
    require=("MODIFY_VERB_RE",))

NS_SP = load_ns(
    "gateway/session_persistence.py", base_seeds("quarry_r1_sp"),
    want_funcs=("_named_profile_for_key", "_profile_home_for_key",
                "_owner_key_for_session_id"))

NS_ST = load_ns(
    "gateway/session_transcript.py", base_seeds("quarry_r1_st"),
    want_funcs=("_get_transcript_drain_lock",))

NS_AS = load_ns(
    "hermes_cli/archive_safe.py", base_seeds("quarry_r1_as"),
    want_funcs=("normalize_archive_parts",))

NS_IR = load_ns(
    "hermes_cli/_install_repair.py", base_seeds("quarry_r1_ir"),
    want_funcs=("_normalize_windows_path", "_windows_user_path_entries",
                "_read_user_path_raw"))

_sz_seeds = base_seeds("quarry_r1_sz")
_sz_seeds["os"] = types.SimpleNamespace(environ={}, path=ntpath)
NS_SZ = load_ns(
    "gateway/scale_to_zero.py", _sz_seeds,
    want_funcs=("dashboard_client_heartbeat_path",
                "touch_dashboard_client_heartbeat", "dashboard_client_last_seen"),
    require=("DASHBOARD_CLIENT_HEARTBEAT_REL",))

NS_GW = load_ns(
    "hermes_cli/gateway_windows.py", base_seeds("quarry_r1_gw"),
    want_funcs=("_start_attestation_path",),
    require=("_START_ATTESTATION_RELATIVE",))

_fo_seeds = base_seeds("quarry_r1_fo")
_fo_seeds["os"] = types.SimpleNamespace(environ={}, path=ntpath)  # PIN ntpath
NS_FO = load_ns(
    "tools/file_operations_search.py", _fo_seeds,
    want_funcs=("_macos_protected_search_warning",))

NS_PXB = load_ns(
    "hermes_cli/proxy/adapters/base.py", base_seeds("quarry_r1_pxb"),
    want_funcs=("name", "display_name"))

NS_CO = load_ns(
    "hermes_cli/cli_output.py", base_seeds("quarry_r1_co"),
    want_funcs=("line_input",))

NS_DU = load_ns(
    "tools/desktop_ui.py", base_seeds("quarry_r1_du"),
    want_funcs=("user_enabled",))

NS_VA = load_ns(
    "agent/vertex_adapter.py", base_seeds("quarry_r1_va"),
    want_funcs=("has_explicit_vertex_config",))

NS_FS = load_ns(
    "tools/file_state.py", base_seeds("quarry_r1_fs"),
    want_funcs=("forget_task",))

NS_SW = load_ns(
    "gateway/shutdown_watchdog.py", base_seeds("quarry_r1_sw"),
    want_funcs=("get_loop_tick_socket_path", "_home"))

_ts_seeds = base_seeds("quarry_r1_ts")
NS_TS = load_ns(
    "tools/terminal_scope.py", _ts_seeds,
    want_funcs=("get_terminal_scope",),
    want_classes=("DeferredTerminalPolicy",))

# Real defs restored before every case of the same module (a prior flow case
# may have left a recorder bound to a sibling the next case calls for real).
REAL_BA = {nm: NS_BA[nm] for nm in (
    "strip_cache_points", "_without_cache_points", "cache_point_rejection_placement",
    "recover_from_cache_point_rejection", "resolve_bedrock_bearer_token")}
REAL_SZ = {nm: NS_SZ[nm] for nm in (
    "dashboard_client_heartbeat_path", "touch_dashboard_client_heartbeat",
    "dashboard_client_last_seen")}
REAL_SW = {nm: NS_SW[nm] for nm in ("get_loop_tick_socket_path", "_home")}
REAL_TS_RESOLVE = NS_TS["DeferredTerminalPolicy"].resolve


# ── recording seam factory ──────────────────────────────────────────────────

def make_rec(script, trace, armed):
    def rec(name, argb, make=None):
        def call(*a, **k):
            r = script.next(name)
            ev = {"ev": name, "args": argb(a, k), "ret": None}
            if isinstance(r, Raise):
                ev["ret"] = r.spec
                if armed[0]:
                    trace.append(ev)
                raise r.exc
            ev["ret"] = norm(r)
            if armed[0]:
                trace.append(ev)
            return make(r) if make else r
        return call
    return rec


def seam_import(modname, attr, import_seam, rec, call_seam, call_argb, make=None):
    """Model `from <modname> import <attr>`: the import itself is a seam
    (scripted ImportError fails it); the attribute is the call seam."""
    mod = types.ModuleType(modname)

    def mod_getattr(name):
        if name != attr:
            raise AttributeError(name)
        rec(import_seam, lambda a, k: {})()
        return rec(call_seam, call_argb, make=make)
    mod.__getattr__ = mod_getattr
    sys.modules[modname] = mod
    parent, _, child = modname.rpartition(".")
    if parent:
        setattr(pkg(parent), child, mod)
    return mod


def minus_io(work):
    work.pop("io", None)
    return work


def ns_obj(r):
    return types.SimpleNamespace(**r) if isinstance(r, dict) else r


def path_obj(r):
    return Path(r) if isinstance(r, str) else r


class HandleDouble:
    """winreg key handle: silent context manager around the scripted value."""
    def __init__(self, raw):
        self.raw = raw

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def winreg_double(rec):
    fired = [False]
    mod = types.ModuleType("winreg")

    def ga(name):
        if not fired[0]:
            fired[0] = True
            rec("import_winreg", lambda a, k: {})()
        if name == "HKEY_CURRENT_USER":
            return "HKEY_CURRENT_USER"
        if name == "REG_EXPAND_SZ":
            return 2
        if name == "OpenKey":
            return rec("winreg.OpenKey", lambda a, k: {"hive": a[0], "key": a[1]},
                       make=HandleDouble)
        if name == "QueryValueEx":
            return rec("winreg.QueryValueEx",
                       lambda a, k: {"key": a[0].raw if isinstance(a[0], HandleDouble) else a[0],
                                     "name": a[1]})
        raise AttributeError(name)
    mod.__getattr__ = ga
    sys.modules["winreg"] = mod
    return mod


def exc_arg(x):
    return str(x) if isinstance(x, BaseException) else x


# ── per-function drivers ────────────────────────────────────────────────────

def d_resolve_bedrock_bearer_token(work, trace, script):
    NS_BA.update(REAL_BA)
    rec = make_rec(script, trace, [True])
    env = work.get("env")
    if env is not None:
        return NS_BA["resolve_bedrock_bearer_token"](env)
    seam_import("hermes_constants", "get_hermes_home_override",
                "import_get_hermes_home_override", rec,
                "get_hermes_home_override", lambda a, k: {})
    seam_import("agent.secret_scope", "get_secret",
                "import_get_secret", rec, "get_secret",
                lambda a, k: {"key": a[0], "default": a[1]})
    NS_BA["os"] = types.SimpleNamespace(environ=types.SimpleNamespace(
        get=rec("environ_get", lambda a, k: {"key": a[0], "default": a[1]})))
    return NS_BA["resolve_bedrock_bearer_token"](None)


def d_cache_point_rejection_placement(work, trace, script):
    NS_BA.update(REAL_BA)
    spec = work.get("exc") or {"type": "Exception", "message": ""}
    return NS_BA["cache_point_rejection_placement"](build_exc(spec))


def d_strip_cache_points(work, trace, script):
    NS_BA.update(REAL_BA)
    kwargs = work.get("kwargs")
    res = NS_BA["strip_cache_points"](kwargs, work.get("placement", ""))
    return {"same": res is kwargs, "value": res}


def d_recover_from_cache_point_rejection(work, trace, script):
    NS_BA.update(REAL_BA)
    rec = make_rec(script, trace, [True])
    NS_BA["note_cache_point_rejection"] = rec(
        "note_cache_point_rejection",
        lambda a, k: {"model_id": a[0], "placement": a[1]})
    NS_BA["logger"] = types.SimpleNamespace(
        warning=rec("logger.warning", lambda a, k: {"msg": a[0], "args": list(a[1:])}))
    spec = work.get("exc") or {"type": "Exception", "message": ""}
    return NS_BA["recover_from_cache_point_rejection"](build_exc(spec), work.get("kwargs"))


def d_prepare_send_message_platforms(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("hermes_cli.plugins", "discover_plugins",
                "import_discover_plugins", rec, "discover_plugins", lambda a, k: {})
    return NS_SM["prepare_send_message_platforms"]()


def d_bounded_send_error(work, trace, script):
    return NS_SM["_bounded_send_error"](work.get("detail"), work.get("max_chars", 900))


def d_shell_write_re(work, trace, script):
    return NS_SG["_shell_write_re"](work.get("file_alt", ""))


def d_prose_modify_re(work, trace, script):
    return NS_SG["_prose_modify_re"](work.get("file_alt", ""))


def d_content_contract_re(work, trace, script):
    return NS_SG["_content_contract_re"](work.get("file_alt", ""))


def d_named_profile_for_key(work, trace, script):
    rec = make_rec(script, trace, [True])
    s = work.get("self") or {}
    config = s.get("config")
    cfg = types.SimpleNamespace(**config) if isinstance(config, dict) else config
    pfk = rec("_profile_from_session_key", lambda a, k: {"session_key": a[0]})
    dbl = types.SimpleNamespace(config=cfg, _profile_from_session_key=pfk)
    return NS_SP["_named_profile_for_key"](dbl, work.get("session_key"))


def d_profile_home_for_key(work, trace, script):
    rec = make_rec(script, trace, [True])
    s = work["self"]
    cache = s["_profile_home_cache"]  # same dict object — mutations land in work
    npfk = rec("_named_profile_for_key", lambda a, k: {"session_key": a[0]})
    dbl = types.SimpleNamespace(_profile_home_cache=cache, _named_profile_for_key=npfk)
    # from hermes_cli.profiles import get_profile_dir, profile_exists — ONE
    # import event on first attribute access, then per-name call seams.
    fired = [False]
    mod = types.ModuleType("hermes_cli.profiles")

    def ga(name):
        if name not in ("get_profile_dir", "profile_exists"):
            raise AttributeError(name)
        if not fired[0]:
            fired[0] = True
            rec("import_profiles", lambda a, k: {})()
        if name == "profile_exists":
            return rec("profile_exists", lambda a, k: {"profile": a[0]})
        return rec("get_profile_dir", lambda a, k: {"profile": a[0]})
    mod.__getattr__ = ga
    sys.modules["hermes_cli.profiles"] = mod
    setattr(pkg("hermes_cli"), "profiles", mod)
    NS_SP["logger"] = types.SimpleNamespace(
        debug=rec("logger.debug",
                  lambda a, k: {"msg": a[0], "args": [a[1], exc_arg(a[2])]}))
    return NS_SP["_profile_home_for_key"](dbl, work.get("session_key"))


def d_owner_key_for_session_id(work, trace, script):
    s = work.get("self") or {}
    entries = s.get("_entries")
    if isinstance(entries, dict):
        entries = {k: (types.SimpleNamespace(**v) if isinstance(v, dict) else v)
                   for k, v in entries.items()}
    dbl = types.SimpleNamespace(_entries=entries,
                                _session_owner_hints=s.get("_session_owner_hints"))
    return NS_SP["_owner_key_for_session_id"](dbl, work.get("session_id"))


def d_get_transcript_drain_lock(work, trace, script):
    rec = make_rec(script, trace, [True])
    dbl = types.SimpleNamespace(
        _lazy=rec("_lazy", lambda a, k: {"name": a[0], "factory": "threading.RLock"}))
    return NS_ST["_get_transcript_drain_lock"](dbl)


def d_normalize_archive_parts(work, trace, script):
    return NS_AS["normalize_archive_parts"](work.get("member_name"))


def d_normalize_windows_path(work, trace, script):
    return NS_IR["_normalize_windows_path"](work.get("value"))


def d_windows_user_path_entries(work, trace, script):
    rec = make_rec(script, trace, [True])
    winreg_double(rec)
    NS_IR["os"] = types.SimpleNamespace(
        path=types.SimpleNamespace(
            expandvars=rec("os.path.expandvars", lambda a, k: {"value": a[0]})),
        environ=types.SimpleNamespace(
            get=rec("os.environ.get", lambda a, k: {"key": a[0], "default": a[1]})))
    return NS_IR["_windows_user_path_entries"]()


def d_read_user_path_raw(work, trace, script):
    rec = make_rec(script, trace, [True])
    winreg_double(rec)
    return NS_IR["_read_user_path_raw"]()


def d_dashboard_client_heartbeat_path(work, trace, script):
    NS_SZ.update(REAL_SZ)
    rec = make_rec(script, trace, [True])
    home = work.get("hermes_home")
    if home is None:
        seam_import("hermes_constants", "get_process_hermes_home",
                    "import_get_process_hermes_home", rec,
                    "get_process_hermes_home", lambda a, k: {})
    return NS_SZ["dashboard_client_heartbeat_path"](home)


def d_touch_dashboard_client_heartbeat(work, trace, script):
    NS_SZ.update(REAL_SZ)
    rec = make_rec(script, trace, [True])
    NS_SZ["dashboard_client_heartbeat_path"] = rec(
        "dashboard_client_heartbeat_path", lambda a, k: {}, make=path_obj)

    class FileDouble:
        def __init__(self, raw):
            self.raw = raw

        def close(self):
            return rec("close", lambda a, k: {})()
    NS_SZ["os"] = types.SimpleNamespace(
        path=types.SimpleNamespace(
            dirname=rec("os.path.dirname", lambda a, k: {"path": posix(a[0])})),
        makedirs=rec("os.makedirs",
                     lambda a, k: {"path": posix(a[0]), "exist_ok": k.get("exist_ok")}),
        utime=rec("os.utime", lambda a, k: {"path": posix(a[0]), "times": a[1]}))
    NS_SZ["open"] = rec("open",
                        lambda a, k: {"path": posix(a[0]), "mode": a[1],
                                      "encoding": k.get("encoding")},
                        make=FileDouble)
    NS_SZ["logger"] = types.SimpleNamespace(
        debug=rec("logger.debug", lambda a, k: {"msg": a[0], "exc_info": k.get("exc_info")}))
    return NS_SZ["touch_dashboard_client_heartbeat"](work.get("path"))


def d_dashboard_client_last_seen(work, trace, script):
    NS_SZ.update(REAL_SZ)
    rec = make_rec(script, trace, [True])
    NS_SZ["dashboard_client_heartbeat_path"] = rec(
        "dashboard_client_heartbeat_path", lambda a, k: {}, make=path_obj)
    NS_SZ["time"] = types.SimpleNamespace(time=rec("time.time", lambda a, k: {}))
    NS_SZ["os"] = types.SimpleNamespace(
        stat=rec("os.stat", lambda a, k: {"path": posix(a[0])}, make=ns_obj))
    return NS_SZ["dashboard_client_last_seen"](work.get("path"), now=work.get("now"))


def d_start_attestation_path(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_GW["_hermes_home"] = rec("_hermes_home", lambda a, k: {}, make=path_obj)
    home = work.get("home")
    if isinstance(home, str):
        home = Path(home)
    return NS_GW["_start_attestation_path"](home)


def d_macos_protected_search_warning(work, trace, script):
    # Live def is a staticmethod-shaped method: (paths) only, no self.
    return NS_FO["_macos_protected_search_warning"](work.get("paths"))


def d_pxb_name(work, trace, script):
    return NS_PXB["name"](object())


def d_pxb_display_name(work, trace, script):
    return NS_PXB["display_name"](object())


def d_line_input(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_CO["sys"] = types.SimpleNamespace(
        stdin=types.SimpleNamespace(isatty=rec("stdin.isatty", lambda a, k: {})),
        stdout=types.SimpleNamespace(isatty=rec("stdout.isatty", lambda a, k: {})))
    NS_CO["input"] = rec("input", lambda a, k: {"prompt": a[0]})
    seam_import("prompt_toolkit", "prompt",
                "import_prompt_toolkit_prompt", rec,
                "prompt_toolkit_prompt", lambda a, k: {"prompt": a[0]})
    # ANSI is an identity wrapper; only its IMPORT is a seam event.
    ansi_mod = types.ModuleType("prompt_toolkit.formatted_text")

    def ansi_ga(name):
        if name != "ANSI":
            raise AttributeError(name)
        rec("import_ANSI", lambda a, k: {})()
        return lambda s: s
    ansi_mod.__getattr__ = ansi_ga
    sys.modules["prompt_toolkit.formatted_text"] = ansi_mod
    setattr(pkg("prompt_toolkit"), "formatted_text", ansi_mod)
    return NS_CO["line_input"](work.get("prompt_text", ""))


def d_user_enabled(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("hermes_cli.config", "load_config_readonly",
                "import_load_config_readonly", rec,
                "load_config_readonly", lambda a, k: {})
    return NS_DU["user_enabled"](work.get("setting"), work.get("default"))


def d_has_explicit_vertex_config(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_VA["_resolve_project_override"] = rec("_resolve_project_override", lambda a, k: {})
    NS_VA["_get_secret"] = rec("_get_secret", lambda a, k: {"key": a[0]})
    NS_VA["os"] = types.SimpleNamespace(
        path=types.SimpleNamespace(isfile=rec("os.path.isfile", lambda a, k: {"path": a[0]})),
        access=rec("os.access", lambda a, k: {"path": a[0], "mode": a[1]}),
        R_OK=4)
    return NS_VA["has_explicit_vertex_config"]()


def d_forget_task(work, trace, script):
    s = work["self"]
    dbl = types.SimpleNamespace(_reads=s["_reads"], _last_writer=s["_last_writer"],
                                _state_lock=NoopLock())
    NS_FS["forget_task"](dbl, work.get("task_id"))
    return None


def d_get_loop_tick_socket_path(work, trace, script):
    NS_SW.update(REAL_SW)
    rec = make_rec(script, trace, [True])
    NS_SW["os"] = types.SimpleNamespace(getpid=rec("os.getpid", lambda a, k: {}))
    NS_SW["_process_hermes_home"] = rec("_process_hermes_home", lambda a, k: {},
                                        make=path_obj)
    home = work.get("home")
    if isinstance(home, str):
        home = Path(home)
    return NS_SW["get_loop_tick_socket_path"](home, work.get("pid"))


def d_get_terminal_scope(work, trace, script):
    rec = make_rec(script, trace, [True])
    Cls = NS_TS["DeferredTerminalPolicy"]

    def make_state(r):
        if not isinstance(r, dict):
            return r
        if "terminal" not in r:
            return types.SimpleNamespace()  # attribute absent -> AttributeError
        t = r["terminal"]
        if isinstance(t, dict) and t.get("__type__") == "DeferredTerminalPolicy":
            t = Cls(build=lambda: None)
        return types.SimpleNamespace(terminal=t)
    seam_import("hermes_scope", "current_state", "import_current_state", rec,
                "current_state", lambda a, k: {}, make=make_state)
    Cls.resolve = lambda self: rec("resolve", lambda a, k: {})()
    try:
        return NS_TS["get_terminal_scope"]()
    finally:
        Cls.resolve = REAL_TS_RESOLVE


DRIVERS = {
    "resolve_bedrock_bearer_token": d_resolve_bedrock_bearer_token,
    "cache_point_rejection_placement": d_cache_point_rejection_placement,
    "strip_cache_points": d_strip_cache_points,
    "recover_from_cache_point_rejection": d_recover_from_cache_point_rejection,
    "prepare_send_message_platforms": d_prepare_send_message_platforms,
    "bounded_send_error": d_bounded_send_error,
    "shell_write_re": d_shell_write_re,
    "prose_modify_re": d_prose_modify_re,
    "content_contract_re": d_content_contract_re,
    "named_profile_for_key": d_named_profile_for_key,
    "profile_home_for_key": d_profile_home_for_key,
    "owner_key_for_session_id": d_owner_key_for_session_id,
    "get_transcript_drain_lock": d_get_transcript_drain_lock,
    "normalize_archive_parts": d_normalize_archive_parts,
    "normalize_windows_path": d_normalize_windows_path,
    "windows_user_path_entries": d_windows_user_path_entries,
    "read_user_path_raw": d_read_user_path_raw,
    "dashboard_client_heartbeat_path": d_dashboard_client_heartbeat_path,
    "touch_dashboard_client_heartbeat": d_touch_dashboard_client_heartbeat,
    "dashboard_client_last_seen": d_dashboard_client_last_seen,
    "start_attestation_path": d_start_attestation_path,
    "macos_protected_search_warning": d_macos_protected_search_warning,
    "pxb_name": d_pxb_name,
    "pxb_display_name": d_pxb_display_name,
    "line_input": d_line_input,
    "user_enabled": d_user_enabled,
    "has_explicit_vertex_config": d_has_explicit_vertex_config,
    "forget_task": d_forget_task,
    "get_loop_tick_socket_path": d_get_loop_tick_socket_path,
    "get_terminal_scope": d_get_terminal_scope,
}


def run_case(driver, inp):
    trace = []
    work = copy.deepcopy(inp)
    script = Script(work.get("io"))
    try:
        ret = driver(work, trace, script)
        minus_io(work)
        return {"ret": norm(ret), "trace": norm(trace), "state": norm(work)}
    except BaseException as e:  # SystemExit included — fail closed per case
        spec = {"__raise__": type(e).__name__, "message": str(e)}
        minus_io(work)
        return {"ret": spec, "trace": norm(trace), "state": norm(work)}


def main():
    n = 0
    mismatches = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as e:
            mismatches += 1
            print(f"BAD JSON from harness: {e}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        driver = DRIVERS.get(func)
        if driver is None:
            mismatches += 1
            print(f"UNKNOWN func {func!r}")
            continue
        if not isinstance(got, dict) or "error" in got:
            mismatches += 1
            print(f"HARNESS ERROR {func}: {got}")
            continue
        try:
            want = run_case(driver, inp)
        except BaseException as e:
            mismatches += 1
            print(f"ORACLE DRIVER FAILURE {func}: {type(e).__name__}: {e}")
            continue
        if got != want:
            mismatches += 1
            if shown < 8:
                shown += 1
                print(f"MISMATCH {func} case#{n}")
                for key in ("ret", "trace", "state"):
                    if got.get(key) != want.get(key):
                        print(f"  {key} C  : {json.dumps(got.get(key), ensure_ascii=False)[:600]}")
                        print(f"  {key} PY : {json.dumps(want.get(key), ensure_ascii=False)[:600]}")
    print(f"ORACLE rest_revive1: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
