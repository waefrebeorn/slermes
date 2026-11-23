#!/usr/bin/env python3
"""Oracle for the pm/build_env.py C port (gate w5_buildenv_1eb2c8).

Requirements filtering is evaluated with the exact expression from main().
--record-selection validation is driven through the LIVE
_check_record_selection() with a real argparse parser + Namespace; a
parser.error() (SystemExit) is the 'invalid' verdict.
"""
import argparse
import ast
import json
import sys
import types
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "pm" / "build_env.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "pm" / "build_env.py")
sys.path.insert(0, str(QUARRY))

pm = sys.modules.get("pm")
if pm is None:
    pm = types.ModuleType("pm")
    pm.__path__ = [str(QUARRY / "pm")]
    sys.modules["pm"] = pm

tree = ast.parse(Path(SRC).read_text(encoding="utf-8"))
ns = {"__name__": "w5_build_env_live", "__builtins__": __builtins__}
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name == "_check_record_selection":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
CHECK = ns["_check_record_selection"]


def parse_requirements(text):
    return [line.strip() for line in text.splitlines()
            if line.strip() and not line.lstrip().startswith("#")]


def record_selection_invalid(rec):
    parser = argparse.ArgumentParser()
    args = argparse.Namespace(
        record_selection=bool(rec.get("record_selection", False)),
        manager_runtime=bool(rec.get("manager_runtime", False)),
        all_extras=bool(rec.get("all_extras", False)),
        groups=["g"] * int(rec.get("groups_count", 0)),
        extras=["e"] * int(rec.get("extras_count", 0)),
    )
    requirements = ["r"] * int(rec.get("requirements_count", 0))
    try:
        CHECK(parser, args, build=bool(rec.get("build", False)), requirements=requirements)
        return 0
    except SystemExit:
        return 1


def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        func = rec.get("func")
        got = rec.get("out")
        try:
            if func == "w5_buildenv_1eb2c8_parse_requirements":
                exp = parse_requirements(rec.get("text", ""))
            elif func == "w5_buildenv_1eb2c8_record_selection_invalid":
                exp = record_selection_invalid(rec)
            else:
                exp = {"__unknown__": func}
        except BaseException as e:
            exp = {"__raise__": type(e).__name__}
        n += 1
        if got != exp:
            mism += 1
            if shown < 10:
                shown += 1
                print("MISMATCH %s case %d:\n  C=%s\n  P=%s" % (
                    func, n, json.dumps(got), json.dumps(exp)), file=sys.stderr)
    print("ORACLE w5_buildenv_1eb2c8: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
