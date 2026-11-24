#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_runtime_keys.py (gate w18_configrunt_356cce)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _seed(name, path=None, **attrs):
    m = types.ModuleType(name)
    if path is not None:
        m.__path__ = [path]
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], m)
    return m


_seed("hermes_cli", Q + "/hermes_cli")
_seed("tools", Q + "/tools")

spec = importlib.util.spec_from_file_location(
    "w18_configrunt_live", Q + "/hermes_cli/config_runtime_keys.py")
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
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
        if func == "covered":
            py = live._covered(r.get("key") or "", r.get("paths") or [])
        elif func == "is_runtime_read_key":
            parts = r.get("parts") or []
            preset = bool(r.get("preset", False))
            selection = list(r.get("selection") or [])
            plugin = list(r.get("plugin") or [])
            _seed("hermes_cli.moa_config", is_preset_field_path=lambda ps: preset)
            _seed("tools.tool_backend_helpers", selection_config_keys=lambda: set(selection))
            live.plugin_declared_config_keys = lambda: frozenset(plugin)
            py = live.is_runtime_read_key(parts)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if bool(py) != bool(got):
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w18_configrunt_356cce: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
