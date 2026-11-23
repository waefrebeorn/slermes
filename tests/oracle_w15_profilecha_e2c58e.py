#!/usr/bin/env python3
"""Differential oracle for hermes_cli/profile_channels.py (gate w15_profilecha_e2c58e)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_profilecha_e2c58e"
sys.path.insert(0, QUARRY)

# parent-package bootstrap for the live hermes_cli module
_hc = types.ModuleType("hermes_cli")
_hc.__path__ = []
sys.modules["hermes_cli"] = _hc

SRC = QUARRY + "/hermes_cli/profile_channels.py"
_spec = importlib.util.spec_from_file_location("profile_channels_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["profile_channels_live"] = live
_spec.loader.exec_module(live)

FUNCS = ("prefixes", "env_key", "explicit_enabled", "config_paths", "warning", "notice")


def live_out(rec):
    f = rec.get("func")
    if f == "prefixes":
        return list(live.platform_env_prefixes(rec.get("pid", "")))
    if f == "env_key":
        return live._env_key_of_line(rec.get("line", ""))
    if f == "explicit_enabled":
        return live._explicit_enabled(rec.get("raw") or {}, rec.get("pid", ""))
    if f == "config_paths":
        return [list(p) for p in live._channel_config_paths(rec.get("raw") or {}, rec.get("platforms") or [])]
    if f == "warning":
        return live.shared_credential_warning(rec.get("profile", ""), rec.get("platforms") or [],
                                              rec.get("source", "default"))
    if f == "notice":
        return live.format_stripped_notice(rec.get("profile", ""), rec.get("platforms") or [],
                                           rec.get("clone_flag", "--clone"))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in FUNCS:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            print(f"LIVE ERROR {rec.get('func')}: {exc!r}", file=sys.stderr)
            mism += 1
            continue
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
