#!/usr/bin/env python3
"""oracle_rest_revive3.py — differential oracle for the rest group-3 revival.

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the LIVE
quarry defs, seeds module constants (Assign + AnnAssign), drives each live
function under the SAME scripted seam results the C harness consumed, and
compares ordered trace + ret + post-state. BaseException is caught per case
(SystemExit included); missing/exhausted scripts fail closed on both sides.

Seam conventions (identical on both sides):
- In-function `from X import a, b` becomes "import_<a>"/"import_<b>" seams
  (recorded on attribute access, in import order); the callables are the
  "<a>"/"<b>" seams. A scripted ImportError models a failed import.
- Plain `import shutil` binds a fake sys.modules entry silently (no event).
- Path-returning helpers convert the scripted string list via `make` for the
  live call; the trace records the RAW scripted value (as the C seam does).
- Integral JSON numbers model Python ints; arrays model lists/tuples.
- dataclass results (HFFileGroup/HFModelHit) normalize via asdict.

Gate:
  bash tests/run_one_oracle.sh rest_revive3 oracle_rest_revive3.py "" "lib/libjson/json.o"
"""
import ast
import builtins
import contextlib
import copy
import dataclasses
import __future__
import json
import math
import os
import re
import sys
import types
import urllib.parse
import posixpath
from pathlib import Path, PurePosixPath, PurePath

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

FUT = __future__.annotations.compiler_flag
MISSING = object()


def norm(x):
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
    if isinstance(x, PurePath):
        return x.as_posix()
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return norm(dataclasses.asdict(x))
    return x


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


class Script:
    def __init__(self, io_map):
        self.io = io_map or {}
        self.cursors = {}

    def next(self, name):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            raise RuntimeError(f"no script for {name}")
        if isinstance(entry, list):
            i = self.cursors.get(name, 0)
            if i >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
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


def load_ns(relpath, seeds, want_funcs=(), want_classes=()):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_r3_" + re.sub(r"\W", "_", relpath)
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

    def seed_consts():
        for n in consts:
            try:
                exec(compile_node(n, relpath), ns)
            except Exception:
                pass

    seed_consts()
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name in want_classes:
            exec(compile_node(n, relpath), ns)
    for n in ast.walk(tree):
        if isinstance(n, ast.FunctionDef) and n.name in want_funcs:
            n2 = copy.copy(n)
            n2.decorator_list = []
            exec(compile_node(n2, relpath), ns)
    seed_consts()
    for name in list(want_funcs) + list(want_classes):
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
        "Awaitable": object, "Iterable": object, "Set": object, "List": object,
        "dataclass": dataclasses.dataclass, "field": dataclasses.field,
        "contextlib": contextlib, "functools": __import__("functools"),
        "threading": types.SimpleNamespace(Lock=NoopLock),
        "logging": logging_double,
        "time": types.SimpleNamespace(monotonic=lambda: 0.0, time=lambda: 0.0),
        "math": math, "re": re, "json": json,
        "os": types.SimpleNamespace(environ={}),
        "sys": sys,
        "Path": PurePosixPath,
        "urllib": urllib,
    }


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


# ── module namespaces ───────────────────────────────────────────────────────

NS_BASE = load_ns("gateway/platforms/base.py", base_seeds("quarry_r3_base"),
                  want_funcs=("_translate_docker_container_media_path", "transcode_to_ogg_opus"))
NS_DMP = load_ns("gateway/platforms/docker_media_paths.py", base_seeds("quarry_r3_dmp"),
                 want_funcs=("_docker_sandbox_dir_candidates", "_default_docker_workspace_host_roots"))
NS_SL = load_ns("tools/skill_ledger.py", base_seeds("quarry_r3_sl"),
                want_funcs=("fill_snapshot_from_curator_backup", "package_prefixes", "_package_rel",
                            "_strip_archive_timestamp", "_rel_posix", "_is_within", "_skill_md_parents",
                            "_skills_dir"))
NS_HB = load_ns("hermes_cli/local_runtime/hf_browse.py", base_seeds("quarry_r3_hb"),
                want_funcs=("repo_files", "search_models", "rough_fit", "_quant_label", "priced_repo_files"),
                want_classes=("HFFileGroup", "HFModelHit"))
NS_BR = load_ns("tools/bot_relay.py", base_seeds("quarry_r3_br"), want_funcs=("_hermes_cli",))
NS_KW = load_ns("gateway/kanban_watchers_notifier.py", base_seeds("quarry_r3_kw"),
                want_funcs=("_wake_scope_id", "_safe_review_reason"))

NS_HB["replace"] = dataclasses.replace
NS_SL["os"] = types.SimpleNamespace(path=posixpath)  # POSIX os.path.normpath for _rel_posix


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


def seam_import_multi(modname, attrs, rec):
    """Model `from <modname> import a, b, ...`. attrs: name -> (call_seam,
    argb, make). Each attribute access records an "import_<name>" seam (in
    import order) and yields the call seam."""
    mod = types.ModuleType(modname)

    def mod_getattr(name):
        if name not in attrs:
            raise AttributeError(name)
        rec("import_" + name, lambda a, k: {})()
        call_seam, argb, make = attrs[name]
        return rec(call_seam, argb, make=make)

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


def paths_make(r):
    return [PurePosixPath(x) for x in r] if isinstance(r, list) else r


def pairs_make(r):
    return [(PurePosixPath(h), PurePosixPath(c)) for h, c in r] if isinstance(r, list) else r


# ── per-function drivers ────────────────────────────────────────────────────

def d_translate(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_BASE["_parse_docker_volume_mounts"] = rec("_parse_docker_volume_mounts", lambda a, k: {}, make=pairs_make)
    NS_BASE["_cache_dir_container_mounts"] = rec("_cache_dir_container_mounts", lambda a, k: {}, make=pairs_make)
    NS_BASE["_resolve_path"] = rec("_resolve_path",
                                   lambda a, k: {"path": str(a[0]), "strict": k.get("strict", False)},
                                   make=lambda r: PurePosixPath(r) if r is not None else None)
    NS_BASE["_path_is_within"] = rec("_path_is_within", lambda a, k: {"path": str(a[0]), "root": str(a[1])})
    NS_BASE["_warn_unresolved_docker_media"] = rec(
        "warn_unresolved_docker_media",
        lambda a, k: {"candidate": str(a[0]), "session_key": a[1], "reason": a[2], "quiet": a[3]})
    seam_import_multi("tools.terminal_tool",
                      {"_ensure_terminal_env_bridged": ("_ensure_terminal_env_bridged", lambda a, k: {}, None)}, rec)
    seam_import_multi("gateway.platforms.docker_media_paths", {
        "_default_docker_workspace_host_roots": (
            "_default_docker_workspace_host_roots",
            lambda a, k: {"session_key": a[0], "workspace_key": a[1], "workspace_cwd": a[2]}, paths_make),
        "_docker_persistent_sandbox_roots": (
            "_docker_persistent_sandbox_roots",
            lambda a, k: {"session_key": a[0], "leaf": a[1], "workspace_key": a[2]}, paths_make),
    }, rec)
    candidate = PurePosixPath(work.get("candidate", ""))
    return NS_BASE["_translate_docker_container_media_path"](
        candidate, work.get("session_key", ""), work.get("workspace_key", ""),
        quiet=work.get("quiet", False), workspace_cwd=work.get("workspace_cwd", ""))


def d_transcode(work, trace, script):
    rec = make_rec(script, trace, [True])
    sys.modules["shutil"] = types.SimpleNamespace(which=rec("shutil.which", lambda a, k: {"name": a[0]}))
    pkg("shutil")
    NS_BASE["tempfile"] = types.SimpleNamespace(mkstemp=rec(
        "tempfile.mkstemp", lambda a, k: {"prefix": k.get("prefix"), "suffix": k.get("suffix")},
        make=lambda r: (r[0], r[1])))
    NS_BASE["os"] = types.SimpleNamespace(
        close=rec("os.close", lambda a, k: {"fd": a[0]}),
        path=types.SimpleNamespace(
            abspath=rec("os.path.abspath", lambda a, k: {"path": a[0]}),
            getsize=rec("os.path.getsize", lambda a, k: {"path": a[0]})),
        replace=rec("os.replace", lambda a, k: {"src": a[0], "dst": a[1]}),
        unlink=rec("os.unlink", lambda a, k: {"path": a[0]}))
    NS_BASE["subprocess"] = types.SimpleNamespace(
        run=rec("subprocess.run", lambda a, k: {"argv": a[0], **k}, make=ns_obj), DEVNULL=-3)
    return NS_BASE["transcode_to_ogg_opus"](
        work.get("path", ""), bitrate=work.get("bitrate", "32k"),
        timeout=work.get("timeout", 60), output_path=work.get("output_path"))


def d_sandbox_candidates(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_DMP["_scope"] = lambda: (rec("tenv", lambda a, k: {"key": a[0], "default": a[1]}), None)
    NS_DMP["_cwd_mount_enabled"] = rec("cwd_mount_enabled", lambda a, k: {})
    seam_import_multi("tools.environments.path_utils",
                      {"sanitize_task_id_for_path": ("sanitize_task_id_for_path", lambda a, k: {"value": a[0]}, None)}, rec)
    seam_import_multi("tools.terminal_tool_config", {
        "home_profile_name": ("home_profile_name", lambda a, k: {}, None),
        "profile_container_key": ("profile_container_key", lambda a, k: {"name": a[0]}, None),
    }, rec)
    return NS_DMP["_docker_sandbox_dir_candidates"](work.get("session_key", ""), work.get("workspace_key", ""))


def d_default_roots(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_DMP["_scope"] = lambda: (rec("tenv", lambda a, k: {"key": a[0], "default": a[1]}),
                                rec("persistent", lambda a, k: {}))
    NS_DMP["_cwd_mount_enabled"] = rec("cwd_mount_enabled", lambda a, k: {})
    NS_DMP["_mounted_workspace_cwd"] = rec("_mounted_workspace_cwd", lambda a, k: {}, make=paths_make)
    NS_DMP["_shared_cwd_mount_guess"] = rec("_shared_cwd_mount_guess", lambda a, k: {"workspace_key": a[0]}, make=paths_make)
    NS_DMP["_session_mounted_workspace"] = rec(
        "_session_mounted_workspace", lambda a, k: {"workspace_key": a[0], "workspace_cwd": a[1]}, make=paths_make)
    NS_DMP["_docker_persistent_sandbox_roots"] = rec(
        "_docker_persistent_sandbox_roots",
        lambda a, k: {"session_key": a[0], "leaf": a[1], "workspace_key": a[2]}, make=paths_make)
    return NS_DMP["_default_docker_workspace_host_roots"](
        work.get("session_key", ""), work.get("workspace_key", ""), work.get("workspace_cwd", ""))


def _sl_seams(rec):
    NS_SL["get_hermes_home"] = rec("get_hermes_home", lambda a, k: {},
                                   make=lambda r: PurePosixPath(r) if r is not None else None)
    NS_SL["_read_package_files_from_latest_backup"] = rec(
        "_read_package_files_from_latest_backup", lambda a, k: {"prefixes": a[0]})
    NS_SL["_store_blob"] = rec("_store_blob", lambda a, k: {"data": a[0]})


def d_fill_snapshot(work, trace, script):
    rec = make_rec(script, trace, [True])
    _sl_seams(rec)
    root = PurePosixPath(work["root"]) if work.get("root") is not None else None
    return NS_SL["fill_snapshot_from_curator_backup"](root, work.get("existing"), skill=work.get("skill"))


def d_package_prefixes(work, trace, script):
    rec = make_rec(script, trace, [True])
    _sl_seams(rec)
    root = PurePosixPath(work["root"]) if work.get("root") is not None else None
    return NS_SL["package_prefixes"](root, work.get("skill"), work.get("before"))


def d_package_rel(work, trace, script):
    rec = make_rec(script, trace, [True])
    _sl_seams(rec)
    return NS_SL["_package_rel"](PurePosixPath(work.get("root", "")))


def d_strip_ts(work, trace, script):
    return NS_SL["_strip_archive_timestamp"](work.get("name", ""))


def d_repo_files(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_HB["_get_json"] = rec("_get_json", lambda a, k: {"url": a[0]})
    return NS_HB["repo_files"](work.get("repo", ""))


def d_search_models(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_HB["_get_json"] = rec("_get_json", lambda a, k: {"url": a[0]})
    return NS_HB["search_models"](work.get("query", ""), work.get("limit", 20))


def d_rough_fit(work, trace, script):
    budget = types.SimpleNamespace(**(work.get("budget") or {}))
    return NS_HB["rough_fit"](work.get("total_bytes", 0), budget)


def d_quant_label(work, trace, script):
    return NS_HB["_quant_label"](work.get("filename", ""))


def d_priced_repo_files(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_HB["_get_json"] = rec("_get_json", lambda a, k: {"url": a[0]})
    budget = types.SimpleNamespace(**(work.get("budget") or {}))
    return NS_HB["priced_repo_files"](work.get("repo", ""), budget)


class _SysDouble:
    def __init__(self, rec):
        self._rec = rec

    def __getattr__(self, name):
        if name == "platform":
            return self._rec("sys_platform", lambda a, k: {})()
        if name == "executable":
            return self._rec("sys_executable", lambda a, k: {})()
        raise AttributeError(name)


def d_hermes_cli(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_BR["sys"] = _SysDouble(rec)
    NS_BR["shutil"] = types.SimpleNamespace(which=rec("shutil.which", lambda a, k: {"name": a[0]}))
    seam_import_multi("hermes_cli._startup_fast",
                      {"project_root_str": ("project_root_str", lambda a, k: {}, None)}, rec)

    class _R3Path(PurePosixPath):
        def is_file(self):
            return rec("is_file", lambda a, k: {"path": str(self)})()

    NS_BR["Path"] = _R3Path
    return NS_BR["_hermes_cli"]()


def d_wake_scope_id(work, trace, script):
    rec = make_rec(script, trace, [True])
    a = work.get("adapter") or {}
    kind = a.get("resolver", "absent") if isinstance(a, dict) else "absent"
    if kind == "callable":
        adapter = types.SimpleNamespace(scope_id_for_chat=rec("scope_id_for_chat", lambda a_, k: {"chat_id": a_[0]}))
    elif kind == "noncallable":
        adapter = types.SimpleNamespace(scope_id_for_chat=5)
    else:
        adapter = types.SimpleNamespace()
    return NS_KW["_wake_scope_id"](adapter, work.get("sub") or {})


def d_safe_review_reason(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import_multi("agent.redact", {
        "redact_sensitive_text": ("redact_sensitive_text",
                                  lambda a, k: {"text": a[0], "force": k.get("force"),
                                                "redact_url_credentials": k.get("redact_url_credentials")}, None),
    }, rec)
    return NS_KW["_safe_review_reason"](work.get("value"), work.get("limit", 160))


DRIVERS = {
    "translate_docker_container_media_path": d_translate,
    "transcode_to_ogg_opus": d_transcode,
    "docker_sandbox_dir_candidates": d_sandbox_candidates,
    "default_docker_workspace_host_roots": d_default_roots,
    "fill_snapshot_from_curator_backup": d_fill_snapshot,
    "package_prefixes": d_package_prefixes,
    "package_rel": d_package_rel,
    "strip_archive_timestamp": d_strip_ts,
    "repo_files": d_repo_files,
    "search_models": d_search_models,
    "rough_fit": d_rough_fit,
    "quant_label": d_quant_label,
    "priced_repo_files": d_priced_repo_files,
    "_hermes_cli": d_hermes_cli,
    "wake_scope_id": d_wake_scope_id,
    "safe_review_reason": d_safe_review_reason,
}


def run_case(driver, inp):
    trace = []
    work = copy.deepcopy(inp)
    script = Script(work.get("io"))
    try:
        ret = driver(work, trace, script)
        minus_io(work)
        return {"ret": norm(ret), "trace": norm(trace), "state": norm(work)}
    except BaseException as e:
        spec = {"__raise__": type(e).__name__, "message": str(e)}
        if isinstance(e, SystemExit):
            spec["code"] = e.code
        minus_io(work)
        return {"ret": spec, "trace": norm(trace), "state": norm(work)}


def main():
    sys.stdin.reconfigure(encoding="utf-8")
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
    print(f"ORACLE rest_revive3: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()