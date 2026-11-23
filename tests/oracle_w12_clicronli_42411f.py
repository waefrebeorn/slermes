#!/usr/bin/env python3
"""Differential oracle for w12_clicronli_42411f (hermes_cli/cli_cron_lines.py).

_t is stubbed to the identity key and the cron store result is injected from
the fixture; the live flag-parser and line builders run unmodified.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Parent-package bootstrap before executing the live module by file path.
hermes_cli = _mod("hermes_cli")
hermes_cli.__path__ = []
_mod("hermes_cli.cli_commands_session_tools",
     _t=lambda key, **kw: key,
     _lines=lambda text, pad="": [pad + text])

SRC = Q + "/hermes_cli/cli_cron_lines.py"
spec = importlib.util.spec_from_file_location("w12_clicronli_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w12_clicronli_live"] = live
spec.loader.exec_module(live)

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
    try:
        if func == "normalize_skills":
            py = live._normalize_skills(r.get("values", []))
        elif func == "parse_cron_flags":
            py = live._parse_cron_flags(r.get("tokens", []))
        elif func in ("add_lines", "job_action_lines"):
            result = r.get("result", {})
            orig = live._cron
            try:
                live._cron = lambda **kw: result
                if func == "add_lines":
                    py = live._add_lines(r.get("subcommand", "add"), r.get("opts", {}))
                else:
                    py = live._job_action_lines(r.get("subcommand", "pause"), r.get("opts", {}))
            finally:
                live._cron = orig
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_clicronli_42411f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
