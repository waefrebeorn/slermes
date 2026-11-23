#!/usr/bin/env python3
"""Differential oracle for tools/delegate_tool_config.py (gate w14_delegateto_5b49f0)."""
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
G = "w14_delegateto_5b49f0"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# parent-package bootstrap + stubs for the heavy imports
_mod("utils", base_url_hostname=lambda u: "", is_truthy_value=lambda v, default=False: bool(v))
_hc = _mod("hermes_cli")
_hc.__path__ = []
_mod("hermes_cli.fallback_config", scoped_fallback_chain=lambda *a, **k: None)
_gw = _mod("gateway")
_gw.__path__ = []
_mod("gateway.session_context", NON_MESSAGING_SESSION_SURFACES=frozenset({
    "", "api_server", "cli", "codex", "desktop", "gateway", "kanban", "local",
    "msgraph_webhook", "tool", "tui", "webhook",
}))

SRC = QUARRY + "/tools/delegate_tool_config.py"
_spec = importlib.util.spec_from_file_location("delegate_tool_config_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["delegate_tool_config_live"] = live
_spec.loader.exec_module(live)


def _ns(d):
    return types.SimpleNamespace(**(d or {}))


def live_out(rec):
    func = rec.get("func")
    if func == "parse_timeout":
        return live._parse_timeout(rec.get("raw"))
    if func == "normalized_url":
        return live._normalized_runtime_url(rec.get("value"))
    if func == "merge":
        return live._merge_request_overrides(rec.get("runtime"), rec.get("explicit"))
    if func == "credential_bundle":
        return live._credential_bundle(
            rec.get("model"), rec.get("provider"), rec.get("base_url"), rec.get("api_key"),
            rec.get("api_mode"), rec.get("request_overrides"), **(rec.get("extra") or {}))
    if func == "inherit_caps":
        return live._inherit_parent_capabilities(
            _ns(rec.get("parent_agent") or {}), rec.get("override_provider"), rec.get("override_base_url"))
    if func == "gateway_inline":
        return live.gateway_delegation_inline(
            rec.get("user_config"), rec.get("platform_key", ""), interim_enabled=bool(rec.get("interim_enabled", False)))
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
        if rec.get("func") not in ("parse_timeout", "normalized_url", "merge", "credential_bundle",
                                    "inherit_caps", "gateway_inline"):
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
