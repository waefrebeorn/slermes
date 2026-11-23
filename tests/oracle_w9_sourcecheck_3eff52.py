#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/source_check.py
(w9_sourcecheck_3eff52 gate): the live pure decision helpers decide."""
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


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_mod("hermes_constants", get_hermes_home=lambda: None)
hcli = _mod("hermes_cli")
hcli.__path__ = []
_mod("hermes_cli.source_releases",
     OFFICIAL_REPOSITORY="NousResearch/hermes-agent",
     _GITHUB_ORIGIN=None,
     resolve_source_target=None)

SRC = Q + "/hermes_cli/source_check.py"
spec = importlib.util.spec_from_file_location("sourcecheck_w9_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sourcecheck_w9_live"] = live
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
        if func == "sha":
            py = live._is_full_sha(r.get("value"))
        elif func == "cfg":
            py = live._configured_branch(r.get("desktop_config"))
        elif func == "cob":
            py = live._checked_out_branch(r.get("current_branch"), r.get("fallback"))
        elif func == "remote":
            co = types.SimpleNamespace(embedded=bool(r.get("embedded", False)),
                                       repository=r.get("repository"),
                                       origin=r.get("origin", ""))
            py = live._branch_remote(co, r.get("branch", ""))
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

print(f"ORACLE w9_sourcecheck_3eff52: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
