#!/usr/bin/env python3
"""Differential oracle for hermes_cli/worktree_archive.py (w15_worktreear_5029d2).

os.lstat is injected via the live module's `os` attribute so _split_oversized is
tested with fixture sizes (decimal strings) and no filesystem. Byte counts are
stringified on both sides to preserve int64 values through the C JSON double.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/worktree_archive.py"
spec = importlib.util.spec_from_file_location("w15_worktreear_5029d2_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class Wt:
    """`wt / rel` returns rel verbatim, avoiding Windows Path backslashes."""
    def __truediv__(self, rel):
        return rel


SIZES = {}


def _lstat(p):
    if p not in SIZES:
        raise OSError("missing size")
    return types.SimpleNamespace(st_size=int(SIZES[p]))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func not in ("ref_safe", "parse_meta", "split_oversized"):
        continue
    cases += 1
    try:
        if func == "ref_safe":
            py = live.ref_safe(r.get("name", ""))
        elif func == "parse_meta":
            py = live._parse_meta(r.get("body", ""))
        else:
            SIZES = {k: v for k, v in (r.get("sizes", {}) or {}).items()}
            live.os = types.SimpleNamespace(lstat=_lstat)
            result = live._split_oversized(Wt(), r.get("untracked", []),
                                            float(r.get("max_mb", 50)))
            py = [{"path": e["path"], "bytes": str(e["bytes"])} for e in result]
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w15_worktreear_5029d2: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
