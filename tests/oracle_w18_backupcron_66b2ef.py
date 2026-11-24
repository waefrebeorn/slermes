#!/usr/bin/env python3
"""Differential oracle for hermes_cli/backup_cron_prompts.py (w18_backupcron_66b2ef)."""
import copy
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


if "hermes_cli" not in sys.modules:
    _mk("hermes_cli")

SRC = Q + "/hermes_cli/backup_cron_prompts.py"
spec = importlib.util.spec_from_file_location("w18_backupcron_66b2ef_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compat(v):
    if isinstance(v, (list, tuple)):
        return [compat(x) for x in v]
    if isinstance(v, dict):
        return {str(k): compat(x) for k, x in v.items()}
    return v


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
        if func == "prompt_degraded":
            py = live._prompt_degraded(r.get("job"))
        elif func == "cron_jobs_list":
            py = live._cron_jobs_list(r.get("doc"))
        elif func == "snapshot_prompts_by_id":
            py = live._snapshot_prompts_by_id(r.get("doc"))
        elif func == "restore_degraded_prompts":
            jobs = copy.deepcopy(r.get("jobs"))
            ids = live._restore_degraded_prompts(jobs, r.get("snap_prompts"),
                                                 apply=bool(r.get("apply", False)))
            py = {"ids": ids, "jobs": jobs}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    py = compat(py)
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w18_backupcron_66b2ef: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
