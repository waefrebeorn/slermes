#!/usr/bin/env python3
"""Differential oracle for plugins/model-providers/gemini/client.py (gate w19_client_ead47a).

Extracts the live routing helpers from the Python AST into stub-filled globals: the two
adapter predicates from agent/gemini_native_adapter.py, the module's own constants, and
the GeminiClient._continues staticmethod. The config load is the seam: the fixture states
whether load_config_readonly() answered a mapping or raised.
"""
import ast
import json
import re
import sys
import types
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
CLIENT = QUARRY / "plugins" / "model-providers" / "gemini" / "client.py"
ADAPTER = QUARRY / "agent" / "gemini_native_adapter.py"
GATE = "w19_client_ead47a"
_ANN = __future__.annotations.compiler_flag


def stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


class _Logger:
    def debug(self, *a, **k):
        pass


class _Wire:
    """The `interactions` wire module, unused by the extracted helpers."""


G = {"__name__": "gemini_live", "re": re, "logger": _Logger(), "httpx": object,
     "wire": _Wire(), "PROVIDER_ID": "gemini"}


def _exec_functions(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []          # a @staticmethod must come out callable
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            names = set()
            for target in node.targets:
                if isinstance(target, ast.Name):
                    names.add(target.id)
                elif isinstance(target, ast.Tuple):
                    names.update(e.id for e in target.elts if isinstance(e, ast.Name))
            if names & {"_INTERACTIONS_HOST", "API_INTERACTIONS", "API_GENERATE_CONTENT",
                        "API_AUTO", "PROVIDER_ID"}:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                             flags=_ANN, dont_inherit=True), into)


_exec_functions(CLIENT, {"_configured_api", "uses_interactions", "_continues"}, G)
_exec_functions(ADAPTER, {"bare_gemini_model_id", "gemini_uses_thinking_level", "_gemini_version"}, G)


def run_case(r):
    f = r.get("func")
    if f == "bare_model_id":
        return G["bare_gemini_model_id"](r.get("model"))
    if f == "uses_thinking_level":
        return G["gemini_uses_thinking_level"](r.get("model"))
    if f == "configured_api":
        cfg = r.get("config")
        ok = not r.get("config_unreadable")
        stub_module("hermes_cli.config",
                    load_config_readonly=(lambda: cfg) if ok else
                    (lambda: (_ for _ in ()).throw(RuntimeError("unreadable"))))
        return G["_configured_api"](r.get("model", ""), r.get("bare", ""))
    if f == "uses_interactions":
        cfg = r.get("config")
        ok = not r.get("config_unreadable")
        stub_module("hermes_cli.config",
                    load_config_readonly=(lambda: cfg) if ok else
                    (lambda: (_ for _ in ()).throw(RuntimeError("unreadable"))))
        return G["uses_interactions"](r.get("model"), r.get("base_url"))
    if f == "continues":
        return G["_continues"](r.get("interaction"))
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
        if f not in {"bare_model_id", "uses_thinking_level", "configured_api",
                     "uses_interactions", "continues"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s args=%r py=raised %r c=%r" % (f, r, exc, r.get("out")),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s args=%r py=%r c=%r" % (f, r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
