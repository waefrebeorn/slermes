#!/usr/bin/env python3
"""Oracle for backup_flow3 -- the 26 STILL-UNCLAIMED heavy path/filesystem
FLOW cores of hermes_cli/backup.py beyond backup_flow2.

A CRLF-aware PoP grep at authoring time confirmed backup.c / backup_gaps.c /
backup_flow2.c claim 44 of backup.py's functions; the 26 driven here carry NO
PoP marker and are therefore unclaimed.

Drives the LIVE defs AST-extracted from the quarry backup.py (no heavy import:
backup.py imports `persistence`, absent from the oracle venv). Every effectful
collaborator is replaced by a recording stub returning the SAME scripted result
the C harness feeds its universal seam: a MISSING key fail-closes with
RuntimeError on both sides, a present JSON null is a valid None, and an array
script yields one result PER CALL (last repeats). Scripted {"__raise__":cls}
raises a REAL builtin of that class (full py3.14 parent hierarchy, including
the BaseException trio), so KeyboardInterrupt/SystemExit/GeneratorExit pass
through every `except Exception` exactly as in the C port.

Input: stdin JSON lines from t_port_backup_flow3:
    {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}
Summary: ORACLE backup_flow3: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses defs or zero cases were compared.

--self-test drives every fixture LIVE (no C) and reports binding/script gaps.
--shadow  diffs a literal Python MIRROR of the C port against the LIVE drive
          over every fixture (the shadow-diff; must print 0 diffs).
"""
import ast
import copy
import json
import os
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

from pathlib import Path as _Path

_REPO = _Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "backup.py")

# harness func -> live python name (all 26 unclaimed defs)
PY = {
    "is_non_regular_path": "_is_non_regular_path",
    "is_link_path": "_is_link_path",
    "snapshot_db": "_snapshot_db",
    "discard_failed_zip_members": "_discard_failed_zip_members",
    "zip_sqlite_snapshot": "_zip_sqlite_snapshot",
    "write_zip_entries": "_write_zip_entries",
    "resolve_backup_output_path": "_resolve_backup_output_path",
    "collect_external_entries": "_collect_external_entries",
    "run_import": "run_import",
    "import_members": "_import_members",
    "import_external_member": "_import_external_member",
    "import_members_exclusive": "_import_members_exclusive",
    "revive_gateway_after_import": "_revive_gateway_after_import",
    "newest_first": "_newest_first",
    "prune_oldest": "_prune_oldest",
    "create_prefixed_full_backup": "_create_prefixed_full_backup",
    "too_large": "_too_large",
    "is_trusted_root_auth_alias": "_is_trusted_root_auth_alias",
    "quick_snapshot_dir": "quick_snapshot_dir",
    "restore_quick_snapshot_exclusive": "_restore_quick_snapshot_exclusive",
    "entry_paths": "_entry_paths",
    "restore_config_model_settings_if_rewritten": "restore_config_model_settings_if_rewritten",
    "db_failure": "_db_failure",
    "capped_errors": "_capped_errors",
    "publish_path": "_publish_path",
    "progress": "_progress",
}
WANTED = set(PY.values())
# nested defs that may be exec'd straight into the namespace (free vars bind
# as globals); _publish_path is transcribed because it uses `nonlocal`.
NESTED_OK = {"_progress", "_too_large", "_db_failure", "_capped_errors", "_entry_paths"}

CUR = {"trace": [], "scripts": {}, "idx": {}}
NS = {}


# ---- seam recorder ----

def seam(name, args):
    args = copy.deepcopy({k: v for k, v in args.items() if v is not None})
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, dict) and "__arr__" in sc:
            r = sc["__arr__"]
        elif isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": copy.deepcopy(r)})
    return maybe_raise(r)


# Full builtin exception hierarchy (py3.14) + backup.py custom classes.
_zlib_error = type("zlib.error", (Exception,), {})
_lzma_error = type("LZMAError", (Exception,), {})
_bad_zip = type("BadZipFile", (Exception,), {})
_sqlite_err = type("_SQLiteSnapshotError", (Exception,), {})

EXC_MAP = {
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "FileExistsError": FileExistsError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "TimeoutError": TimeoutError, "BlockingIOError": BlockingIOError,
    "ProcessLookupError": ProcessLookupError, "ChildProcessError": ChildProcessError,
    "InterruptedError": InterruptedError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
    "ValueError": ValueError, "TypeError": TypeError, "KeyError": KeyError,
    "AttributeError": AttributeError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "IndexError": IndexError, "LookupError": LookupError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeError": UnicodeError, "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "AssertionError": AssertionError, "StopIteration": StopIteration,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "OverflowError": OverflowError, "FloatingPointError": FloatingPointError,
    "MemoryError": MemoryError, "ReferenceError": ReferenceError, "SystemError": SystemError,
    "BufferError": BufferError, "EOFError": EOFError, "Exception": Exception,
    "BaseException": BaseException, "KeyboardInterrupt": KeyboardInterrupt,
    "SystemExit": SystemExit, "GeneratorExit": GeneratorExit,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ResourceWarning": ResourceWarning,
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
    "BadZipFile": _bad_zip, "zlib.error": _zlib_error, "LZMAError": _lzma_error,
    "_SQLiteSnapshotError": _sqlite_err,
    "AdminUnsupported": type("AdminUnsupported", (Exception,), {}),
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        raise cls(r.get("msg", ""))
    return r


# ---- extraction ----

class _Stat:
    def __init__(self, **kw): self.__dict__.update(kw)


class AdminUnsupported(Exception):
    pass


def _silent_logger():
    class L:
        def _rec(self, se, fmt, args):
            norm = [str(x) for x in args]
            seam(se, {"fmt": fmt, "args": norm})
        def debug(self, fmt, *args, **kw): self._rec("logger_debug", fmt, args)
        def warning(self, fmt, *args, **kw): self._rec("logger_warning", fmt, args)
        def error(self, fmt, *args, **kw): self._rec("logger_error", fmt, args)
        def info(self, fmt, *args, **kw): self._rec("logger_info", fmt, args)
    return L()


def extract():
    import stat as _stat
    from typing import Any, Callable, Optional, Dict, List, Tuple

    src = open(SRC, encoding="utf-8").read()
    # stat stub with the projections backup.py uses
    class _StatMod:
        S_IFMT = 0o170000
        S_IFREG = 0o100000
        S_IFLNK = 0o120000
        FILE_ATTRIBUTE_REPARSE_POINT = 0x400
        @staticmethod
        def S_ISREG(m): return (m & _StatMod.S_IFMT) == _StatMod.S_IFREG
        @staticmethod
        def S_ISLNK(m): return (m & _StatMod.S_IFMT) == _StatMod.S_IFLNK

    ns = {
        "__name__": "hermes_cli.backup_flow3_extracted",
        "json": json, "os": os, "sys": sys, "copy": copy,
        "Any": Any, "Callable": Callable, "Optional": Optional,
        "Dict": Dict, "List": List, "Tuple": Tuple,
        "stat": _StatMod,
        "Path": P,
        "AdminUnsupported": AdminUnsupported,
        "_SQLiteSnapshotError": type("_SQLiteSnapshotError", (Exception,), {}),
        "_EXTERNAL_PREFIX": "_external/",
        "_SQLITE_SIDECAR_SUFFIXES": (".db-wal", ".db-shm", ".db-journal"),
        "_IMPORT_SKIP_NAMES": {"gateway_state.json", "gateway.pid", "cron.pid",
                                "gateway.lock", "processes.json"},
        "PM_RUNTIME_ROOT_DIRS": {"models", "runtimes", "node", "installs", "tools"},
        "_SECRET_FILE_NAMES": {"config.yaml", ".env", "auth.json", "token.json"},
        "_PRE_UPDATE_BACKUPS_DIR": "backups",
        "_RUN_BACKUP_PREFIX": "",
        "_PROTECTED_CONFIG_PATHS": [("model", "provider"), ("model", "default"), ("moa",)],
        "_ZIP_MEMBER_READ_ERRORS": (OSError, _bad_zip, _zlib_error, EOFError, _lzma_error),
        "print": lambda *a, **k: seam("print", {"msg": " ".join(str(x) for x in a)}),
        "logger": _silent_logger(),
        "_format_size": lambda n: str(n),
        "suppress": __import__("contextlib").suppress,
        "profile_scope": lambda **k: None,
        "datetime": type("DTStub", (), {"now": staticmethod(
            lambda: Obj(strftime=lambda _: "20260101-000000"))})(),
    }
    # In-function imports resolve sys.modules, not the exec namespace. Install
    # module stubs so the LIVE defs' `from ... import ...` lines are inert.
    import contextlib as _cl
    import types as _types
    gm = sys.modules.get("gateway") or _types.ModuleType("gateway")
    rm = sys.modules.get("gateway.runtime_ownership") or _types.ModuleType("gateway.runtime_ownership")
    rm.exclusive_maintenance = lambda homes: _cl.nullcontext()
    rm.OwnershipConflict = type("OwnershipConflict", (Exception,), {})
    gm.runtime_ownership = rm
    sys.modules["gateway"] = gm
    sys.modules["gateway.runtime_ownership"] = rm
    hcm = sys.modules.get("hermes_cli") or _types.ModuleType("hermes_cli")
    cfg = sys.modules.get("hermes_cli.config") or _types.ModuleType("hermes_cli.config")
    cfg.atomic_config_write = lambda p, d: seam("atomic_write", {})
    hcm.config = cfg
    sys.modules["hermes_cli"] = hcm
    sys.modules["hermes_cli.config"] = cfg
    gw = sys.modules.get("hermes_cli.gateway") or _types.ModuleType("hermes_cli.gateway")
    gw._is_service_running = lambda: seam("service_running", {})
    hcm.gateway = gw
    sys.modules["hermes_cli.gateway"] = gw
    gs = sys.modules.get("hermes_cli.gateway_setup_service") or _types.ModuleType("hermes_cli.gateway_setup_service")
    gs.ensure_gateway_service = lambda **kw: seam("ensure_service", kw)
    hcm.gateway_setup_service = gs
    sys.modules["hermes_cli.gateway_setup_service"] = gs

    tree = ast.parse(src)
    top = {n.name for n in tree.body if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in NESTED_OK \
                and node.name not in top:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # _publish_path uses nonlocal -> transcribe (nonlocal 'published' cannot be
    # exec'd at module scope).
    def _publish_path_transcribed():
        published = None
        if not NS.get("errors"):
            published = NS.get("out_path")
        elif len(NS.get("errors", [])) < len(NS.get("files_to_add", [])):
            published = NS.get("salvage_path")
        return published
    ns["_publish_path"] = _publish_path_transcribed
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    global NS
    NS = ns
    return ns


# ---- generic path double (per-drive hooks) ----

class P:
    """Path projection. Per-drive hooks let each function's collaborators fire
    the exact seam contract its C port uses."""
    def __init__(self, tag, *, suffix="", name=None, resolve=None, is_dir=None,
                 exists=None, is_file=None, is_symlink=None, relative_to=None):
        self.tag = tag
        self._suffix = suffix
        self._name = name if name is not None else tag
        self._resolve = resolve
        self._is_dir = is_dir
        self._exists = exists
        self._is_file = is_file
        self._is_symlink = is_symlink
        self._relative_to = relative_to
    @property
    def name(self): return self._name
    @property
    def suffix(self):
        if self._suffix: return self._suffix
        i = self.tag.rfind(".")
        return self.tag[i:] if i > 0 else ""
    def resolve(self, strict=False):
        return self._resolve(self.tag) if self._resolve else P(self.tag, suffix=self._suffix, name=self._name)
    def is_dir(self): return self._is_dir(self.tag) if self._is_dir else False
    def exists(self): return self._exists(self.tag) if self._exists else False
    def is_file(self): return self._is_file(self.tag) if self._is_file else False
    def is_symlink(self): return self._is_symlink(self.tag) if self._is_symlink else False
    def relative_to(self, other):
        if self._relative_to: return self._relative_to(self.tag, other)
        return self.tag
    def is_relative_to(self, other): return True
    def lstat(self): return seam("lstat", {"path": self.tag})
    def stat(self): return Obj(st_size=seam("stat_size", {"path": self.tag}))
    @property
    def parent(self): return P(self.tag)
    def mkdir(self, **kw): return seam("mkdir", {"path": self.tag})
    def __truediv__(self, other):
        return P("%s/%s" % (self.tag, other), suffix=("." + other.rsplit(".", 1)[1] if "." in other else ""))
    def __str__(self): return self.tag
    def __repr__(self): return "P(%r)" % self.tag
    def __eq__(self, other): return self.tag == str(other)
    def __hash__(self): return hash(self.tag)
    @staticmethod
    def home(*a, **k): return P("")
    @property
    def parts(self): return tuple(self.tag.split("/"))


class Obj:
    def __init__(self, **kw): self.__dict__.update(kw)


# ---- live drive ----

def drive(ns, func, inp):
    g = ns

    if func == "is_non_regular_path":
        class IP(P):
            def lstat(self):
                r = seam("lstat", {"path": self.tag})
                return Obj(st_mode=r.get("mode", 0))
        return ns["_is_non_regular_path"](IP(inp.get("path", "")))

    if func == "is_link_path":
        ns["os"] = Obj(name="nt" if inp.get("os_name_nt") else "posix")
        class LStatP(P):
            def lstat(self):
                r = seam("lstat", {"path": self.tag})
                return Obj(st_mode=r.get("mode", 0),
                           st_file_attributes=r.get("st_file_attributes", 0))
        return ns["_is_link_path"](LStatP(inp.get("path", "")))

    if func == "snapshot_db":
        def is_store(src): return seam("is_store_file", {"src": str(src)})
        def safe_copy(src, dst): return seam("safe_copy_db", {"src": str(src), "dst": str(dst)})
        def open_admin(src):
            r = seam("open_store_admin", {"src": str(src)})
            if isinstance(r, dict) and r.get("unsupported"):
                return AdminUnsupported()
            return Obj(backup_to=lambda dst: seam("admin_backup_to", {"dst": str(dst)}))
        ns["_is_store_file"] = is_store
        ns["_safe_copy_db"] = safe_copy
        ns["open_store_admin"] = open_admin
        return ns["_snapshot_db"](P(inp.get("src", "")), P(inp.get("dst", "")))

    if func == "discard_failed_zip_members":
        class FL:
            def __init__(self, zf, entries): self.zf, self.entries = zf, list(entries); self.last = None
            def __len__(self): return seam("filelist_len", {"zf": self.zf})
            def __iter__(self): return iter([Obj(filename=e.get("name", "")) for e in self.entries])
            def __getitem__(self, i):
                if isinstance(i, slice): return self.entries[i]
                return Obj(header_offset=seam("header_offset", {"zf": self.zf, "index": i}),
                           filename=self.entries[i].get("name", "") if i < len(self.entries) else "")
            def __delitem__(self, i):
                if isinstance(i, slice): del self.entries[i]
        class FP:
            def __init__(self, fl): self.fl = fl
            def seek(self, off): self.fl.last = off; return seam("fp_seek", {"offset": off})
            def truncate(self): return seam("fp_truncate", {"offset": self.fl.last})
        class NTI:
            def __init__(self, fl): self.fl = fl
            def clear(self): self.map = {}
            def update(self, gen):
                seam("nametoinfo", {"zf": self.fl.zf, "offset": self.fl.last}); self.map = dict(gen)
        class ZF:
            def __init__(self, tag, ents): self.tag = tag; self.filelist = FL(tag, ents)
            @property
            def fp(self):
                if not hasattr(self, "_fp"): self._fp = FP(self.filelist); self._nti = NTI(self.filelist)
                return self._fp
            @property
            def NameToInfo(self): self.fp; return self._nti
            def _set_start_dir(self, v): self._sd = v
            def _get_start_dir(self): return getattr(self, "_sd", 0)
            start_dir = property(_get_start_dir, _set_start_dir)
        ents = [{"name": e.get("name", "")} for e in inp.get("entries", [])]
        zf = ZF(inp.get("zf", ""), ents)
        return ns["_discard_failed_zip_members"](zf, inp.get("filelist_len", 0))

    if func == "zip_sqlite_snapshot":
        import contextlib
        out = inp.get("out_path", "")
        @contextlib.contextmanager
        def staged(parent, name):
            tmp = seam("staged_backup", {"out": str(parent)})
            yield P(str(tmp), resolve=lambda t: P(t),
                    stat=lambda: seam("stat_size", {"path": t}) if False else None)
        # simpler explicit tmp double with stat()
        class Tmp:
            def __init__(self, t): self.t = t
            def stat(self): return Obj(st_size=seam("stat_size", {"path": self.t}))
            def __str__(self): return self.t
        @contextlib.contextmanager
        def staged2(parent, name):
            tmp = seam("staged_backup", {"out": str(parent)})
            yield Tmp(str(tmp))
        ns["staged_backup_file"] = staged2
        ns["_snapshot_db"] = lambda a, b: seam("snapshot_db", {"src": str(a), "dst": str(b)})
        ns["warn_legacy_backup_file"] = lambda p: seam("warn_legacy", {"path": str(p)})
        ns["_write_zip_file"] = lambda z, p, a: seam("write_zip_file", {"path": str(p), "arcname": a})
        return ns["_zip_sqlite_snapshot"](Obj(), P(inp.get("abs_path", "")),
                                          P(inp.get("rel_path", "")), P(out))

    if func == "write_zip_entries":
        def zsqlite(z, a, r, out): return seam("zip_sqlite_snapshot", {"src": str(a), "rel": str(r)})
        def warn(p): return seam("warn_legacy", {"path": str(p)})
        def wzf(z, p, a): return seam("write_zip_file", {"path": str(p), "arcname": a})
        ns["_zip_sqlite_snapshot"] = zsqlite
        ns["warn_legacy_backup_file"] = warn
        ns["_write_zip_file"] = wzf
        files = [(P(a), P(r)) for a, r in inp.get("files", [])]
        return ns["_write_zip_entries"](
            Obj(), files, P(inp.get("out_path", "")),
            on_db_failure=lambda r: seam("db_failure", {"rel": str(r)}),
            on_error=lambda r, e: seam("on_error", {"rel": str(r), "exc": str(e)}),
            on_progress=lambda i: seam("on_progress", {"i": i}),
            track_bytes=bool(inp.get("track_bytes", False)))

    if func == "resolve_backup_output_path":
        dflt = inp.get("default_name", "backup.zip")
        class DT:
            @staticmethod
            def now(): return Obj(strftime=lambda _: dflt[:-4] if dflt.endswith(".zip") else dflt)
        ns["datetime"] = DT
        output = inp.get("output")
        class RD:
            def __init__(self, tag): self.tag = tag
            @property
            def suffix(self):
                i = self.tag.rfind(".")
                return self.tag[i:] if i > 0 else ""
            def is_dir(self): return seam("is_dir", {"path": self.tag})
            def with_suffix(self, sfx):
                new = (self.tag[:-len(self.suffix)] + sfx) if self.suffix else (self.tag + sfx)
                seam("with_suffix", {"path": new})
                return RD(new)
            @property
            def parent(self): return self
            def mkdir(self, **kw): return seam("mkdir", {"path": self.tag})
            def __truediv__(self, o): return RD(self.tag + "/" + str(o))
            def __str__(self): return self.tag
        class RootPath:
            def expanduser(self): return self
            def resolve(self):
                v = seam("resolve_output", {"path": output or ""})
                return RD(str(v))
        ns["Path"] = type("PathFactory", (), {
            "__new__": lambda cls, v=None: RootPath(),
            "home": staticmethod(lambda: RootPath()),
        })
        return str(ns["_resolve_backup_output_path"](output))

    if func == "collect_external_entries":
        def home_resolve(): return seam("home_resolve", {})
        class BD:
            def __init__(self, tag): self.tag = str(tag)
            def resolve(self):
                r = seam("base_resolve", {"path": self.tag})
                return Obj(relative_to=lambda h: r)
            def __str__(self): return self.tag
        class FD:
            def __init__(self, tag): self.tag = str(tag)
            def resolve(self):
                r = seam("fpath_resolve", {"path": self.tag})
                return Obj(relative_to=lambda h: Obj(as_posix=lambda: str(r)))
            def __str__(self): return self.tag
        def ext_paths():
            r = seam("external_paths", {})
            return [BD(x) for x in (r or [])]
        def iter_ext(b):
            r = seam("iter_external_files", {"path": str(b)})
            return [FD(x) for x in (r or [])]
        ns["Path"] = type("PF", (), {"home": staticmethod(lambda: Obj(resolve=home_resolve))})
        ns["_collect_memory_provider_external_paths"] = ext_paths
        ns["_iter_external_files"] = iter_ext
        res = ns["_collect_external_entries"]()
        add, skip = res
        add = [[str(a), b] for a, b in add]
        return [add, list(skip)]

    if func == "run_import":
        ns["require_file_tooling"] = lambda w: seam("require_file_tooling", {"what": w})
        class ZP(P):
            def expanduser(self): return self
            def resolve(self): return ZP(str(seam("expanduser_resolve", {"path": self.tag})))
            def is_file(self): return seam("is_file", {"path": self.tag})
        ns["Path"] = type("PF", (), {"__new__": lambda cls, v=None: ZP(str(v))})
        ns["sys"] = Obj(exit=lambda c: (_ for _ in ()).throw(SystemExit(str(c))))
        import contextlib as _cl
        class _ZFC:
            def __enter__(self): return Obj()
            def __exit__(self, *a): return False
        ns["zipfile"] = Obj(is_zipfile=lambda p: seam("is_zipfile", {"path": str(p)}),
                            ZipFile=lambda *a, **k: _ZFC())
        ns["get_hermes_home"] = lambda: P(inp.get("hermes_root", ""))
        ns["_validate_backup_zip"] = lambda z: tuple(_valzip())
        def _valzip():
            r = seam("validate_zip", {"path": inp.get("zipfile", "")})
            if isinstance(r, dict): return (r.get("ok", True), r.get("reason", ""))
            return (bool(r), "")
        class Args: pass
        a = Args(); a.zipfile = inp.get("zipfile", ""); a.force = inp.get("force", False)
        return ns["run_import"](a)

    if func == "import_members":
        ns["_import_members_exclusive"] = lambda *a, **k: seam("import_members_exclusive", {})
        g_root = inp.get("hermes_root", "")
        class MR(P):
            def resolve(self):
                seam("resolve_root", {"path": self.tag}); return self
            def __truediv__(self, o): return MR(self.tag + "/" + str(o))
            @property
            def parts(self): return tuple(seam("member_parts", {"member": self.tag}))
        ns["Path"] = type("PF", (), {"__new__": lambda cls, v=None: MR(str(v))})
        class KH:
            def homes(self): return seam("keep_host_homes", {"homes": [g_root]}) or []
            def skips(self, parts): return False
            def around(self, f): return f
        kh = KH()  # always present so homes() fires the seam on both sides
        r = ns["_import_members"](Obj(), list(inp.get("members", [])), inp.get("prefix", ""),
                                  MR(g_root), inp.get("file_count", 0), kh)
        return r

    if func == "import_external_member":
        def relative_to(t): return seam("relative_to", {"target": str(t)})
        class TP(P):
            def resolve(self): return Obj(relative_to=lambda h: seam("relative_to", {"target": self.tag}))
            @property
            def parent(self): return TP(self.tag)
            def __truediv__(self, o): return TP(str(o))
        ns["Path"] = type("PF", (), {"__new__": lambda cls, v=None: TP(str(v))})
        ns["_extract_member_atomically"] = lambda z, m, t, mode: seam("extract", {"member": m, "target": str(t)})
        ns["os"] = Obj(chmod=lambda p, m: seam("chmod", {"path": str(p), "mode": m}))
        ns["_SECRET_FILE_NAMES"] = {"config.yaml", ".env", "auth.json", "token.json"}
        errors = []
        r = ns["_import_external_member"](Obj(), inp.get("member", ""), TP(inp.get("home_dir", "")),
                                          inp.get("mode", 384), errors)
        return [r, errors[-1] if errors else None]

    if func == "import_members_exclusive":
        class HP(P):
            def resolve(self): seam("home_resolve", {"path": self.tag}); return self
        ns["Path"] = type("PF", (), {"home": staticmethod(lambda: HP(inp.get("hermes_root", "")))})
        ns["_default_new_file_mode"] = lambda: seam("default_mode", {})
        def iem(z, m, h, mode, errors):
            r = seam("import_external_member", {"member": m})
            if not r[0]: errors.append("x")
            return r[0]
        ns["_import_external_member"] = iem
        r = ns["_import_members_exclusive"](Obj(), list(inp.get("members", [])),
                                            inp.get("prefix", ""), P(inp.get("hermes_root", "")),
                                            inp.get("file_count", 0))
        return list(r)

    if func == "revive_gateway_after_import":
        class RP(P):
            def __truediv__(self, o): return P(self.tag + "/" + str(o), exists=marker_exists)
        ns["_get_platform_default_hermes_home"] = lambda: RP(str(seam("native_default", {}) or ""), exists=marker_exists)
        fired = {"n": 0}
        def marker_exists(tag):
            if fired["n"] == 0:
                fired["n"] = 1
                r = seam("marker_exists", {"root": inp.get("native_default", "")})
                return bool(r)
            return False
        ns["Path"] = type("PF2", (), {
            "__new__": lambda cls, v=None: RP(str(v), exists=marker_exists),
        })
        ns["Path"].home = staticmethod(lambda: RP("", exists=marker_exists))
        ns["store_exists"] = lambda s: seam("store_exists", {})
        ns["_is_service_running"] = lambda: seam("service_running", {})
        ns["ensure_gateway_service"] = lambda **kw: seam("ensure_service", kw)
        ns["logger"] = _silent_logger()
        return ns["_revive_gateway_after_import"](P(inp.get("hermes_root", "")))

    if func == "newest_first":
        root = P(inp.get("root", ""),
                 exists=lambda t: seam("path_exists", {"path": t}),
                 resolve=None)
        class NF(P):
            def iterdir(self):
                r = seam("path_iterdir", {"path": self.tag})
                return [P(e.get("name", ""), name=e.get("name", ""),
                          resolve=lambda t, nm=e.get("name", ""): P(nm)) for e in (r or [])]
        root2 = NF(inp.get("root", ""), exists=lambda t: seam("path_exists", {"path": t}))
        keep = lambda p: inp.get("entries_map", {}).get(p.name, {}).get("keep", False)
        r = ns["_newest_first"](root2, keep)
        return [p.name for p in r]

    if func == "prune_oldest":
        names = [P(n) for n in inp.get("names", [])]
        r = ns["_prune_oldest"](names, inp.get("keep", 0),
                                lambda p: seam("unlink", {"path": p.name}), inp.get("what", ""))
        return r

    if func == "create_prefixed_full_backup":
        home = inp.get("hermes_home", "")
        class HB(P):
            def is_dir(self): return seam("is_dir", {"path": self.tag})
            def mkdir(self, **kw): return seam("mkdir", {"path": self.tag})
            def __truediv__(self, o): return HB(self.tag)
        ns["get_default_hermes_root"] = lambda: HB(home)
        root = HB(home)
        def wfz(op, hr):
            r = seam("write_full_zip", {"path": str(op), "prefix": inp.get("prefix", "")})
            return P(str(r)) if r is not None else None
        ns["_write_full_zip_backup"] = wfz
        ns["_prune_incomplete_zips"] = lambda d, p, w: seam("prune_incomplete", {"path": str(d)})
        ns["_prune_prefixed_zips"] = lambda d, p, k, w: seam("prune_prefixed", {"path": str(d)})
        ns["os"] = Obj(chmod=lambda p, m: seam("chmod", {"path": str(p)}))
        ns["logger"] = _silent_logger()
        r = ns["_create_prefixed_full_backup"](root, inp.get("prefix", ""), inp.get("keep", 0),
                                               inp.get("what", ""), inp.get("prune_what", ""))
        return str(r) if r is not None else None

    if func == "too_large":
        g["max_file_size"] = inp.get("max_file_size")
        ns["max_file_size"] = inp.get("max_file_size")
        ns["_format_size"] = lambda n: str(n)
        # remove hooks in P.stat so it fires our seam (P.stat already does)
        return ns["_too_large"](P(inp.get("path", "")), inp.get("rel_name", ""))

    if func == "is_trusted_root_auth_alias":
        class EqObj:
            def __init__(self, tag, se, other_key): self.tag = tag; self.se = se; self.ok = other_key
            def __str__(self): return str(self.tag)
            def __eq__(self, other): return seam(self.se, {"path": self.tag, self.ok: str(other)})
            def __truediv__(self, o): return EqObj(self.tag, "noop", "root")  # trusted projects to root tag
            def relative_to(self, other): return self.tag
            def resolve(self, strict=False): return self
        def gdr(home=None):
            r = seam("get_default_root", {"home": str(home)})
            return EqObj(r, "noop", "root")
        ns["get_default_hermes_root"] = gdr
        class AliasP:
            def __init__(self, tag): self.tag = tag
            def is_symlink(self): return seam("is_symlink", {"path": self.tag})
            def resolve(self, strict=False): return EqObj(self.tag, "resolve_equal_trusted", "root")
        class HomeP:
            def __init__(self, tag): self.tag = tag
            def resolve(self, strict=False): return EqObj(self.tag, "resolve_equal", "other")
            def __str__(self): return self.tag
        return ns["_is_trusted_root_auth_alias"](AliasP(inp.get("path", "")), HomeP(inp.get("home", "")))

    if func == "quick_snapshot_dir":
        sid = inp.get("snapshot_id", "")
        r0 = seam("snapshot_root", {"home": inp.get("hermes_home", "")})
        class QSnap:
            def __init__(self, tag, root): self.tag = tag; self.root = root
            def resolve(self): return Obj(relative_to=lambda r: seam(
                "relative_to", {"path": sid, "root": self.root}))
            def is_dir(self): return seam("is_dir", {"path": self.tag})
            def __truediv__(self, o): return Obj(exists=lambda: seam(
                "manifest_exists", {"path": self.tag + "/" + str(o)}))
            def __str__(self): return self.tag
        class QRoot:
            def __init__(self, tag): self.tag = tag
            def __truediv__(self, o): return QSnap(self.tag + "/" + str(o), self.tag)
            def resolve(self): return Obj()
        ns["_quick_snapshot_root"] = lambda h: QRoot(str(r0)) if r0 is not None else None
        return ns["quick_snapshot_dir"](sid, inp.get("hermes_home", ""))

    if func == "restore_quick_snapshot_exclusive":
        ns["quick_snapshot_dir"] = lambda sid, h: seam("quick_snapshot_dir", {"snapshot_id": sid, "home": str(h)})
        class MN:
            def get(self, k, d=None): return d
        return ns["_restore_quick_snapshot_exclusive"](inp.get("snapshot_id", ""), P(inp.get("home", "")))

    if func == "entry_paths":
        def resolve_src(rel): return seam("resolve_src", {"rel": rel})
        def resolve_dst(rel): return seam("resolve_dst", {"rel": rel})
        class EP(P):
            def __truediv__(self, o): return EP(self.tag + "/" + str(o))
            def resolve(self, strict=False):
                if self.tag.startswith("snap/"): return Obj(is_relative_to=lambda r: resolve_src(self.tag[5:]))
                return Obj(is_relative_to=lambda r: resolve_dst(self.tag[5:]))
        snap = EP("snap")
        home = EP("home")
        ns["_is_trusted_root_auth_alias"] = lambda d, h: seam("trusted_alias", {"rel": inp.get("rel", "")})
        ns["snap_dir"] = snap
        ns["home"] = home
        ns["snap_res"] = P("snap")
        ns["home_res"] = P("home")
        r = ns["_entry_paths"](inp.get("rel", ""))
        return list(r) if r is not None else None

    if func == "restore_config_model_settings_if_rewritten":
        ns["supports_file_tooling"] = lambda: seam("supports_file_tooling", {})
        reads = {"i": 0}
        class YD:
            def __init__(self, which, v): self.which = which; self.v = v
            def __bool__(self): return bool(self.v)
        def read_yaml(p):
            which = "snap" if reads["i"] == 0 else "live"
            reads["i"] += 1
            v = seam("read_yaml", {"kind": which})
            return None if v is None else YD(which, v)
        ns["_read_raw_yaml_dict"] = read_yaml
        def get_cv(d, dotted):
            return seam("get_config_value", {"key": ".".join(dotted), "which": d.which})
        ns["_get_config_path_value"] = get_cv
        ns["_set_config_path_value"] = lambda live, dotted, v: seam("set_config_value", {"key": ".".join(dotted)})
        ns["atomic_config_write"] = lambda p, d: seam("atomic_write", {})
        ns["get_hermes_home"] = lambda: P(inp.get("hermes_home", ""))
        ns["_quick_snapshot_root"] = lambda h: P("ROOT")
        ns["_PROTECTED_CONFIG_PATHS"] = [tuple(x.split(".")) for x in inp.get("protected", [])]
        ns["logger"] = _silent_logger()
        return ns["restore_config_model_settings_if_rewritten"](inp.get("snapshot_id", ""),
                                                                  P(inp.get("hermes_home", "")))

    if func == "db_failure":
        g["errors"] = []
        ns["logger"] = _silent_logger()
        try:
            ns["_db_failure"](P(inp.get("rel_path", "")))
            return None
        except BaseException as ex:
            return {"__raise__": type(ex).__name__, "msg": str(ex)}

    if func == "capped_errors":
        ns["errors"] = inp.get("errors", [])
        return ns["_capped_errors"]()

    if func == "publish_path":
        ns["errors"] = inp.get("errors", [])
        ns["out_path"] = inp.get("out_path", "")
        ns["salvage_path"] = inp.get("salvage_path", "")
        ns["files_to_add"] = inp.get("files_to_add", [None] * inp.get("nfiles", 0))
        return ns["_publish_path"]()

    if func == "progress":
        g["file_count"] = inp.get("file_count", 0)
        ns["file_count"] = inp.get("file_count", 0)
        return ns["_progress"](inp.get("i", 0))

    raise ValueError("unhandled func: %s" % func)


# ---- comparison ----

def _numnorm(o):
    if type(o) is float and o.is_integer(): return int(o)
    if isinstance(o, dict): return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)): return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def reset_case(inp):
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams", {})
    CUR["idx"] = {}


def run_live(func, inp):
    extract_once()
    reset_case(inp)
    snap = dict(NS)  # extracted defs close over NS; restore per-case stub edits
    try:
        ret = drive(NS, func, inp)
        return {"ret": ret, "trace": CUR["trace"]}, None
    except BaseException as ex:
        return {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                "trace": CUR["trace"]}, ex
    finally:
        NS.clear(); NS.update(snap)


_EXTRACTED = [False]


def extract_once():
    if not _EXTRACTED[0]:
        extract()
        _EXTRACTED[0] = True


def fixture():
    return _REPO / "tests" / "fixtures" / "backup_flow3.jsonl"


def load_cases():
    p = fixture()
    if not p.exists(): return []
    return [json.loads(l) for l in p.read_text(encoding="utf-8").splitlines() if l.strip()]


def check_func_set(cases):
    ff = {c["func"] for c in cases}
    if ff != set(PY):
        print("FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(ff - set(PY)), sorted(set(PY) - ff)))
        sys.exit(2)
    counts = {}
    for c in cases:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("FATAL <5 cases for: %s" % thin)
        sys.exit(2)


SEAM_NAMES = [
    "lstat", "is_store_file", "safe_copy_db", "open_store_admin", "admin_backup_to",
    "filelist_len", "header_offset", "fp_seek", "fp_truncate", "nametoinfo",
    "staged_backup", "snapshot_db", "warn_legacy", "write_zip_file", "stat_size",
    "zip_sqlite_snapshot", "db_failure", "on_error", "on_progress",
    "resolve_output", "is_dir", "with_suffix", "mkdir", "print",
    "home_resolve", "external_paths", "base_resolve", "iter_external_files", "fpath_resolve",
    "require_file_tooling", "expanduser_resolve", "is_file", "is_zipfile", "validate_zip",
    "resolve_root", "member_parts", "keep_host_homes", "import_members_exclusive",
    "relative_to", "extract", "chmod", "default_mode", "import_external_member",
    "native_default", "marker_exists", "store_exists", "service_running", "ensure_service",
    "logger_debug", "logger_warning", "logger_error", "logger_info",
    "path_exists", "path_iterdir", "unlink", "snapshot_root", "manifest_exists",
    "quick_snapshot_dir", "resolve_src", "resolve_dst", "trusted_alias",
    "supports_file_tooling", "read_yaml", "get_config_value", "set_config_value", "atomic_write",
    "is_symlink", "get_default_root", "resolve_equal", "resolve_equal_trusted",
    "write_full_zip", "prune_incomplete", "prune_prefixed",
]


def self_test():
    extract_once()
    cases = load_cases()
    check_func_set(cases)
    bad = 0
    for c in cases:
        _, ex = run_live(c["func"], c["in"])
        if isinstance(ex, AttributeError) and any(
                ("'%s'" % n) in str(ex) for n in list(PY) + SEAM_NAMES):
            print("SELFTEST BINDING %s: %s: %s" % (c["func"], type(ex).__name__, ex))
            bad += 1
        elif isinstance(ex, NameError):
            print("SELFTEST NAMEERROR %s: %s" % (c["func"], ex))
            bad += 1
    print("SELFTEST %d cases, %d binding/name errors" % (len(cases), bad))
    sys.exit(1 if bad else 0)


# ---- shadow-only seam variants ----

def sseam(name, args):
    args = copy.deepcopy({k: v for k, v in args.items() if v is not None})
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, dict) and "__arr__" in sc:
            r = sc["__arr__"]
        elif isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": copy.deepcopy(r)})
    return r


def sseam_raise(name, args):
    return maybe_raise(sseam(name, args))


def _bail(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def _caught(r, cls):
    if not (isinstance(r, dict) and "__raise__" in r): return False
    k = r["__raise__"]
    chain = {
        "ValueError": "Exception", "TypeError": "Exception", "RuntimeError": "Exception",
        "AttributeError": "Exception", "OSError": "Exception", "EOFError": "Exception",
        "BadZipFile": "Exception", "zlib.error": "Exception", "LZMAError": "Exception",
        "_SQLiteSnapshotError": "Exception",
        "PermissionError": "OSError", "FileNotFoundError": "OSError",
        "IndexError": "LookupError", "KeyError": "LookupError", "LookupError": "Exception",
        "Exception": "BaseException", "SystemExit": "BaseException",
        "KeyboardInterrupt": "BaseException", "GeneratorExit": "BaseException",
        "AdminUnsupported": "Exception", "BaseException": None,
    }
    if k == "ExceptionGroup" and cls == "Exception": return True
    while k:
        if k == cls: return True
        k = chain.get(k, "Exception")
    return False


def _is_exc(r):
    if not (isinstance(r, dict) and "__raise__" in r): return False
    k = r["__raise__"]
    if k in ("SystemExit", "KeyboardInterrupt", "GeneratorExit", "BaseException", "BaseExceptionGroup"):
        return False
    return True


def _slog(se, fmt, args):
    return sseam_raise(se, {"fmt": fmt, "args": [str(x) for x in args]})


# ---- shadow mirror (literal transcription of the C port) ----

def shadow_drive(func, inp):
    reset_case(inp)

    if func == "is_non_regular_path":
        info = sseam("lstat", {"path": inp.get("path", "")})
        if isinstance(info, dict) and "__raise__" in info:
            if _caught(info, "OSError"): return False
            return info
        mode = int(info.get("mode", 0)) if isinstance(info, dict) else 0
        return (mode & 0o170000) != 0o100000

    if func == "is_link_path":
        info = sseam("lstat", {"path": inp.get("path", "")})
        if isinstance(info, dict) and "__raise__" in info:
            if _caught(info, "OSError"): return False
            return info
        mode = int(info.get("mode", 0)) if isinstance(info, dict) else 0
        attrs = int(info.get("st_file_attributes", 0)) if isinstance(info, dict) else 0
        if (mode & 0o170000) == 0o120000: return True
        return bool(inp.get("os_name_nt")) and (attrs & 0x400) != 0

    if func == "snapshot_db":
        src, dst = inp.get("src", ""), inp.get("dst", "")
        st = sseam("is_store_file", {"src": src})
        if isinstance(st, dict) and "__raise__" in st: return st
        if not st: return sseam("safe_copy_db", {"src": src, "dst": dst})
        admin = sseam("open_store_admin", {"src": src})
        if isinstance(admin, dict) and "__raise__" in admin: return admin
        if isinstance(admin, dict) and admin.get("unsupported"): return False
        r = sseam("admin_backup_to", {"dst": dst})
        if isinstance(r, dict) and "__raise__" in r: return r
        return bool(r)

    if func == "discard_failed_zip_members":
        zf, fl = inp.get("zf", ""), inp.get("filelist_len", 0)
        n = sseam("filelist_len", {"zf": zf})
        if isinstance(n, dict) and "__raise__" in n: return n
        if (n if isinstance(n, (int, float)) else 0) <= fl: return None
        off = sseam("header_offset", {"zf": zf, "index": fl})
        if isinstance(off, dict) and "__raise__" in off: return off
        offset = off if isinstance(off, (int, float)) else 0
        r = sseam("fp_seek", {"offset": offset})
        if isinstance(r, dict) and "__raise__" in r: return r
        r = sseam("fp_truncate", {"offset": offset})
        if isinstance(r, dict) and "__raise__" in r: return r
        r = sseam("nametoinfo", {"zf": zf, "offset": offset})
        if isinstance(r, dict) and "__raise__" in r: return r
        return None

    if func == "zip_sqlite_snapshot":
        out, rel = inp.get("out_path", ""), inp.get("rel_path", "")
        stage = sseam("staged_backup", {"out": out})
        if isinstance(stage, dict) and "__raise__" in stage: return stage
        tmp = stage if isinstance(stage, str) else ""
        ok = sseam("snapshot_db", {"src": inp.get("abs_path", ""), "dst": tmp})
        if isinstance(ok, dict) and "__raise__" in ok: return ok
        if not ok:
            r = sseam("warn_legacy", {"path": inp.get("abs_path", "")})
            if isinstance(r, dict) and "__raise__" in r: return r
            return None
        r = sseam("write_zip_file", {"path": tmp, "arcname": rel})
        if isinstance(r, dict) and "__raise__" in r: return r
        return sseam("stat_size", {"path": tmp})

    if func == "write_zip_entries":
        total = 0
        for i, pair in enumerate(inp.get("files", []), 1):
            ab, rel = pair[0], pair[1]
            err = False; r = None
            if ab.endswith(".db"):
                r = sseam("zip_sqlite_snapshot", {"src": ab, "rel": rel})
                if isinstance(r, dict) and "__raise__" in r: err = True
                elif r is None:
                    r = sseam("db_failure", {"rel": rel})
                    if isinstance(r, dict) and "__raise__" in r: err = True
                    else: continue
                else:
                    if isinstance(r, (int, float)): total += r
                    pass
            else:
                r = sseam("warn_legacy", {"path": ab})
                if isinstance(r, dict) and "__raise__" in r: err = True
                else:
                    r = sseam("write_zip_file", {"path": ab, "arcname": rel})
                    if isinstance(r, dict) and "__raise__" in r: err = True
                if not err and inp.get("track_bytes"):
                    sz = sseam("stat_size", {"path": ab})
                    if isinstance(sz, dict) and "__raise__" in sz: err = True
                    else:
                        if isinstance(sz, (int, float)): total += sz
            if err:
                er = sseam("on_error", {"rel": rel,
                                         "exc": r.get("msg", "") if isinstance(r, dict) and "__raise__" in r else None})
                if isinstance(er, dict) and "__raise__" in er: return er
                continue
            if i % 500 == 0:
                pr = sseam("on_progress", {"i": i})
                if isinstance(pr, dict) and "__raise__" in pr: return pr
        return total

    if func == "resolve_backup_output_path":
        output = inp.get("output")
        p = sseam("resolve_output", {"path": output or ""})
        if isinstance(p, dict) and "__raise__" in p:
            if _caught(p, "OSError"):
                sseam_raise("print", {"msg": "Error: cannot write backup to %s: %s" % (output or "", p.get("msg", ""))})
                return _bail("SystemExit", "1")
            return p
        cur = p if isinstance(p, str) else ""
        d = sseam("is_dir", {"path": cur})
        if isinstance(d, dict) and "__raise__" in d: return d
        final = (cur + "/" + inp.get("default_name", "backup.zip")) if d else cur
        if inp.get("suffix", "") != ".zip":
            nf = final + ".zip"
            r = sseam("with_suffix", {"path": nf})
            if isinstance(r, dict) and "__raise__" in r: return r
            final = r if isinstance(r, str) else final
        mk = sseam("mkdir", {"path": final})
        if isinstance(mk, dict) and "__raise__" in mk:
            if _caught(mk, "OSError"):
                sseam_raise("print", {"msg": "Error: cannot write backup to %s: %s" % (output or final, mk.get("msg", ""))})
                return _bail("SystemExit", "1")
            return mk
        return final

    if func == "collect_external_entries":
        h = sseam("home_resolve", {})
        if isinstance(h, dict) and "__raise__" in h: return h
        bases = sseam("external_paths", {})
        if isinstance(bases, dict) and "__raise__" in bases: return bases
        add, skip = [], []
        for b in bases or []:
            br = sseam("base_resolve", {"path": b})
            if isinstance(br, dict) and "__raise__" in br:
                if _caught(br, "ValueError") or _caught(br, "OSError"):
                    skip.append(b); continue
                return br
            files = sseam("iter_external_files", {"path": b})
            if isinstance(files, dict) and "__raise__" in files: return files
            for f in files or []:
                fr = sseam("fpath_resolve", {"path": f})
                if isinstance(fr, dict) and "__raise__" in fr: continue
                add.append([f, "_external/" + (fr if isinstance(fr, str) else f)])
        return [add, skip]

    if func == "run_import":
        zp = inp.get("zipfile", "")
        r = sseam("require_file_tooling", {"what": "`hermes import`"})
        if isinstance(r, dict) and "__raise__" in r: return r
        rz = sseam("expanduser_resolve", {"path": zp})
        if isinstance(rz, dict) and "__raise__" in rz: return rz
        isf = sseam("is_file", {"path": rz})
        if isinstance(isf, dict) and "__raise__" in isf: return isf
        if not isf:
            sseam_raise("print", {"msg": "Error: File not found: %s" % rz})
            return _bail("SystemExit", "1")
        iz = sseam("is_zipfile", {"path": rz})
        if isinstance(iz, dict) and "__raise__" in iz: return iz
        if not iz:
            sseam_raise("print", {"msg": "Error: Not a valid zip file: %s" % rz})
            return _bail("SystemExit", "1")
        ok = sseam("validate_zip", {"path": zp})
        if isinstance(ok, dict) and "__raise__" in ok: return ok
        valid = ok.get("ok", True) if isinstance(ok, dict) else bool(ok)
        reason = ok.get("reason", "") if isinstance(ok, dict) else ""
        if not valid:
            sseam_raise("print", {"msg": "Error: " + str(reason)})
            return _bail("SystemExit", "1")
        return None

    if func == "import_members":
        root = inp.get("hermes_root", "")
        rr = sseam("resolve_root", {"path": root})
        if isinstance(rr, dict) and "__raise__" in rr: return rr
        homes = [root]
        for m in inp.get("members", []):
            if m.startswith("_external/"): continue
            pa = sseam("member_parts", {"member": m})
            if isinstance(pa, dict) and "__raise__" in pa: return pa
            if isinstance(pa, list) and len(pa) >= 3 and pa[0] == "profiles":
                homes.append("%s/%s/%s" % (root, pa[0], pa[1]))
        kh = sseam("keep_host_homes", {"homes": homes})
        if isinstance(kh, dict) and "__raise__" in kh: return kh
        return sseam("import_members_exclusive", {})

    if func == "import_external_member":
        member = inp.get("member", "")
        rest = member[len("_external/"):] if member.startswith("_external/") else member
        rel = sseam("relative_to", {"target": rest})
        if isinstance(rel, dict) and "__raise__" in rel:
            if _caught(rel, "ValueError"):
                return [False, "  %s: path traversal blocked" % member]
            return rel
        mk = sseam("mkdir", {"path": rest})
        if isinstance(mk, dict) and "__raise__" in mk:
            return [False, "  %s: %s" % (member, mk.get("msg", ""))]
        ex = sseam("extract", {"member": member, "target": rest})
        if isinstance(ex, dict) and "__raise__" in ex:
            return [False, "  %s: %s" % (member, ex.get("msg", ""))]
        if inp.get("secret"):
            ch = sseam("chmod", {"path": rest, "mode": 384})
            if isinstance(ch, dict) and "__raise__" in ch:
                if not _caught(ch, "OSError"): return ch
        return [True, None]

    if func == "import_members_exclusive":
        root = inp.get("hermes_root", "")
        hr = sseam("home_resolve", {"path": root})
        if isinstance(hr, dict) and "__raise__" in hr: return hr
        md = sseam("default_mode", {})
        if isinstance(md, dict) and "__raise__" in md: return md
        restored = restored_ext = 0
        for m in inp.get("members", []):
            if not m.startswith("_external/"): continue
            ok = sseam("import_external_member", {"member": m})
            if isinstance(ok, dict) and "__raise__" in ok: return ok
            if ok[0]: restored += 1; restored_ext += 1
            if restored % 500 == 0:
                pr = sseam_raise("print", {"msg": "  %d/%d files ..." % (restored, inp.get("file_count", 0))})
                if isinstance(pr, dict) and "__raise__" in pr: return pr
        return [restored, restored_ext, [], [], []]

    if func == "revive_gateway_after_import":
        root = inp.get("hermes_root", "")
        nd = sseam("native_default", {})
        if isinstance(nd, dict) and "__raise__" in nd: return nd
        same = root == nd
        if not same:
            me = sseam("marker_exists", {"root": inp.get("native_default", "")})
            if isinstance(me, dict) and "__raise__" in me: return me
            clash = bool(me)
            if not clash:
                se = sseam("store_exists", {})
                if isinstance(se, dict) and "__raise__" in se: return se
                clash = bool(se)
            if clash:
                sseam_raise("print", {"msg": "\nRestored into a non-default home; leaving the gateway service alone "
                      "to avoid clashing with the install at %s.\n"
                      "To start a gateway for this home, run:  hermes gateway run" % nd})
                return None
        run = sseam("service_running", {})
        if isinstance(run, dict) and "__raise__" in run:
            if _is_exc(run):
                _slog("logger_debug", "gateway revive after import failed", [])
                sseam_raise("print", {"msg": "\nStart the gateway to activate cron jobs and messaging:\n  hermes gateway run"})
                return None
            return run
        if not run:
            sseam_raise("print", {"msg": ""})
            ens = sseam("ensure_service", {"context": "import"})
            if isinstance(ens, dict) and "__raise__" in ens: return ens
        return None

    if func == "newest_first":
        ex = sseam("path_exists", {"path": inp.get("root", "")})
        if isinstance(ex, dict) and "__raise__" in ex: return ex
        if not ex: return []
        ents = sseam("path_iterdir", {"path": inp.get("root", "")})
        if isinstance(ents, dict) and "__raise__" in ents: return ents
        names = [e.get("name", "") for e in ents if inp.get("entries_map", {}).get(e.get("name", ""), {}).get("keep")]
        return sorted(names, reverse=True)

    if func == "prune_oldest":
        keep, what = inp.get("keep", 0), inp.get("what", "")
        deleted = 0
        for n in inp.get("names", [])[keep:]:
            r = sseam("unlink", {"path": n})
            if isinstance(r, dict) and "__raise__" in r:
                if _caught(r, "OSError"):
                    _slog("logger_warning", "Failed to prune %s %s: %s", [what, n, r.get("msg", "")])
                    continue
                return r
            deleted += 1
        return deleted

    if func == "create_prefixed_full_backup":
        home = inp.get("hermes_home", "")
        d = sseam("is_dir", {"path": home})
        if isinstance(d, dict) and "__raise__" in d: return d
        if not d: return None
        mk = sseam("mkdir", {"path": home})
        if isinstance(mk, dict) and "__raise__" in mk:
            if _caught(mk, "OSError"):
                _slog("logger_warning", "Could not create %s backup dir %s: %s",
                      [inp.get("what", ""), home, mk.get("msg", "")])
                return None
            return mk
        ch = sseam("chmod", {"path": home})
        if isinstance(ch, dict) and "__raise__" in ch:
            if not _caught(ch, "OSError"): return ch
        wb = sseam("write_full_zip", {"path": home, "prefix": inp.get("prefix", "")})
        if isinstance(wb, dict) and "__raise__" in wb: return wb
        if wb is None:
            pi = sseam("prune_incomplete", {"path": home})
            if isinstance(pi, dict) and "__raise__" in pi: return pi
            return None
        outp = wb if isinstance(wb, str) else ""
        pp = sseam("prune_prefixed", {"path": outp})
        if isinstance(pp, dict) and "__raise__" in pp: return pp
        return outp

    if func == "too_large":
        mx = inp.get("max_file_size")
        if mx is None: return False
        st = sseam("stat_size", {"path": inp.get("path", "")})
        if isinstance(st, dict) and "__raise__" in st:
            if _caught(st, "OSError"): return False
            return st
        sz = st if isinstance(st, (int, float)) else 0
        if sz <= mx: return False
        rel, fsz, fmax = inp.get("rel_name", ""), inp.get("fmt_size", ""), inp.get("fmt_max", "")
        sseam_raise("print", {"msg": "  \u26a0 Snapshot: skipping %s (%s exceeds %s limit)" % (rel, fsz, fmax)})
        _slog("logger_warning", "Quick snapshot skipped %s: %d bytes exceeds %d byte limit", [rel, fsz, fmax])
        return True

    if func == "is_trusted_root_auth_alias":
        path, home = inp.get("path", ""), inp.get("home", "")
        sym = sseam("is_symlink", {"path": path})
        if isinstance(sym, dict) and "__raise__" in sym: return sym
        if not sym: return False
        root = sseam("get_default_root", {"home": home})
        if isinstance(root, dict) and "__raise__" in root:
            if _caught(root, "OSError") or _caught(root, "RuntimeError") or _caught(root, "ValueError"):
                return False
            return root
        eq = sseam("resolve_equal", {"path": home, "other": root})
        if isinstance(eq, dict) and "__raise__" in eq:
            if _caught(eq, "OSError") or _caught(eq, "RuntimeError") or _caught(eq, "ValueError"):
                return False
            return eq
        if eq: return False
        pr = sseam("resolve_equal_trusted", {"path": path, "root": root})
        if isinstance(pr, dict) and "__raise__" in pr:
            if _caught(pr, "OSError") or _caught(pr, "ValueError"): return False
            return pr
        return bool(pr)

    if func == "quick_snapshot_dir":
        sid = inp.get("snapshot_id", "")
        root = sseam("snapshot_root", {"home": inp.get("hermes_home", "")})
        if isinstance(root, dict) and "__raise__" in root: return root
        if root is None: return None
        if not sid or "/" in sid or "\\" in sid or sid in (".", ".."):
            _slog("logger_error", "Invalid snapshot_id: %s", [sid]); return None
        rel = sseam("relative_to", {"path": sid, "root": root})
        if isinstance(rel, dict) and "__raise__" in rel:
            if _caught(rel, "ValueError"):
                _slog("logger_error", "Snapshot path traversal blocked for id: %s", [sid]); return None
            return rel
        sd = "ROOT/" + sid
        d = sseam("is_dir", {"path": sd})
        if isinstance(d, dict) and "__raise__" in d: return d
        if not d: return None
        mf = sseam("manifest_exists", {"path": sd + "/manifest.json"})
        if isinstance(mf, dict) and "__raise__" in mf: return mf
        return sd if mf else None

    if func == "restore_quick_snapshot_exclusive":
        sd = sseam("quick_snapshot_dir", {"snapshot_id": inp.get("snapshot_id", ""),
                                          "home": inp.get("home", "")})
        if isinstance(sd, dict) and "__raise__" in sd: return sd
        if sd is None: return False
        return sseam("restore_manifest", {"manifest": inp.get("manifest")})

    if func == "entry_paths":
        rel = inp.get("rel", "")
        src = sseam("resolve_src", {"rel": rel})
        if isinstance(src, dict) and "__raise__" in src: return src
        if not src: return None
        dst = sseam("resolve_dst", {"rel": rel})
        if isinstance(dst, dict) and "__raise__" in dst: return dst
        if not dst:
            if rel != "auth.json": return None
            tr = sseam("trusted_alias", {"rel": rel})
            if isinstance(tr, dict) and "__raise__" in tr: return tr
            if not tr: return None
        return ["snap/" + rel, "home/" + rel]

    if func == "restore_config_model_settings_if_rewritten":
        sid = inp.get("snapshot_id", "")
        if not sid: return None
        sup = sseam("supports_file_tooling", {})
        if isinstance(sup, dict) and "__raise__" in sup: return sup
        if not sup: return None
        snap = sseam("read_yaml", {"kind": "snap"})
        if isinstance(snap, dict) and "__raise__" in snap: return snap
        if not snap: return None
        live = sseam("read_yaml", {"kind": "live"})
        if isinstance(live, dict) and "__raise__" in live: return live
        if live is None: return None
        keys = []
        for dotted in inp.get("protected", []):
            sv = sseam("get_config_value", {"key": dotted, "which": "snap"})
            if isinstance(sv, dict) and "__raise__" in sv: return sv
            if sv is None or sv == "" or sv == {} or sv == []: continue
            lv = sseam("get_config_value", {"key": dotted, "which": "live"})
            if isinstance(lv, dict) and "__raise__" in lv: return lv
            if json.dumps(_numnorm(sv), sort_keys=True) == json.dumps(_numnorm(lv), sort_keys=True): continue
            st = sseam("set_config_value", {"key": dotted})
            if isinstance(st, dict) and "__raise__" in st: return st
            keys.append(".".join(dotted) if isinstance(dotted, (list, tuple)) else dotted)
        if not keys: return None
        wr = sseam("atomic_write", {})
        if isinstance(wr, dict) and "__raise__" in wr:
            if _caught(wr, "OSError"):
                _slog("logger_error", "config.yaml model settings were rewritten during update but "
                      "auto-restore failed: %s", [wr.get("msg", "")])
                return None
            return wr
        _slog("logger_warning", "Restored user config value(s) %s from pre-update snapshot %s "
              "\u2014 the update flow rewrote them (#64160)", [", ".join(keys), sid])
        return {"restored": True, "keys": keys, "snapshot_id": sid}

    if func == "db_failure":
        rel = inp.get("rel_path", "")
        r = _slog("logger_warning", "Full-zip backup aborted: SQLite snapshot failed for %s", [rel])
        return _bail("_SQLiteSnapshotError", rel)

    if func == "capped_errors":
        errs = inp.get("errors", [])
        n = len(errs)
        shown = "; ".join(errs[:10])
        return shown + " (+%d more)" % (n - 10) if n > 10 else shown

    if func == "publish_path":
        errs, nf = inp.get("errors", []), inp.get("nfiles", 0)
        if not errs: return inp.get("out_path", "")
        if len(errs) < nf: return inp.get("salvage_path", "")
        return None

    if func == "progress":
        i, fc = inp.get("i", 0), inp.get("file_count", 0)
        sseam_raise("print", {"msg": "  %d/%d files ..." % (i, fc)})
        _slog("logger_info", "backup phase=archive status=progress completed=%d total=%d", [i, fc])
        return None

    raise ValueError("no shadow for %s" % func)


def shadow_mode():
    cases = load_cases()
    check_func_set(cases)
    extract_once()
    diffs = 0
    for c in cases:
        want, _ = run_live(c["func"], c["in"])
        try:
            shadow_ret = shadow_drive(c["func"], c["in"])
        except BaseException as ex:
            shadow_ret = _bail(type(ex).__name__, str(ex))
        got = {"ret": shadow_ret, "trace": CUR["trace"]}
        if canon(want) != canon(got):
            diffs += 1
            print("SHADOW DIFF %s" % c["func"])
            print("  MIRROR: %s" % json.dumps(got, ensure_ascii=False, default=str))
            print("  LIVE  : %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("SHADOW backup_flow3: %d cases, %d diffs" % (len(cases), diffs))
    sys.exit(1 if diffs else 0)


def main():
    if "--self-test" in sys.argv:
        self_test(); return
    if "--shadow" in sys.argv:
        shadow_mode(); return
    extract_once()
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"): continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        want, _ = run_live(func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False, default=str))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE backup_flow3: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE backup_flow3: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
