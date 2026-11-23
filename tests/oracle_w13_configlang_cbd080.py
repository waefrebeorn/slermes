#!/usr/bin/env python3
"""Differential oracle for w13_configlang_cbd080 (hermes_cli/config_language.py)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/config_language.py"
sys.path.insert(0, QUARRY)
import hermes_cli  # noqa: F401  parent-package bootstrap
spec = importlib.util.spec_from_file_location("hermes_cli.config_language", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.config_language"] = live
spec.loader.exec_module(live)
import agent.i18n as i18n


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False))


def compute(r):
    resolved = r.get("resolved")
    supported = tuple(r.get("supported") or ())
    live.resolve_display_language = lambda x, rv=resolved: rv
    i18n.supported_languages = lambda sv=supported: sv
    return live.display_language_error(r.get("value"))


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
        py = canon(compute(r))
    except Exception as exc:
        print(f"ORACLE w13_configlang_cbd080: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH w13_configlang_cbd080 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_configlang_cbd080: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
