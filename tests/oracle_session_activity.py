#!/usr/bin/env python3
"""Oracle for agent/session_activity.py pure provenance/description helpers.

ast-extract the LIVE ActivityProvenance enum + TERMINAL_COMPRESSION_PROVENANCES
+ normalize / is_terminal / bound, seed only Enum, and replay the harness
families on the same inputs:
  is_terminal {in() via seq, ret}  normalize {out}  bound    {in,out}
The harness's is_terminal/normalize families reference provs by array index in
'case' but do NOT emit the provenance inline, so the oracle re-derives input
from the harness's own static provs table mirroring t_port_session_activity.c.
Bound family emits {'in': <desc|null>, 'out': <desc|...>} inline and is driven
directly from the line.
"""
import ast
import json
import sys
from enum import Enum

# the C harness emits a UTF-8 ellipsis in the bound family; without forcing the
# stream codecs the bytes arrive as U+FFFD and every comparison fails
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/session_activity.py"

# mirror of the harness's provs[] (same order, same values incl. NULL slots)
PROVS = [
    "agent.compression_timeout", "agent.compression_cooldown",
    "agent.compression_turnhold", "agent.compression",
    "unknown", "", "  agent.compression_cooldown  ", "BOGUS", None,
    "agent.compression.turnhold", "AGENT.COMPRESSION_COOLDOWN",
]
# max guard so we never index past the table
MAX_PROV = len(PROVS)


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"Enum": Enum}
    for n in ast.parse(src).body:
        if isinstance(n, ast.ClassDef) and n.name == "ActivityProvenance":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id in ("TERMINAL_COMPRESSION_PROVENANCES",
                                                 "ACTIVITY_DESCRIPTION_MAX")
            for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in (
                "normalize_activity_provenance", "is_terminal_compression_provenance",
                "bound_activity_description"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    normalize = ns["normalize_activity_provenance"]
    is_terminal = ns["is_terminal_compression_provenance"]
    bound = ns["bound_activity_description"]
    UNKNOWN = ns["ActivityProvenance"].UNKNOWN

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
        print("ORACLE session_activity: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        fam = c.get("family")
        case = c.get("case", "")
        # case carries an index like is_term_7 or norm_9
        idx_s = (case or "").split("_")[-1]
        idx = int(idx_s) if idx_s.isdigit() else None
        if fam == "is_terminal":
            total += 1
            q = PROVS[idx] if (idx is not None and idx < MAX_PROV) else None
            want = bool(is_terminal(q))
            if c.get("ret") != want:
                mism += 1
                print(f"MISMATCH is_terminal {case} prov={q!r} C={c.get('ret')!r} PY={want!r}")
        elif fam == "normalize":
            total += 1
            q = PROVS[idx] if (idx is not None and idx < MAX_PROV) else None
            # C emits the enum VALUE string
            want = normalize(q).value
            # normalize(UNKNOWN) -> the value "unknown" not the Enum repr
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH normalize {case} prov={q!r} C={c.get('out')!r} PY={want!r}")
        elif fam == "bound":
            total += 1
            want = bound(c.get("desc"))
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH bound {case} in={c.get('desc')!r} C={c.get('out')!r} PY={want!r}")
    print(f"ORACLE session_activity: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
