#!/usr/bin/env python3
"""Oracle for upstream Hermes cron/jobs_model_pin.py (jobspin gate).

_main_model_pin reads config.yaml and resolves the provider via I/O. The
oracle injects controlled stand-ins for those inline imports and then calls
the LIVE function unchanged, comparing its (provider, model) tuple to the C
decision core's JSON output.
"""
import importlib.util, json, sys, types, pathlib

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)


def _load(name, rel):
    spec = importlib.util.spec_from_file_location(name, PYROOT + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


live = _load("jobspin_live", "cron/jobs_model_pin.py")
normalizer = _load("jobspin_norm", "cron/jobs_normalize.py")

holder = {"cfg": {}, "provider": None}

fake_jobs = types.ModuleType("cron.jobs")
fake_jobs._normalize_job_optional_text = normalizer._normalize_job_optional_text
cb = types.ModuleType("hermes_cli.config_backend")
cb.ConfigBackendUnavailable = type("ConfigBackendUnavailable", (Exception,), {})
cb.config_exists = lambda p: True
ce = types.ModuleType("hermes_cli.config_effective")
ce.load_user_config_effective = lambda p: holder["cfg"]
rp = types.ModuleType("hermes_cli.runtime_provider")
rp.resolve_runtime_provider = lambda requested: {"provider": holder["provider"]}
hc = types.ModuleType("hermes_constants")
hc.get_hermes_home = lambda: pathlib.Path(".")

import cron  # noqa: F401  (so "cron.jobs" is a valid sys.modules key)
sys.modules["cron.jobs"] = fake_jobs
sys.modules["hermes_cli.config_backend"] = cb
sys.modules["hermes_cli.config_effective"] = ce
sys.modules["hermes_cli.runtime_provider"] = rp
sys.modules["hermes_constants"] = hc

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") != "pin":
        continue
    holder["cfg"] = r.get("cfg")
    holder["provider"] = r.get("provider")
    py = live._main_model_pin()
    py = [py[0], py[1]]  # tuple -> JSON array
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH pin args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE jobspin: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
