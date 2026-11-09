#!/usr/bin/env python3
"""Oracle for port_nous_subscription.c -- drives the LIVE nous_subscription.py defs.

The harness echoes its inputs from C. The live functions are ast-extracted
because importing hermes_cli.nous_subscription pulls in the managed-tool
gateway. `is_truthy_value` is ast-extracted from utils for the same reason.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\nous_subscription.py"
UTILS = r"C:\Users\eman5\hermes-agent\utils.py"
WANTED = ["_norm", "_section", "_ensure_section", "_provider_is_nous",
         "_audio_provider", "_uses_gateway"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # is_truthy_value lives in utils
    try:
        usrc = open(UTILS, encoding="utf-8").read()
        for node in ast.parse(usrc).body:
            if isinstance(node, ast.FunctionDef) and node.name == "is_truthy_value":
                exec(compile(ast.Module(body=[node], type_ignores=[]), UTILS, "exec"), ns)
    except Exception:
        pass
    # is_truthy_value reads this module constant in utils
    ns.setdefault("TRUTHY_STRINGS", {"true", "yes", "on", "1"})
    if "is_truthy_value" not in ns:
        def is_truthy_value(value, default=False):
            if value is None: return default
            if isinstance(value, bool): return value
            if isinstance(value, (int, float)): return value != 0
            if isinstance(value, str): return value.strip().lower() in ("true", "yes", "on", "1")
            return default
        ns["is_truthy_value"] = is_truthy_value
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        cases += 1

        if fn == "_norm":
            raw = json.loads(c["in"]) if c["in"] is not None else None
            want = ns["_norm"](raw, c["fallback"] or "")
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _norm in={c['in']!r} C={got!r} PY={want!r}")

        elif fn == "_provider_is_nous":
            label = c.get("case")
            cfgs = {
                "nous": {"model": {"provider": "nous"}},
                "cased": {"model": {"provider": "  NOUS  "}},
                "other": {"model": {"provider": "openrouter"}},
                "no-model": {},
                "model-not-object": {"model": "nous"},
            }
            want = bool(ns["_provider_is_nous"](cfgs.get(label, {})))
            got = bool(c.get("out"))
            if got != want:
                mism += 1
                print(f"MISMATCH _provider_is_nous C={got!r} PY={want!r}")

        elif fn == "_audio_provider":
            label = c.get("case")
            cfgs = {
                "nous-managed": {"provider": "nous"},
                "gw-override": {"provider": "elevenlabs"},
                "byok": {"provider": "  ELEVENLABS "},
                "default": {},
            }
            want = ns["_audio_provider"](cfgs.get(label, {}), c["fallback"] or "", bool(c["gw"]))
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _audio_provider case={label} C={got!r} PY={want!r}")

        elif fn == "_uses_gateway":
            label = c.get("case")
            sections = {
                "true": {"use_gateway": True}, "yes": {"use_gateway": "yes"},
                "on": {"use_gateway": "on"}, "one": {"use_gateway": 1},
                "false": {"use_gateway": False}, "zero": {"use_gateway": 0},
                "missing": {}, "not-object": "x",
            }
            want = bool(ns["_uses_gateway"](sections.get(label)))
            got = bool(c.get("out"))
            if got != want:
                mism += 1
                print(f"MISMATCH _uses_gateway case={label} C={got!r} PY={want!r}")

    print(f"ORACLE nous_subscription: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
