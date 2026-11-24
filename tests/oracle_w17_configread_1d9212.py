#!/usr/bin/env python3
"""Differential oracle for gate w17_configread_1d9212 (hermes_cli/config_read_errors.py)."""
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

_hcli = types.ModuleType("hermes_cli")
_hcli.__path__ = []
sys.modules["hermes_cli"] = _hcli

NAME = "w17cre_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/hermes_cli/config_read_errors.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w17_configread_1d9212"


class _Path:
    """A path whose str() is the raw fixture string (no Windows backslashes)."""
    def __init__(self, s):
        self._s = s

    def __str__(self):
        return self._s


def make_exc(d):
    d = d or {}
    if d.get("permission"):
        cls = PermissionError
    elif d.get("oserror"):
        cls = OSError
    else:
        cls = Exception
    e = cls(d.get("str") or "")
    for attr in ("problem_mark", "context_mark"):
        v = d.get(attr)
        setattr(e, attr, types.SimpleNamespace(**v) if isinstance(v, dict) else None)
    e.problem = d.get("problem")
    return e


def compute(r):
    inp = r.get("in") or {}
    func = r.get("func")
    exc = make_exc(inp.get("exc"))
    if func == "yaml_error_location":
        return live._yaml_error_location(exc)
    if func == "yaml_error_details":
        return live._yaml_error_details(exc)
    if func == "format_config_parse_failure":
        return live.format_config_parse_failure(
            _Path(inp.get("path", "")), exc, fallback=inp.get("fallback", "defaults"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
