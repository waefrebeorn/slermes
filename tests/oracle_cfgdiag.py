#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/config_check_diagnostics.py."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/config_check_diagnostics.py"
sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg


def load(name, rel):
    spec = importlib.util.spec_from_file_location(name, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


# Live collaborator implementations, loaded from the quarry (not re-implemented).
real_cvc = load("cfgdiag_cvc_live", "hermes_cli/config_value_checks.py")

spec = importlib.util.spec_from_file_location("cfgdiag_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["cfgdiag_live"] = live
spec.loader.exec_module(live)


def fake(**attrs):
    m = types.ModuleType("cfgdiag_fake")
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


def install(config_mod, plugins_mod, toolset_mod, checks_mod):
    sys.modules["hermes_cli.config"] = config_mod
    sys.modules["hermes_cli.plugins_discovery"] = plugins_mod
    sys.modules["hermes_cli.toolset_validation"] = toolset_mod
    sys.modules["hermes_cli.config_value_checks"] = checks_mod


def compute(r):
    func = r.get("func")
    if func == "voice":
        install(fake(_platform_manifest_env_entries=lambda *a, **k: [],
                     _platform_plugin_manifests=lambda *a, **k: []),
                fake(_get_disabled_plugins=lambda: set()),
                fake(saved_toolset_resolver=lambda c: (lambda n: True),
                     validate_platform_toolsets=lambda *a, **k: []),
                real_cvc)
        return live.config_check_diagnostics(r.get("config") or {}, lambda k: None)
    if func == "disabled":
        manifests = r.get("manifests") or []
        disabled = set(r.get("disabled") or [])
        env = r.get("env") or {}

        def platform_manifests(source="all", **kw):
            return [(m["name"],
                     {"name": m.get("manifest_name"),
                      "__req__": m.get("required") or []}) for m in manifests]

        install(fake(
                    _platform_manifest_env_entries=lambda m, optional=True:
                        [(e, None, None) for e in m["__req__"]],
                    _platform_plugin_manifests=platform_manifests),
                fake(_get_disabled_plugins=lambda: disabled),
                fake(saved_toolset_resolver=lambda c: (lambda n: True),
                     validate_platform_toolsets=lambda *a, **k: []),
                fake(_validate_voice=lambda config, issues: None))
        return live.config_check_diagnostics({}, lambda k: env.get(k))
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
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE cfgdiag: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH cfgdiag {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE cfgdiag: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
