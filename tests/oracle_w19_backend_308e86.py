#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_backend.py (gate w19_backend_308e86).

Extracts the live helpers via ast.parse and execs them into a stub-filled globals dict: the
contract error family, the sqlite error predicates and the registration globals are the seams,
modelled identically in C.
"""
import ast
import json
import sqlite3
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = Path("C:/Users/eman5/hermes-agent")
SRC = Q / "persistence" / "sqlite" / "_backend.py"
ERRORS = Q / "persistence" / "sqlite" / "_state" / "errors.py"
GATE = "w19_backend_308e86"


def _literal_assign(path, name, default=None):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                getattr(t, "id", None) == name for t in node.targets):
            try:
                return ast.literal_eval(node.value)
            except ValueError:
                return default
        if isinstance(node, ast.AnnAssign) and getattr(node.target, "id", None) == name:
            try:
                return ast.literal_eval(node.value)
            except ValueError:
                return default
    return default


def _capability_fields():
    """The keyword names/values of `_BASE_CAPABILITIES = PersistenceCapabilities(...)`."""
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                getattr(t, "id", None) == "_BASE_CAPABILITIES" for t in node.targets):
            call = node.value
            return {kw.arg: ast.literal_eval(kw.value) for kw in call.keywords}
    return {}


def _error_class(name):
    return type(name, (Exception,), {})


class ContractError(Exception):
    """Stub of a contract persistence error: its class name is the C `kind`."""


def _contract(name):
    return type(name, (ContractError,), {})


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "w19_backend_live", "__builtins__": __builtins__,
         "sqlite3": sqlite3,
         "_DISK_IO_ERROR_MARKER": _literal_assign(ERRORS, "_DISK_IO_ERROR_MARKER", "disk i/o error"),
         "ValueError": ValueError, "bool": bool}
    # Seams: the sqlite error predicates answer from the fixture flags.
    g["is_disk_full_error"] = lambda exc: bool(getattr(exc, "fixture_disk_full", False))
    g["is_sqlite_lock_error"] = lambda exc: bool(getattr(exc, "fixture_lock", False))
    g["_is_no_more_rows"] = lambda exc: bool(getattr(exc, "fixture_no_more_rows", False))
    g["is_fts_scoped_corruption_error"] = lambda exc: bool(getattr(exc, "fixture_fts", False))
    g["is_malformed_schema_error"] = lambda exc: bool(getattr(exc, "fixture_malformed", False))
    g["classify_persistence_error"] = lambda exc: getattr(exc, "fixture_classify", "unknown")
    for name in ("DiskFull", "StoreContention", "TransientStoreError", "SearchIndexCorrupt",
                 "MalformedSchema", "CorruptStoreError", "StoreUnavailable", "PermanentStoreError",
                 "DeletedWalGeneration", "StoreReplaced", "StoreQuarantined"):
        g[name] = _contract(name)
    for name in ("DeletedWalGenerationError", "StateDbReplacedError", "StateDbCorruptError"):
        g[name] = _error_class(name)
    g["CAPABILITY_ATTRIBUTES"] = _literal_assign(SRC, "CAPABILITY_ATTRIBUTES") or ()
    g["_REPOSITORY_FACTORIES"] = {}
    g["_CAPABILITY_OVERRIDES"] = {}
    g["_BASE_CAPABILITIES"] = types.SimpleNamespace(**_capability_fields())
    g["RepositoryFactory"] = object
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in (
                "_store_error", "register_repository", "translated"):
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        if isinstance(node, ast.ClassDef) and node.name == "SQLiteBackend":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in ("open", "_usable"):
                    sub.decorator_list = []
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), str(SRC), "exec"), g)
    return g


G = extract()


class FakeExc(Exception):
    def __init__(self, message, **flags):
        super().__init__(message)
        for key, value in flags.items():
            setattr(self, "fixture_" + key, value)


def make_exc(kind, name, message, flags):
    if kind == 1:
        return G["DeletedWalGenerationError"](message)
    if kind == 2:
        return G["StateDbReplacedError"](message)
    if kind == 3:
        return G["StateDbCorruptError"](message)
    if kind == 4:
        exc = sqlite3.OperationalError(message) if message else sqlite3.OperationalError()
        for key, value in flags.items():
            setattr(exc, "fixture_" + key, value)
        return exc
    exc_class = type(name or "Exception", (Exception,), {})
    return exc_class(message)


class Holder:
    def __init__(self, registry_owned):
        self._shared_registry_owned = registry_owned


class Backend:
    def __init__(self, keys, read_only, closed):
        self._stores = {k: object() for k in keys}
        self.read_only = read_only
        self._closed = closed

    def _require_open(self):
        if self._closed:
            raise RuntimeError("SQLite backend used after close")


class Scope:
    def __init__(self, key, profile_dir):
        self.hermes_home_key = key
        self.profile_dir = profile_dir


def run_case(r):
    f = r.get("func")
    flags = {"disk_full": bool(r.get("is_disk_full")), "lock": bool(r.get("is_lock")),
             "no_more_rows": bool(r.get("no_more_rows")), "fts": bool(r.get("fts_corrupt")),
             "malformed": bool(r.get("malformed"))}
    if f == "store_error":
        exc = FakeExc(r.get("message") or "", classify=r.get("classify") or "unknown", **flags)
        err = G["_store_error"](exc)
        return {"kind": type(err).__name__, "message": str(err)}
    if f == "translate":
        kind = int(r.get("exc_kind") or 0)
        gen = G["translated"]()
        next(gen)
        if kind == 0:
            gen.close()
            return None
        exc = make_exc(kind, r.get("exc_name"), r.get("message") or "",
                       dict(flags, classify=r.get("classify") or "unknown"))
        try:
            gen.throw(exc)
        except BaseException as raised:  # noqa: BLE001
            answer = {"kind": type(raised).__name__, "message": str(raised)}
            if kind == 5:
                answer["propagated"] = True
            return answer
        return None
    if f == "register_repository":
        G["_REPOSITORY_FACTORIES"].clear()
        for attr in (r.get("already_registered") or []):
            G["_REPOSITORY_FACTORIES"][attr] = object()
        G["_CAPABILITY_OVERRIDES"].clear()
        answer = {}
        try:
            G["register_repository"](r.get("attribute"), lambda store: store,
                                     r.get("capabilities"))
            answer["ok"] = True
        except ValueError as exc:
            answer["ok"] = False
            answer["error"] = str(exc)
        answer["registered"] = list(G["_REPOSITORY_FACTORIES"])
        answer["overrides"] = dict(G["_CAPABILITY_OVERRIDES"])
        return answer
    if f == "open_plan":
        backend = Backend(r.get("store_keys") or [], bool(r.get("read_only")), bool(r.get("closed")))
        scope = Scope(r.get("scope_key"), r.get("profile_dir") or "")
        G["SQLiteStore"] = lambda *a, **k: object()
        try:
            G["open"](backend, scope)
            return {"ok": True}
        except (ValueError, RuntimeError) as exc:
            return {"ok": False, "error": type(exc).__name__, "message": str(exc)}
    if f == "usable":
        holder = Holder(bool(r.get("registry_owned"))) if r.get("has_holder") else None
        backend = Backend([], bool(r.get("read_only")), False)
        return bool(G["_usable"](backend, holder))
    raise KeyError(f)


FUNCS = {"store_error", "translate", "register_repository", "open_plan", "usable"}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in FUNCS:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
