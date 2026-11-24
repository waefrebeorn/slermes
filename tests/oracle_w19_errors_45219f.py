#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_state/errors.py (gate w19_errors_45219f)."""
import ast, json, sqlite3, sys, types, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = Path("C:/Users/eman5/hermes-agent")
if str(Q) not in sys.path:
    sys.path.insert(0, str(Q))
GATE = "w19_errors_45219f"
SRC = Q / "persistence" / "sqlite" / "_state" / "errors.py"
_ANN = __future__.annotations.compiler_flag

import storage_errors as STORAGE  # real seam: the shared phrase/result-code classifier

CLASS_NAMES = {
    "turn_lease": "SessionTurnLeaseLostError",
    "compression_closed": "CompressionSessionClosedError",
    "compression": "CompressionSessionBusyError",
    "compression_in_progress": "SessionCompressionInProgressError",
    "replaced": "StateDbReplacedError",
    "deleted_wal": "DeletedWalGenerationError",
    "corrupt": "StateDbCorruptError",
    "prune_unresolved": "PruneRowUnresolvedError",
    "prune_stale": "PruneRowStaleError",
    "held_row_rewritten": "HeldRowRewrittenError",
    "lease_guard": "SessionActiveWriteGuardError",
}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    _mod = types.ModuleType("state_errors_live")
    _mod.__path__ = []
    sys.modules["state_errors_live"] = _mod
    g = _mod.__dict__
    g.update({"__name__": "state_errors_live", "sqlite3": sqlite3,
              "_MALFORMED_SCHEMA_MARKERS": STORAGE._MALFORMED_SCHEMA_MARKERS,
              "is_sqlite_lock_error": STORAGE.is_sqlite_lock_error,
              "_classify_storage_error": STORAGE.classify_persistence_error})
    wanted_fn = {"_is_no_more_rows", "is_transient_sqlite_error", "is_malformed_schema_error",
                 "classify_persistence_error"}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted_fn:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in set(CLASS_NAMES.values()) | {
                "CompressionSessionClosedError"}:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             flags=_ANN, dont_inherit=True), g)
            except Exception:
                continue
    return g


G = extract()


def mk_exc(kind, msg, code):
    msg = msg or ""
    if kind in (None, "", "none"):
        return None
    if kind == "str":
        return msg
    if kind == "operational":
        e = sqlite3.OperationalError(msg)
    elif kind == "database":
        e = sqlite3.DatabaseError(msg)
    elif kind == "integrity":
        e = sqlite3.IntegrityError(msg)
    elif kind == "programming":
        e = sqlite3.ProgrammingError(msg)
    elif kind == "sqlite_error":
        e = sqlite3.Error(msg)
    elif kind == "runtime":
        e = RuntimeError(msg)
    elif kind in CLASS_NAMES:
        e = G[CLASS_NAMES[kind]](msg)
    else:
        e = RuntimeError(msg)
    if code is not None:
        e.sqlite_errorcode = code
    return e


def run_case(r):
    f = r.get("func")
    msg = r.get("msg") or ""
    code = r.get("code")
    if f == "is_no_more_rows":
        return bool(G["_is_no_more_rows"](msg))
    if f == "is_transient":
        return bool(G["is_transient_sqlite_error"](mk_exc(r.get("kind"), msg, code)))
    if f == "is_malformed_schema":
        return bool(G["is_malformed_schema_error"](mk_exc(r.get("kind"), msg, code)))
    if f == "is_fts_scoped":
        return bool(STORAGE.is_fts_scoped_corruption_error(mk_exc("operational", msg, code)
                                                           if code is not None else msg))
    if f == "classify":
        exc = r.get("exc")
        if isinstance(exc, dict):
            return G["classify_persistence_error"](
                mk_exc(exc.get("kind"), exc.get("msg") or "", exc.get("code")))
        return G["classify_persistence_error"](exc)
    if f == "compression_closed_msg":
        return str(G["CompressionSessionClosedError"](r.get("session_id")))
    return None


def main():
    cases = mism = 0
    known = {"is_no_more_rows", "is_transient", "is_malformed_schema", "is_fts_scoped",
             "classify", "compression_closed_msg"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in known:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
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
