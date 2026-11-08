#!/usr/bin/env python3
"""Oracle for tools/skills_hub_github.py github_provider_for (port embedded in
src/skills_hub.c). The live module imports httpx at module load (missing in the
gate interpreter), so ast-extract the pure fn + GITHUB_TAP_PROVIDERS dict and
replay live on each harness repo (strip/lower applied live); None -> ""."""
import ast
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def main() -> int:
    src = open("C:/Users/eman5/hermes-agent/tools/skills_hub_github.py",
               encoding="utf-8").read()
    g = {"Optional": None, "httpx": __import__("types").ModuleType("httpx")}
    m = None
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "github_provider_for":
            exec(compile(ast.Module(body=[n], type_ignores=[]), "g", "exec"), g)
            m = g["github_provider_for"]
        elif isinstance(n, ast.Assign):
            for tar in n.targets:
                if isinstance(tar, ast.Name) and tar.id == "GITHUB_TAP_PROVIDERS":
                    exec(compile(ast.Module(body=[n], type_ignores=[]), "g", "exec"), g)
    if m is None:
        print("ORACLE github_provider: live fn not extracted")
        return 1
    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        val = c.get("in") or ""
        want = m(val) or ""
        want = "" if want is None else want
        c_out = c.get("out") or ""
        if want != c_out:
            mism += 1
            print(f"MISMATCH {val!r} C={c_out!r} PY={want!r}")
    if total == 0:
        print("ORACLE github_provider: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE github_provider: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
