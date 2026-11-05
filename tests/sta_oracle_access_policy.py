#!/usr/bin/env python3
"""Oracle for gateway/platforms/access_policy_mixin.py pure helpers:
_open_dm_opted_in and _entry_matches. Both ast-extracted and run live on a
mock self; the secret env read and OPTIN_TRUTHY are provided as namespaced
globals.
"""
import ast
import json
import sys
from types import SimpleNamespace

SRC = "C:/Users/eman5/hermes-agent/gateway/platforms/access_policy_mixin.py"
code = open(SRC, encoding="utf-8").read()
tree = ast.parse(code)

wants = {}
for n in tree.body:
    if isinstance(n, ast.Assign):
        for t in n.targets:
            if isinstance(t, ast.Name) and t.id == "OPTIN_TRUTHY":
                wants["OPTIN_TRUTHY"] = ast.get_source_segment(code, n)
    elif isinstance(n, ast.ClassDef):
        for m in n.body:
            if isinstance(m, ast.FunctionDef) and m.name in (
                    "_open_dm_opted_in", "_entry_matches"):
                wants[m.name] = ast.get_source_segment(code, m)

NS = {}
for v in wants.values():
    exec(compile(ast.Module(body=[ast.parse(v).body[0]], type_ignores=[]),
                 "x", "exec"), NS)
optin_ = NS["_open_dm_opted_in"]
match_ = NS["_entry_matches"]

NAMES = ("GATEWAY_ALLOW_ALL_USERS", "WHATSAPP_ALLOW_ALL_USERS")


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE access_policy: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "optin":
            envmap = {NAMES[0]: c["e0"], NAMES[1]: c["e1"]}
            NS["OPTIN_TRUTHY"] = frozenset({"true", "1", "yes"})
            NS["get_scoped_secret"] = lambda name, d: envmap.get(name, d)
            me = SimpleNamespace(ALLOW_ALL_ENV_PREFIX="WHATSAPP")
            me._allow_all_env_names = lambda: NAMES
            want = 1 if optin_(me) else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH optin {c['case']}: C={c['out']} PY={want} "
                      f"e0={c['e0']!r} e1={c['e1']!r}")
        elif fam == "match":
            entries = list(c["entries"])
            want = 1 if match_(SimpleNamespace(), entries, c["target"]) else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH match {c['case']}: C={c['out']} PY={want} "
                      f"target={c['target']!r}")
    print(f"ORACLE access_policy: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
