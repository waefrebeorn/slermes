#!/usr/bin/env python3
"""Differential oracle for plugins/google_meet/node/registry.py
(gate w21_registry_5bbc35).

The live `NodeRegistry` class is lifted out of the source with the AST
extractor and driven on the SAME temp file the harness used: relative paths
resolve under $HOME, which the gate runner points at an isolated directory, so
a case can never touch a real registry. `read_json` is the live helper (lifted
from plugins/google_meet/_jsonfile.py); `atomic_json_write` -- the file-write
seam -- is a faithful writer (json.dumps indent=2, utf-8) and `time.time` is
the injected clock. Everything else (validation, merge order, sorting, resolve
ambiguity) is the class's own code.
"""
import __future__
import ast
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "google_meet" / "node" / "registry.py"
JSONFILE = QUARRY / "plugins" / "google_meet" / "_jsonfile.py"
GATE = "w21_registry_5bbc35"
_ANN = __future__.annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

HOME = Path(os.environ.get("SLERMES_HOME") or os.environ.get("HOME") or ".")
NOW = {"value": 0.0}
HERMES_HOME = {"value": ""}


def resolve_path(rel):
    """Mirror the harness: absolute paths pass through, relative live under HOME."""
    if not rel:
        return None
    if rel.startswith(("/", "\\")) or (len(rel) > 1 and rel[1] == ":"):
        return Path(rel)
    return HOME / rel


def atomic_json_write(path, data, *, indent=2, mode=None, **kwargs):
    """The write seam: json.dump(indent=2, ensure_ascii=False) into a real file."""
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(data, indent=indent, ensure_ascii=False), encoding="utf-8")


def get_hermes_home():
    """hermes_constants.get_hermes_home() with the injected HERMES_HOME value;
    None when the gate injects nothing (home resolution is the seam)."""
    return HERMES_HOME["value"] or None


def load_ns():
    NS = {
        "atomic_json_write": atomic_json_write,
        "get_hermes_home": get_hermes_home,
        "time": type("T", (), {"time": staticmethod(lambda: NOW["value"])}),
        "Path": Path,
        "json": json,
        "Any": object,
        "Dict": dict,
        "List": list,
        "Optional": object,
    }
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == "_default_path":
            node = ast.parse(ast.unparse(node)).body[0]
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), NS)
        elif isinstance(node, ast.ClassDef) and node.name == "NodeRegistry":
            node = ast.parse(ast.unparse(node)).body[0]
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), NS)
    jtree = ast.parse(JSONFILE.read_text(encoding="utf-8"))
    for node in jtree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == "read_json":
            node = ast.parse(ast.unparse(node)).body[0]
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(JSONFILE), "exec",
                         flags=_ANN, dont_inherit=True), NS)
    return NS


NS = load_ns()


def file_view(path):
    """The registry document a next reader would see (None when it is gone)."""
    data = NS["read_json"](Path(path))
    return data


def drive(case):
    rel = case.get("path")
    path = resolve_path(rel) if rel is not None else None
    preload = case.get("preload")
    if path is not None:
        if isinstance(preload, str):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(preload if case.get("raw") else preload, encoding="utf-8")
        elif path.exists():
            path.unlink()
    NOW["value"] = case.get("now") or 0.0
    f = case.get("func")

    if f == "default_path":
        HERMES_HOME["value"] = case.get("hermes_home") or ""
        try:
            if not HERMES_HOME["value"]:
                return {"result": None, "file": None}   # no injected home -> none
            return {"result": str(NS["_default_path"]()).replace("\\", "/"), "file": None}
        finally:
            HERMES_HOME["value"] = ""

    reg = NS["NodeRegistry"](path)
    try:
        if f == "get":
            result = reg.get(case.get("name"))
        elif f == "add":
            try:
                reg.add(case.get("name"), case.get("url"), case.get("token"))
                result = {"status": "ok"}
            except ValueError as exc:
                result = {"status": "error", "type": type(exc).__name__, "message": str(exc)}
        elif f == "remove":
            result = reg.remove(case.get("name"))
        elif f == "list":
            result = reg.list_all()
        elif f == "resolve":
            result = reg.resolve(case.get("chrome_node"))
        else:
            raise AssertionError("unknown func %r" % (f,))
    except TypeError as exc:
        # `{"name": name, **entry}` on a non-mapping entry
        result = {"status": "error", "type": type(exc).__name__, "message": str(exc)}
    return {"result": result, "file": file_view(path)}


def canon(x):
    """Float/int identity for numbers, tuple -> list for JSON."""
    if isinstance(x, dict):
        return {k: canon(v) for k, v in x.items()}
    if isinstance(x, tuple):
        return [canon(v) for v in x]
    if isinstance(x, list):
        return [canon(v) for v in x]
    return x


def main():
    cases = mism = 0
    wanted = {"get", "add", "remove", "list", "resolve", "default_path"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = canon(drive(r))
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = canon(r.get("out"))
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
