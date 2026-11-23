#!/usr/bin/env python3
"""Oracle for upstream Hermes cron/env_settings.py."""
import importlib.util, json, os, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/cron/env_settings.py"
spec = importlib.util.spec_from_file_location("cronenv_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

class _Scope:
    def __init__(self, values):
        self.values = values or {}
    def get(self, key):
        return self.values.get(key)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "cron_env_setting":
        continue
    name = r["name"]
    env = r.get("env_val")
    if env is None:
        os.environ.pop(name, None)
    else:
        os.environ[name] = env
    has_scope = bool(r.get("has_scope", False))
    live.is_multiplex_active = lambda: bool(r.get("multiplex", False))
    live.current_secret_scope = lambda: (_Scope(r.get("scope")) if has_scope else None)
    live.build_profile_secret_scope = lambda home: _Scope(r.get("home_scope"))
    live.get_hermes_home = lambda: r.get("home", "")
    py = live.cron_env_setting(name, r.get("default", ""))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH cron_env_setting args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE cronenv: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
