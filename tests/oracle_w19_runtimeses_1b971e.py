#!/usr/bin/env python3
"""Differential oracle for agent/runtime_session_compression.py (gate w19_runtimeses_1b971e)."""
import ast, importlib.util, json, sys, types, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
Q = REPO.parent / "hermes-agent"
SRC = Q / "agent" / "runtime_session_compression.py"
VOCAB = Q / "persistence" / "workers" / "vocabulary.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_runtimeses_1b971e"


class WorkerPersistenceError(RuntimeError):
    pass


def stub(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    return mod


def bootstrap():
    # the worker vocabulary is a leaf module: load the live file, then wire the dotted parents
    spec = importlib.util.spec_from_file_location("persistence.workers.vocabulary", str(VOCAB))
    vocab = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(vocab)
    for pkg in ("persistence", "persistence.workers"):
        if pkg not in sys.modules:
            stub(pkg)
    sys.modules["persistence.workers.vocabulary"] = vocab
    stub("agent.runtime_session_store", apply_row_annotations=lambda messages, annotations: None,
         WorkerPersistenceError=WorkerPersistenceError)


def extract():
    bootstrap()
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    mod = types.ModuleType("rsc_live")
    mod.__dict__["__builtins__"] = __builtins__
    sys.modules["rsc_live"] = mod
    g = mod.__dict__
    g.update(dict(WorkerPersistenceError=WorkerPersistenceError))
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef,)) and node.name == "_handoff_rows":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name == "RuntimeSessionCompressionMixin":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
MIXIN = G["RuntimeSessionCompressionMixin"]


class Host(MIXIN):
    """The mixin's host: the owner RPC (_apply) is captured, everything else is the seam."""

    def __init__(self, replies=None, scope=None):
        self.replies = replies or {}
        self.scope = scope or {"session_id": "s-0"}
        self.payloads = {}
        self.context_rewrites = 0

    def _session(self, session_id):
        return None

    def _apply(self, op, payload=None):
        self.payloads[op] = payload
        return self.replies.get(op, {})


def run_case(r):
    f = r.get("func")
    if f == "handoff_rows":
        return G["_handoff_rows"](r.get("messages") or [])
    if f == "cas_rows":
        host = Host({"compression.archive": {"annotations": [], "value": "done"}})
        host.archive_and_compact("s-0", [], held_for_cas=r.get("held") or [])
        return host.payloads["compression.archive"]["held_for_cas"]
    if f == "declared_scope_identity":
        host = Host({"compression.lineage": {"identity": r.get("identity") or []}})
        return list(host.declared_scope_identity("s-0"))
    if f == "is_explicit_fork_child":
        host = Host({"compression.lineage": {"identity": r.get("identity") or []}})
        return host.is_explicit_fork_child("s-0")
    if f == "receipt_journal":
        host = Host(scope=r.get("scope") or {"session_id": "s-0"})
        try:
            return host._compression_receipt_journal(dict(r.get("candidate") or {}),
                                                     dict(r.get("result") or {}))
        except WorkerPersistenceError as e:
            return {"error": str(e)}
        except KeyError as e:
            return {"error": "KeyError: %r" % (e.args[0],)}
    if f == "fallback_streak":
        host = Host()
        try:
            host.set_compression_fallback_streak("s-0", r.get("streak"))
        except TypeError:
            return {"error": "TypeError"}
        except ValueError:
            return {"error": "ValueError"}
        return host.payloads["compression.fallback"]["value"]
    if f == "ineffective_count":
        host = Host()
        try:
            host.set_compression_ineffective_count("s-0", r.get("count"))
        except TypeError:
            return {"error": "TypeError"}
        except ValueError:
            return {"error": "ValueError"}
        return host.payloads["compression.ineffective"]["value"]
    if f == "recovery_deadline":
        host = Host()
        host.set_compression_recovery_deadline("s-0", r.get("deadline"))
        return host.payloads["compression.recovery"]
    if f == "session_readable":
        scope = {"session_id": r.get("scope_session_id", "s-0")}
        host = Host({"compression.lineage": {"readable_ids": r.get("readable_ids") or []},
                     "compression.context": {"session": {"id": r.get("requested", "")}}}, scope)
        try:
            host.get_session(r.get("requested", ""))
        except WorkerPersistenceError as e:
            return {"error": str(e)}
        return {"allowed": True}
    return None


FUNCS = {"handoff_rows", "cas_rows", "declared_scope_identity", "is_explicit_fork_child",
         "receipt_journal", "fallback_streak", "ineffective_count", "recovery_deadline",
         "session_readable"}


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
            print("MISMATCH %s args=%r py=%r c=%r" % (f, r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or not cases) else 0)


if __name__ == "__main__":
    main()
