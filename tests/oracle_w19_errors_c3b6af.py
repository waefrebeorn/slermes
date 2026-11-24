#!/usr/bin/env python3
"""Differential oracle for persistence/core/errors.py (gate w19_errors_c3b6af)."""
import importlib.util
import inspect
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "persistence" / "core" / "errors.py"
GATE = "w19_errors_c3b6af"
FUNCS = {"one_line", "cause", "is_kind", "in_group", "message", "busy_refusal", "rewind_text"}


def load_live():
    for name, path in (("persistence", []), ("persistence.core", [])):
        mod = types.ModuleType(name)
        mod.__path__ = list(path)
        sys.modules[name] = mod
    ident = types.ModuleType("persistence.core.identity")
    ident.SessionId = str
    ident.JsonValue = object
    sys.modules["persistence.core.identity"] = ident
    spec = importlib.util.spec_from_file_location("persistence.core.errors", str(SRC))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


LIVE = load_live()


def build(cls, a):
    a = dict(a or {})
    if cls.__name__ in {"TransientStoreError", "StoreContention", "DiskFull", "PermanentStoreError",
                        "CorruptStoreError", "StoreQuarantined", "MalformedSchema", "SearchIndexCorrupt",
                        "StoreUnavailable", "StoreReplaced", "DeletedWalGeneration", "PersistenceError"}:
        if cls.__name__ == "StoreContention":
            return cls(a.get("message", ""), reason=a.get("reason", "busy"))
        if cls.__name__ == "TransientStoreError":
            return cls(a.get("message", ""), reason=a.get("reason"))
        return cls(a.get("message", "")) if a.get("message") is not None else cls()
    sig = inspect.signature(cls.__init__)
    params = [p for name, p in list(sig.parameters.items())[1:]]
    kwargs = {}
    for p in params:
        if p.kind == inspect.Parameter.VAR_POSITIONAL:
            continue
        if p.name in a:
            val = a[p.name]
        elif p.default is not inspect.Parameter.empty:
            continue
        else:
            raise KeyError("missing constructor arg %s for %s" % (p.name, cls.__name__))
        if p.annotation is not inspect.Parameter.empty and "StaleFenceKind" in str(p.annotation):
            val = LIVE.StaleFenceKind(val)
        kwargs[p.name] = val
    if cls.__name__ == "FenceStale":
        kwargs["kind"] = LIVE.StaleFenceKind(a["kind"])
    return cls(**kwargs)


def run_case(r):
    f = r.get("func")
    if f == "one_line":
        return LIVE._one_line(r.get("value"), r.get("limit", 80))
    if f == "cause":
        return getattr(LIVE, r["cls"]).cause
    if f == "is_kind":
        cls, base = getattr(LIVE, r["cls"]), getattr(LIVE, r["base"])
        return issubclass(cls, base)
    if f == "in_group":
        g = r.get("group")
        groups = {
            "store_failures": LIVE.STORE_FAILURES,
            "mutation_guards": LIVE.MUTATION_GUARD_REFUSALS,
        }
        cls = getattr(LIVE, r["cls"])
        if g in groups:
            return any(issubclass(cls, t) for t in groups[g])
        if g == "cause":
            return cls.cause in LIVE.PERSISTENCE_ERROR_CAUSES
        roots = {"persistence_error": "PersistenceError", "transient": "TransientStoreError",
                 "permanent": "PermanentStoreError", "authority": "AuthorityRefusal",
                 "conflict": "Conflict", "not_found": "NotFound", "corrupt": "CorruptStoreError",
                 "replaced": "StoreReplaced"}
        return issubclass(cls, getattr(LIVE, roots[g]))
    if f == "message":
        return str(build(getattr(LIVE, r["cls"]), r.get("args")))
    if f == "busy_refusal":
        ref = LIVE.busy_refusal(build(getattr(LIVE, r["exc"]), r.get("args")))
        return {"reason": ref.reason, "field": ref.field}
    if f == "rewind_text":
        return LIVE._REWIND_REFUSAL_TEXT.get(r.get("reason"))
    return None


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
