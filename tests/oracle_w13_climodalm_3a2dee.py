#!/usr/bin/env python3
"""Differential oracle for w13_climodalm_3a2dee (hermes_cli/cli_modal_mixin.py)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/cli_modal_mixin.py"
sys.path.insert(0, QUARRY)
import hermes_cli  # noqa: F401  parent-package bootstrap
spec = importlib.util.spec_from_file_location("hermes_cli.cli_modal_mixin", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.cli_modal_mixin"] = live
spec.loader.exec_module(live)
CM = live.CLIModalMixin
# The token set lives on HermesCLI (cli.py); mirror it for the isolated class.
CM._DESTRUCTIVE_SKIP_TOKENS = frozenset({"now", "--yes", "-y"})


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
    f = r.get("func")
    if f == "approval_choices":
        return CM._approval_choices(None, r.get("command", ""),
                                    allow_permanent=r.get("allow_permanent", True),
                                    allow_session=r.get("allow_session", True),
                                    smart_denied=r.get("smart_denied", False))
    if f == "normalize":
        choices = [tuple(c) for c in (r.get("choices") or [])]
        return CM._normalize_slash_confirm_choice(None, r.get("raw"), choices)
    if f == "opening_phase":
        return CM._connection_opening_phase(r.get("target") or {})
    if f == "split_skip":
        return CM._split_destructive_skip(r.get("cmd_text"))
    if f == "connection_fields":
        return CM._connection_fields(r.get("target") or {})
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
        py = canon(compute(r))
    except Exception as exc:
        print(f"ORACLE w13_climodalm_3a2dee: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH w13_climodalm_3a2dee {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_climodalm_3a2dee: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
