#!/usr/bin/env python3
"""Oracle for the micro pure helpers: terminal_env_provider Base
(strip_env_keys, env_description), access_policy_mixin._allow_all_env_names
(ast-extracted, run on a mock self), and host_topology._normalized
(normalize_profile_name run live via import).
"""
import ast
import json
import sys
from types import SimpleNamespace

sys.path.insert(0, "C:/Users/eman5/hermes-agent")

files = [
    "C:/Users/eman5/hermes-agent/agent/terminal_env_provider.py",
    "C:/Users/eman5/hermes-agent/gateway/platforms/access_policy_mixin.py",
]

NS = {}
for path in files:
    code = open(path, encoding="utf-8").read()
    tree = ast.parse(code)
    for n in tree.body:
        if isinstance(n, ast.ClassDef):
            for m in n.body:
                if isinstance(m, ast.FunctionDef) and m.name in (
                        "strip_env_keys", "env_description",
                        "_allow_all_env_names"):
                    seg = ast.get_source_segment(code, m)
                    # drop any decorator lines ("@property")
                    lines = [l for l in seg.splitlines()
                             if not l.strip().startswith("@")]
                    seg = "\n".join(lines)
                    exec(compile(ast.Module(body=[ast.parse(seg).body[0]],
                                            type_ignores=[]), "x", "exec"), NS)

strip_ = NS["strip_env_keys"]
desc_ = NS["env_description"]
aaen_ = NS["_allow_all_env_names"]

# _normalized: run the real normalize_profile_name live.
from hermes_cli.profiles import normalize_profile_name  # noqa: E402


def _norm(name):
    if not name:
        return ""
    return normalize_profile_name(name)


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
        print("ORACLE micro_env: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "strip":
            want = list(strip_(None))
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH strip: C={c['out']} PY={want}")
        elif fam == "desc":
            me = SimpleNamespace(display_name=c["name"])
            want = desc_(me)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH desc {c['case']}: C={c['out']!r} PY={want!r}")
        elif fam == "aaen":
            me = SimpleNamespace(ALLOW_ALL_ENV_PREFIX=c["prefix"])
            want = list(aaen_(me))
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH aaen {c['case']}: C={c['out']} PY={want}")
        elif fam == "norm":
            want = _norm(c["name"]) or ""
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH norm {c['case']}: C={c['out']!r} PY={want!r}")
    print(f"ORACLE micro_env: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
