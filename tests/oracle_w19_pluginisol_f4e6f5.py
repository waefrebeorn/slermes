#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_isolation_audit.py (gate w19_pluginisol_f4e6f5)."""
import ast
import json
import sys
import tempfile
import types
from dataclasses import dataclass, field
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
Q = REPO.parent / "hermes-agent"
SRC = Q / "hermes_cli" / "plugin_isolation_audit.py"
TABLES = Q / "hermes_cli" / "plugin_isolation.py"
GATE = "w19_pluginisol_f4e6f5"
FUNCS = {"hermes_module", "dedupe", "python_files", "ctx_method", "hook_note", "registry_call",
         "patch_reason", "setattr_reason", "manifest_kind_reason", "streaming_marker", "report"}
WANT_ASSIGN = {"HERMES_PACKAGES", "_DIRECT_REGISTRY_CALLS", "_PROVIDERS_MODULES",
               "_MANIFEST_KIND_REASONS", "_STREAMING_MARKERS", "_SKIP_DIRS",
               "VERDICT_HOST", "VERDICT_IN_PROCESS", "VERDICT_PORTABLE"}
WANT_FUNCS = {"_python_files", "_dedupe", "_read_manifest"}
WANT_CLASSES = {"IsolationReport", "_SourceVisitor"}


def literal_tables():
    tree = ast.parse(TABLES.read_text(encoding="utf-8"))
    found = {}
    for node in tree.body:
        if isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            name = node.target.id
            if name in {"HOST_UNSUPPORTED_CTX_METHODS", "HOST_SKIPPED_CTX_METHODS",
                        "HOST_DEGRADED_HOOKS", "HOST_OBJECT_BASES"}:
                found[name] = ast.literal_eval(node.value)
        elif isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id in {
                        "HOST_UNSUPPORTED_CTX_METHODS", "HOST_SKIPPED_CTX_METHODS",
                        "HOST_DEGRADED_HOOKS", "HOST_OBJECT_BASES"}:
                    found[t.id] = ast.literal_eval(node.value)
    return found


def load_live():
    tables = literal_tables()
    for name in ("hermes_cli", "hermes_agent", "hermes_agent.compat"):
        mod = types.ModuleType(name)
        mod.__path__ = []
        sys.modules.setdefault(name, mod)
    compat = types.ModuleType("hermes_agent.compat.paths")
    compat.current_module = lambda m: m
    sys.modules["hermes_agent.compat.paths"] = compat
    pi = types.ModuleType("hermes_cli.plugin_isolation")
    for k, v in tables.items():
        setattr(pi, k, v)
    sys.modules["hermes_cli.plugin_isolation"] = pi

    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "pia_live", "ast": ast, "Path": Path, "dataclass": dataclass, "field": field,
         "Any": object, "Dict": object, "List": object, "Mapping": object, "Optional": object,
         "Set": object, "current_module": compat.current_module}
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(getattr(t, "id", None) in WANT_ASSIGN for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        elif isinstance(node, ast.AnnAssign) and getattr(node.target, "id", None) in WANT_ASSIGN:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        elif isinstance(node, ast.FunctionDef) and node.name in WANT_FUNCS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        elif isinstance(node, ast.ClassDef) and node.name in WANT_CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
    g["_TABLE_MOD"] = pi
    return g


G = load_live()


def visitor(rel, aliases=None, model_provider=False):
    v = G["_SourceVisitor"](rel, model_provider=model_provider)
    if aliases:
        v.aliases.update(aliases)
    return v


def padded(src, line):
    return "\n" * max(0, int(line) - 1) + src


def run_case(r):
    f = r.get("func")
    if f == "hermes_module":
        return G["_SourceVisitor"]("x")._hermes(r.get("module") or "")
    if f == "dedupe":
        return G["_dedupe"](list(r.get("items") or []))
    if f == "python_files":
        paths = [p for p in (r.get("paths") or []) if p.endswith(".py")]
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            for p in paths:
                fp = root / p
                fp.parent.mkdir(parents=True, exist_ok=True)
                fp.write_text("", encoding="utf-8")
            got = [Path(x).relative_to(root).as_posix() for x in G["_python_files"](root)]
        return sorted(got)
    if f == "ctx_method":
        name, rel, line = r.get("name"), r.get("rel"), r.get("line", 1)
        v = visitor(rel)
        v.visit(ast.parse(padded("_ctx.%s()" % name, line)))
        return {"reason": v.reasons[0] if v.reasons else None,
                "note": v.notes[0] if v.notes else None,
                "registers": name in v.ctx_methods}
    if f == "hook_note":
        v = visitor("plugin.py")
        v.visit(ast.parse("_ctx.register_hook(%r)" % (r.get("hook"),)))
        return v.notes[0] if v.notes else None
    if f == "registry_call":
        v = visitor(r.get("rel") or "", model_provider=bool(r.get("model_provider")))
        v._flag_registry_call(r.get("target"), int(r.get("line", 0)))
        return v.reasons[0] if v.reasons else None
    if f == "patch_reason":
        owner, attr, rel, line = r.get("owner"), r.get("attr"), r.get("rel"), r.get("line", 1)
        v = visitor(rel, aliases={"m": owner})
        v.visit(ast.parse(padded("m.%s = 1" % attr, line)))
        return v.reasons[0] if v.reasons else None
    if f == "setattr_reason":
        owner, rel, line = r.get("owner"), r.get("rel"), r.get("line", 1)
        v = visitor(rel, aliases={"m": owner})
        v.visit(ast.parse(padded("setattr(m, 'x', 1)", line)))
        return v.reasons[0] if v.reasons else None
    if f == "manifest_kind_reason":
        kind = r.get("kind")
        key = str(kind or "").strip().lower()
        return G["_MANIFEST_KIND_REASONS"].get(key)
    if f == "streaming_marker":
        src = r.get("source") or ""
        for marker in G["_STREAMING_MARKERS"]:
            if marker in src:
                return marker
        return None
    if f == "report":
        rep = G["IsolationReport"](r.get("verdict"), list(r.get("reasons") or []),
                                   list(r.get("notes") or []), set(r.get("ctx_methods") or []))
        return {"dict": rep.to_dict(), "summary": rep.summary()}
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
        if f == "python_files":
            py = sorted(x.replace("\\", "/") for x in py)
            got = sorted(x.replace("\\", "/") for x in (got or []))
        if f == "ctx_method" and py:
            py = {"reason": py["reason"], "note": py["note"], "registers": py["registers"]}
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
