#!/usr/bin/env python3
"""Oracle for backup_flow2 — the STILL-UNCLAIMED remainder of backup.py.

Ports covered (12 FLOW cores; a CRLF-aware PoP grep at authoring time showed
33 of backup.py's functions already claimed by backup.c / backup_gaps.c):
    snapshot_recovery_hint, print_capped, import_skipped, import_member_rel,
    prune_prefixed_zips, prune_incomplete_zips, write_zip_file,
    find_corrupt_members, snapshot_epochs_restorable,
    create_pre_update_snapshots_all_profiles,
    restore_config_model_settings_all_profiles, restore_cron_jobs_all_profiles.

Drives the LIVE defs AST-extracted from the quarry backup.py (no heavy import:
backup.py imports `persistence` which is absent from the venv). Every effectful
collaborator is replaced by a recording stub returning the SAME scripted
results the C harness feeds its universal seam: a MISSING key fail-closes with
RuntimeError on both sides, a present JSON null is a valid None, and an array
script yields one result PER CALL (last repeats). Scripted {"__raise__": cls}
raises a REAL builtin of that class (full py3.14 parent hierarchy, including
the BaseException trio), so KeyboardInterrupt/SystemExit/GeneratorExit pass
through every `except Exception` exactly as in the C port. Args/ret are
DEEP-COPIED into the trace at call time, matching the C harness's json_copy
snapshots.

Input: stdin JSON lines from t_port_backup_flow2:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE backup_flow2: N cases, M mismatches. Fail-closed (exit 2) when
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
import types

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

from pathlib import Path
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "backup_flow2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "backup.py")

# harness func -> live python name
PY = {
    "snapshot_recovery_hint": "_snapshot_recovery_hint",
    "print_capped": "_print_capped",
    "import_skipped": "_import_skipped",
    "import_member_rel": "_import_member_rel",
    "prune_prefixed_zips": "_prune_prefixed_zips",
    "prune_incomplete_zips": "_prune_incomplete_zips",
    "write_zip_file": "_write_zip_file",
    "find_corrupt_members": "_find_corrupt_members",
    "snapshot_epochs_restorable": "_snapshot_epochs_restorable",
    "create_pre_update_snapshots_all_profiles": "create_pre_update_snapshots_all_profiles",
    "restore_config_model_settings_all_profiles": "restore_config_model_settings_all_profiles",
    "restore_cron_jobs_all_profiles": "restore_cron_jobs_all_profiles",
}
WANTED = set(PY.values())
# inline ported siblings (transcribed in C, no PoP re-claim): needed as the
# LIVE reference for the composed flows.
EXTRA = {"_newest_first", "_prune_oldest"}

CUR = {"trace": [], "scripts": {}, "idx": {}}
NS = {}


# ── seam recorder ──

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


# Full builtin exception hierarchy. Kinds whose __name__ must carry a module
# prefix (zlib.error) get a synthetic class with that exact __name__.
_zlib_error = type("zlib.error", (Exception,), {})
_lzma_error = type("LZMAError", (Exception,), {})
_bad_zip = type("BadZipFile", (Exception,), {})

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
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        raise cls(r.get("msg", ""))
    return r


# ── extraction ──

def extract():
    from typing import Any, Callable, Optional, Dict, List, Tuple

    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "hermes_cli.backup_flow2_extracted",
        "json": json, "os": os, "sys": sys,
        "Any": Any, "Callable": Callable, "Optional": Optional,
        "Dict": Dict, "List": List, "Tuple": Tuple,
        "Path": PathDouble,
        # module constants (literal + imported)
        "_IMPORT_SKIP_NAMES": {"gateway_state.json", "gateway.pid", "cron.pid",
                               "gateway.lock", "processes.json"},
        "PM_RUNTIME_ROOT_DIRS": {"models", "runtimes", "node", "installs", "tools"},
        "_SQLITE_SIDECAR_SUFFIXES": (".db-wal", ".db-shm", ".db-journal"),
        "_EXTERNAL_PREFIX": "_external/",
        "_INCOMPLETE_ZIP_SUFFIX": ".incomplete.zip",
        "_ZIP_MEMBER_READ_ERRORS": (OSError, _bad_zip, _zlib_error, EOFError, _lzma_error),
        # extracted-into stubs (installed per-case in install_stubs, bound here
        # so the extracted defs compile against real names)
        "print": lambda *a, **k: None,
        "logger": _silent_logger(),
    }
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and \
                node.name in (WANTED | EXTRA):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    missing = (WANTED | EXTRA) - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    global NS
    NS = ns
    return ns


# ── doubles / stubs ──

class PathDouble:
    """Path projection: tag used for seam args; name/suffix/is_file from fixture."""
    def __init__(self, tag, name=None, is_file=False, rname=None):
        self.tag = tag
        self._name = name if name is not None else tag
        self._is_file = is_file
        self._rname = rname if rname is not None else self._name

    @property
    def name(self):
        return self._name

    @property
    def suffix(self):
        i = self._name.rfind(".")
        return self._name[i:] if i > 0 else ""

    def is_file(self):
        return bool(self._is_file)

    def exists(self):
        return seam("path_exists", {"path": self.tag})

    def iterdir(self):
        ents = seam("path_iterdir", {"path": self.tag})
        return [PathDouble(e.get("name", ""), e.get("name", ""),
                           e.get("is_file", False)) for e in (ents or [])]

    def resolve(self):
        # project the resolved name separately (entry[1].resolve().name)
        return PathDouble(self.tag, self._rname, self._is_file, self._rname)

    @staticmethod
    def unlink(p):
        return seam("unlink", {"path": p.name})

    def __str__(self):
        return self.tag


class _LenDouble:
    """Make len(zf.filelist) fire the zip_filelist_len seam."""
    def __init__(self, tag):
        self.tag = tag

    def __len__(self):
        v = seam("zip_filelist_len", {"zf": self.tag})
        return int(v) if isinstance(v, (int, float)) else 0


class _ReadDouble:
    def __init__(self, member):
        self.member = member

    def read(self, n):
        return seam("zip_read", {"member": self.member})


class _ZipOpenCM:
    def __init__(self, zf, member):
        self.zf = zf
        self.member = member

    def __enter__(self):
        seam("zip_open", {"zf": self.zf, "member": self.member})
        return _ReadDouble(self.member)

    def __exit__(self, et, ev, tb):
        seam("zip_open_exit", {"member": self.member})
        return False


class ZipDouble:
    def __init__(self, tag):
        self.tag = tag
        self.filelist = _LenDouble(tag)

    def write(self, path, arcname):
        return seam("zip_write", {"path": str(path), "arcname": arcname})

    def open(self, member):
        return _ZipOpenCM(self.tag, member)


def _silent_logger():
    class L:
        def _rec(self, se, fmt, args):
            norm = [str(x) if isinstance(x, BaseException) else x for x in args]
            seam(se, {"fmt": fmt, "args": norm})
        def debug(self, fmt, *args): self._rec("logger_debug", fmt, args)
        def warning(self, fmt, *args): self._rec("logger_warning", fmt, args)
        def error(self, fmt, *args): self._rec("logger_error", fmt, args)
        def info(self, fmt, *args): self._rec("logger_info", fmt, args)
    return L()


def install_stubs(ns):
    ns["print"] = lambda *a, **k: seam("print", {"msg": " ".join(str(x) for x in a)})
    ns["logger"] = _silent_logger()
    ns["normalize_archive_parts"] = lambda rel: seam("normalize_archive_parts", {"rel": rel})
    ns["profile_root_entry"] = lambda parts: seam("profile_root_entry", {"parts": list(parts)})
    ns["_discard_failed_zip_members"] = lambda zf, n: seam(
        "discard_failed_zip_members", {"filelist_len": n})
    ns["_restore_destination_epoch"] = lambda p: seam(
        "restore_destination_epoch", {"path": str(p)})
    ns["_sibling_profile_homes"] = lambda home: seam("sibling_profile_homes", {"home": home})

    def cqs(*, label, hermes_home, keep, max_file_size):
        return copy.deepcopy(seam("create_quick_snapshot", {"label": label, "hermes_home": hermes_home,
                                                            "keep": keep, "max_file_size": max_file_size}))
    ns["create_quick_snapshot"] = cqs

    def rcfg(snapshot_id, hermes_home=None):
        # C's universal seam returns json_copy(sc) per call: deep-copy so the
        # live `result["profile"] = name` write cannot leak into the script or
        # into a later call's trace ret (matches json_copy ownership).
        return copy.deepcopy(seam("restore_config_model_settings_if_rewritten",
                                   {"snapshot_id": snapshot_id, "hermes_home": hermes_home}))
    ns["restore_config_model_settings_if_rewritten"] = rcfg

    def rcron(snapshot_id, hermes_home=None):
        return copy.deepcopy(seam("restore_cron_jobs_if_emptied",
                                   {"snapshot_id": snapshot_id, "hermes_home": hermes_home}))
    ns["restore_cron_jobs_if_emptied"] = rcron


# ── live drive ──

def drive(ns, func, inp):
    if func == "snapshot_recovery_hint":
        return ns["_snapshot_recovery_hint"]()
    if func == "print_capped":
        return ns["_print_capped"](inp.get("header", ""), inp.get("lines", []), inp.get("indent", ""))
    if func == "import_skipped":
        return ns["_import_skipped"](inp.get("rel", ""))
    if func == "import_member_rel":
        return ns["_import_member_rel"](inp.get("member", ""), inp.get("prefix", ""))
    if func in ("prune_prefixed_zips", "prune_incomplete_zips"):
        root = PathDouble(inp.get("backup_dir", ""))
        if func == "prune_prefixed_zips":
            return ns["_prune_prefixed_zips"](root, inp.get("prefix", ""),
                                              inp.get("keep", 0), inp.get("what", ""))
        return ns["_prune_incomplete_zips"](root, inp.get("prefix", ""), inp.get("what", ""))
    if func == "write_zip_file":
        zf = ZipDouble(inp.get("zf", ""))
        return ns["_write_zip_file"](zf, inp.get("path", ""), inp.get("arcname", ""))
    if func == "find_corrupt_members":
        zf = ZipDouble(inp.get("zf", ""))
        return ns["_find_corrupt_members"](zf, inp.get("members", []))
    if func == "snapshot_epochs_restorable":
        class _BoolPath:
            """Live checks entry[0].exists() on a Path; project the scripted
            bool through a Path-like so the method is present."""
            def __init__(self, v): self.v = v
            def exists(self): return bool(self.v)

        def ep(rel):
            e = seam("entry_paths", {"rel": rel})
            if not e:
                return None
            return (_BoolPath(e[0]), PathDouble(e[1], e[1], False, e[2]),
                    PathDouble(e[2], e[2], False, e[2]))
        return ns["_snapshot_epochs_restorable"](inp.get("files", []), ep)
    if func == "create_pre_update_snapshots_all_profiles":
        return ns["create_pre_update_snapshots_all_profiles"](
            invoking_home=inp.get("invoking_home"), keep=inp.get("keep"),
            max_file_size=inp.get("max_file_size"))
    if func == "restore_config_model_settings_all_profiles":
        return ns["restore_config_model_settings_all_profiles"](
            inp.get("profile_snapshots", {}), invoking_home=inp.get("invoking_home"))
    if func == "restore_cron_jobs_all_profiles":
        return ns["restore_cron_jobs_all_profiles"](
            inp.get("profile_snapshots", {}), invoking_home=inp.get("invoking_home"))
    raise ValueError("unhandled func: %s" % func)


# ── comparison ──

def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def reset_case(inp):
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams", {})
    CUR["idx"] = {}


def run_live(func, inp):
    reset_case(inp)
    try:
        ret = drive(NS, func, inp)
        return {"ret": ret, "trace": CUR["trace"]}, None
    except BaseException as ex:
        return {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                "trace": CUR["trace"]}, ex


def fixture():
    return _REPO / "tests" / "fixtures" / "backup_flow2.jsonl"


def load_cases():
    p = fixture()
    if not p.exists():
        return []
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
    "print", "normalize_archive_parts", "profile_root_entry", "path_exists",
    "path_iterdir", "unlink", "logger_warning", "zip_filelist_len", "zip_write",
    "discard_failed_zip_members", "zip_open", "zip_read", "zip_open_exit",
    "entry_paths", "restore_destination_epoch", "logger_error",
    "sibling_profile_homes", "create_quick_snapshot", "logger_debug",
    "restore_config_model_settings_if_rewritten", "restore_cron_jobs_if_emptied",
]


def self_test():
    ns = extract()
    install_stubs(ns)
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


# ── shadow-only seam variants ──
# The live drive catches exceptions inside the extracted function's own
# try/except blocks, so the mirror must NOT raise at the recorder. sseam
# returns the raise descriptor as a value; sseam_raise is for fire-and-forget
# effect seams (print / logger / CM __exit__) whose raise propagates live.
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


# ── shadow mirror (literal transcription of the C port) ──

CONST_HINT = ("To restore a newer snapshot, start `hermes` in a terminal and run `/snapshot list`, then "
              "`/snapshot restore <id>` (CLI only).")


def _bail(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def _is_exc(r):
    if not (isinstance(r, dict) and "__raise__" in r):
        return False
    k = r["__raise__"]
    if k in ("SystemExit", "KeyboardInterrupt", "GeneratorExit", "BaseException",
             "BaseExceptionGroup"):
        return False
    return True


def _caught(r, cls):
    if not (isinstance(r, dict) and "__raise__" in r):
        return False
    k = r["__raise__"]
    chain = {
        "ValueError": "Exception", "TypeError": "Exception", "RuntimeError": "Exception",
        "AttributeError": "Exception", "OSError": "Exception", "EOFError": "Exception",
        "BadZipFile": "Exception", "zlib.error": "Exception", "LZMAError": "Exception",
        "PermissionError": "OSError", "FileNotFoundError": "OSError",
        "IndexError": "LookupError", "KeyError": "LookupError", "LookupError": "Exception",
        "Exception": "BaseException", "SystemExit": "BaseException",
        "KeyboardInterrupt": "BaseException", "GeneratorExit": "BaseException",
        "BaseException": None,
    }
    while k:
        if k == cls:
            return True
        k = chain.get(k, "Exception")
    return False


def shadow_drive(func, inp):
    """Literal Python MIRROR of the bk2_* C port; fires the SAME seams in the
    same order with the same args. Diffed against the live drive by --shadow."""
    reset_case(inp)
    if func == "snapshot_recovery_hint":
        return CONST_HINT
    if func == "print_capped":
        sseam_raise("print", {"msg": inp.get("header", "")})
        lines = inp.get("lines", [])
        ind = inp.get("indent", "")
        for line in lines[:10]:
            sseam_raise("print", {"msg": ind + line})
        if len(lines) > 10:
            sseam_raise("print", {"msg": "%s... and %d more" % (ind, len(lines) - 10)})
        return None

    if func == "import_skipped":
        rel = inp.get("rel", "")
        parts = sseam("normalize_archive_parts", {"rel": rel})
        if isinstance(parts, dict) and "__raise__" in parts:
            if _caught(parts, "ValueError"):
                return False
            return parts
        if not parts:
            return _bail("IndexError", "list index out of range")
        last = parts[-1]
        if last in ("gateway_state.json", "gateway.pid", "cron.pid", "gateway.lock", "processes.json"):
            return True
        proot = sseam("profile_root_entry", {"parts": parts})
        if isinstance(proot, dict) and "__raise__" in proot:
            return proot
        if proot in ("models", "runtimes", "node", "installs", "tools"):
            return True
        for suf in (".db-wal", ".db-shm", ".db-journal"):
            if rel.endswith(suf):
                return True
        return False

    if func == "import_member_rel":
        member = inp.get("member", "")
        prefix = inp.get("prefix", "")
        if member.startswith("_external/"):
            return [member[len("_external/"):], False]
        rel = member[len(prefix):] if prefix and member.startswith(prefix) else member
        CUR["scripts"] = dict(CUR["scripts"])
        # inline the import_skipped port against the same scripts
        parts = sseam("normalize_archive_parts", {"rel": rel})
        if isinstance(parts, dict) and "__raise__" in parts:
            if _caught(parts, "ValueError"):
                return [rel, False]
            return parts
        last = parts[-1]
        if last in ("gateway_state.json", "gateway.pid", "cron.pid", "gateway.lock", "processes.json"):
            return [rel, True]
        proot = sseam("profile_root_entry", {"parts": parts})
        if isinstance(proot, dict) and "__raise__" in proot:
            return proot
        sk = proot in ("models", "runtimes", "node", "installs", "tools")
        if not sk:
            for suf in (".db-wal", ".db-shm", ".db-journal"):
                if rel.endswith(suf):
                    sk = True
                    break
        return [rel, sk]

    if func in ("prune_prefixed_zips", "prune_incomplete_zips"):
        root = inp.get("backup_dir", "")
        prefix = inp.get("prefix", "")
        what = inp.get("what", "")
        ex = sseam("path_exists", {"path": root})
        if isinstance(ex, dict) and "__raise__" in ex:
            return ex
        if not ex:
            return 0
        ents = sseam("path_iterdir", {"path": root})
        if isinstance(ents, dict) and "__raise__" in ents:
            return ents
        inc = ".incomplete.zip"
        names = []
        for e in ents:
            n = e.get("name", "")
            f = bool(e.get("is_file", False))
            if func == "prune_prefixed_zips":
                keep = (f and n.startswith(prefix) and n.lower().endswith(".zip")
                        and not n.lower().endswith(inc))
            else:
                keep = f and n.startswith(prefix) and n.lower().endswith(inc)
            if keep:
                names.append(n)
        names.sort(reverse=True)
        k = int(inp.get("keep", 0)) if func == "prune_prefixed_zips" else 1
        w = what if func == "prune_prefixed_zips" else "incomplete " + what
        deleted = 0
        for n in names[k:]:
            r = sseam("unlink", {"path": n})
            if isinstance(r, dict) and "__raise__" in r:
                if _caught(r, "OSError"):
                    sseam_raise("logger_warning", {"fmt": "Failed to prune %s %s: %s",
                                            "args": [w, n, r.get("msg", "")]})
                    continue
                return r
            deleted += 1
        return deleted

    if func == "write_zip_file":
        zf = inp.get("zf", "")
        n = sseam("zip_filelist_len", {"zf": zf})
        if isinstance(n, dict) and "__raise__" in n:
            return n
        flen = n if isinstance(n, (int, float)) else 0
        r = sseam("zip_write", {"path": inp.get("path", ""), "arcname": inp.get("arcname", "")})
        if isinstance(r, dict) and "__raise__" in r:
            if _is_exc(r):
                d = sseam("discard_failed_zip_members", {"filelist_len": flen})
                if isinstance(d, dict) and "__raise__" in d:
                    return d
            return r
        return None

    if func == "find_corrupt_members":
        zf = inp.get("zf", "")
        bad = []
        for m in inp.get("members", []):
            op = sseam("zip_open", {"zf": zf, "member": m})
            if isinstance(op, dict) and "__raise__" in op:
                if (_caught(op, "OSError") or _caught(op, "EOFError") or
                        op.get("__raise__") in ("BadZipFile", "zlib.error", "LZMAError")):
                    bad.append("%s: %s" % (m, op.get("msg", "")))
                    continue
                return op
            exited = False
            while True:
                rd = sseam("zip_read", {"member": m})
                if isinstance(rd, dict) and "__raise__" in rd:
                    ex = sseam_raise("zip_open_exit", {"member": m})
                    exited = True
                    if isinstance(ex, dict) and "__raise__" in ex:
                        return ex
                    if (_caught(rd, "OSError") or _caught(rd, "EOFError") or
                            rd.get("__raise__") in ("BadZipFile", "zlib.error", "LZMAError")):
                        bad.append("%s: %s" % (m, rd.get("msg", "")))
                        break
                    return rd
                if not rd:
                    break
            if not exited:
                ex = sseam_raise("zip_open_exit", {"member": m})
                if isinstance(ex, dict) and "__raise__" in ex:
                    return ex
        return bad

    if func == "snapshot_epochs_restorable":
        for rel in inp.get("files", []):
            e = sseam("entry_paths", {"rel": rel})
            if isinstance(e, dict) and "__raise__" in e:
                return e
            exists = bool(e[0]) if e else False
            name = e[1] if e else ""
            rname = e[2] if e else ""
            if exists and (name == "state.db" or rname == "state.db"):
                r = sseam("restore_destination_epoch", {"path": name})
                if isinstance(r, dict) and "__raise__" in r:
                    if _caught(r, "OSError"):
                        sseam_raise("logger_error", {"fmt": "%s", "args": [r.get("msg", "")]})
                        return False
                    return r
        return True

    if func == "create_pre_update_snapshots_all_profiles":
        home = inp.get("invoking_home")
        keep = inp.get("keep")
        mx = inp.get("max_file_size")
        results = {}
        homes = sseam("sibling_profile_homes", {"home": home})
        if isinstance(homes, dict) and "__raise__" in homes:
            return homes
        for pair in homes:
            name, phome = pair[0], pair[1]
            r = sseam("create_quick_snapshot", {"label": "pre-update", "hermes_home": phome,
                                               "keep": keep, "max_file_size": mx})
            if isinstance(r, dict) and "__raise__" in r:
                if _is_exc(r):
                    sseam_raise("logger_debug", {"fmt": "Pre-update snapshot for profile %s failed: %s",
                                          "args": [name, r.get("msg", "")]})
                    continue
                return r
            if r:
                results[name] = r
        return results

    if func in ("restore_config_model_settings_all_profiles", "restore_cron_jobs_all_profiles"):
        snaps = inp.get("profile_snapshots", {})
        home = inp.get("invoking_home")
        restored = []
        if not snaps:
            return restored
        homes = sseam("sibling_profile_homes", {"home": home})
        if isinstance(homes, dict) and "__raise__" in homes:
            return homes
        by_name = {p[0]: p[1] for p in homes}
        if func == "restore_config_model_settings_all_profiles":
            sname = "restore_config_model_settings_if_rewritten"
            fmt = "Config model-settings restore check for profile %s failed: %s"
        else:
            sname = "restore_cron_jobs_if_emptied"
            fmt = "Cron restore check for profile %s failed: %s"
        for name, snapid in snaps.items():
            phome = by_name.get(name)
            if phome is None:
                continue
            r = sseam(sname, {"snapshot_id": snapid, "hermes_home": phome})
            if isinstance(r, dict) and "__raise__" in r:
                if _is_exc(r):
                    sseam_raise("logger_debug", {"fmt": fmt, "args": [name, r.get("msg", "")]})
                    continue
                return r
            if r:
                r = dict(r)
                r["profile"] = name
                restored.append(r)
        return restored

    raise ValueError("no shadow for %s" % func)


def shadow_mode():
    cases = load_cases()
    check_func_set(cases)
    ns = extract()
    install_stubs(ns)
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
            print("  MIRROR: %s" % json.dumps(got, ensure_ascii=False))
            print("  LIVE  : %s" % json.dumps(want, ensure_ascii=False))
    print("SHADOW backup_flow2: %d cases, %d diffs" % (len(cases), diffs))
    sys.exit(1 if diffs else 0)


def main():
    if "--self-test" in sys.argv:
        self_test()
        return
    if "--shadow" in sys.argv:
        shadow_mode()
        return
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
        want, _ = run_live(func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE backup_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE backup_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
