#!/usr/bin/env python3
"""Differential oracle for gateway/warning_notifications.py (gate w17_warningnot_af9536).

gateway.display_config.resolve_display_setting is stubbed with the same
suppress_warning_notifications subset that the C port implements; the
DiagnosticText marker and notice/event shapes are reconstructed per case.
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


def _norm(value):
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.strip().lower() in {"true", "1", "yes", "on"}
    return False


def _resolve(user_config, platform_key, setting, fallback=None):
    """Faithful subset of resolve_display_setting for suppress_warning_notifications."""
    display = user_config.get("display") if isinstance(user_config, dict) else None
    values = []
    if isinstance(display, dict):
        platforms = display.get("platforms")
        po = platforms.get(platform_key) if isinstance(platforms, dict) else None
        if isinstance(po, dict) and po.get(setting) is not None:
            values.append(po[setting])
        if setting != "streaming" and display.get(setting) is not None:
            values.append(display[setting])
    for v in values:
        nv = _norm(v)
        if nv is not None:
            return nv
    return fallback


gateway = _mod("gateway")
gateway.__path__ = []
dc = _mod("gateway.display_config", resolve_display_setting=_resolve)
gateway.display_config = dc
hcli = _mod("hermes_cli")
hcli.__path__ = []
_mod("hermes_cli.config_effective", load_user_config_effective=lambda: {})

SRC = Q + "/gateway/warning_notifications.py"
spec = importlib.util.spec_from_file_location("w17_warningnot_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_warningnot_live"] = live
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
        if func == "is_warning_status":
            msg = live.DiagnosticText("x") if r.get("diag", False) else "plain"
            py = live.is_warning_status(r.get("event_type", ""), msg)
        elif func == "is_diagnostic_notice":
            notice = types.SimpleNamespace(level=r.get("level"), key=r.get("key", ""))
            py = live.is_diagnostic_notice(notice)
        elif func == "diagnostic_metadata":
            event = types.SimpleNamespace(internal=r.get("internal", False),
                                          metadata=r.get("metadata"))
            py = live.diagnostic_metadata(event)
        elif func == "warning_notifications_enabled":
            py = live.warning_notifications_enabled(r.get("platform", ""), r.get("user_config"))
        elif func == "diagnostic_turn_muted":
            py = live.diagnostic_turn_muted(r.get("display_metadata"), r.get("platform", ""),
                                            r.get("user_config"))
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

print(f"ORACLE w17_warningnot_af9536: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
