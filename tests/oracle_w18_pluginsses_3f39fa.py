#!/usr/bin/env python3
"""Differential oracle for w18_pluginsses_3f39fa / hermes_cli/plugins_session_env.py."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/plugins_session_env.py"
sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

spec = importlib.util.spec_from_file_location("sessenv_live_w18", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sessenv_live_w18"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "env_key":
        return live._env_key(r.get("name", ""))
    if f == "name_valid":
        # Drive the live registration gate; a fresh registry per case avoids
        # cross-case clashes. Release the token when the name is accepted.
        live._REGISTRATIONS.clear()
        live._DECLARED.clear()
        try:
            release = live.register_session_env(
                "scope", [r.get("name")], lambda sid: {}, owner="o")
            release()
            return True
        except (ValueError, TypeError):
            return False
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None: continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w18_pluginsses_3f39fa: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w18_pluginsses_3f39fa {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_pluginsses_3f39fa: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
