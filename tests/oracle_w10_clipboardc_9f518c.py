#!/usr/bin/env python3
"""Differential oracle for hermes_cli/clipboard_cmd.py (gate w10_clipboardc_9f518c)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/clipboard_cmd.py"
sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

spec = importlib.util.spec_from_file_location("w10_clipboardc_9f518c_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_clipboardc_9f518c_live"] = live
spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "mime":
        vals = [int(x) & 0xFF for x in (r.get("head") or [])[:16]]
        return live._mime(bytes(vals))
    if func == "exit_codes":
        return {"EXIT_OK": live.EXIT_OK, "EXIT_NO_IMAGE": live.EXIT_NO_IMAGE,
                "EXIT_NO_TOOL": live.EXIT_NO_TOOL, "EXIT_WRITE_FAILED": live.EXIT_WRITE_FAILED}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except json.JSONDecodeError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w10_clipboardc_9f518c: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH w10_clipboardc_9f518c {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w10_clipboardc_9f518c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
