#!/usr/bin/env python3
"""Differential oracle for w13_cliloopsm_ec3523 (hermes_cli/cli_loops_mixin.py)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/cli_loops_mixin.py"
sys.path.insert(0, QUARRY)
# Parent-package bootstrap: load the real hermes_cli package so its lazy imports resolve.
import hermes_cli  # noqa: F401
spec = importlib.util.spec_from_file_location("hermes_cli.cli_loops_mixin", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.cli_loops_mixin"] = live
spec.loader.exec_module(live)


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


class _Self:
    pass


def compute(r):
    f = r.get("func")
    if f == "preview":
        return live._preview(r.get("payload"))
    if f == "parse_queue_index":
        return live._parse_queue_index(r.get("token"))
    if f == "last_assistant_response_text":
        s = _Self()
        s.conversation_history = r.get("history") or []
        return live.CLILoopsMixin._last_assistant_response_text(s)
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
        print(f"ORACLE w13_cliloopsm_ec3523: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH w13_cliloopsm_ec3523 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_cliloopsm_ec3523: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
