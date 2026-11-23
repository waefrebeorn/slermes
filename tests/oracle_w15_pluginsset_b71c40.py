#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_settings.py (gate w15_pluginsset_b71c40)."""
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
G = "w15_pluginsset_b71c40"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# parent-package bootstrap + stub for the plugins_state import
_hc = _mod("hermes_cli")
_hc.__path__ = []
_mod("hermes_cli.plugins_state",
     _plugin_relative_segments=lambda key: [key],
     _plugin_settings_entry=lambda cfg, pid: {},
     save_plugin_setting=lambda pid, segs, value: None)

SRC = QUARRY + "/hermes_cli/plugins_settings.py"
_spec = importlib.util.spec_from_file_location("plugins_settings_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["plugins_settings_live"] = live
_spec.loader.exec_module(live)

# injected seams: schema from the fixture, writer is a no-op
live._manifest_config_schema = lambda plugin_dir: {}
live.save_plugin_setting = lambda pid, segs, value: None
live._plugin_relative_segments = lambda key: [key]


def live_out(rec):
    func = rec.get("func")
    if func == "secret_env_name":
        return live.secret_env_name(rec.get("plugin_id", ""), rec.get("key", ""), rec.get("spec") or {})
    if func == "field_type":
        return live._field_type(rec.get("spec") or {})
    if func == "save_plugin_settings":
        live._manifest_config_schema = lambda plugin_dir: rec.get("schema") or {}
        return live.save_plugin_settings(rec.get("plugin_id", ""), None, rec.get("values") or {})
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
        if rec.get("func") not in ("secret_env_name", "field_type", "save_plugin_settings"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except ValueError as exc:
            py = f"ValueError: {exc}"
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
