#!/usr/bin/env python3
"""Differential oracle for w12_cliinitmi_95d44d (hermes_cli/cli_init_mixin.py).

CLI attributes, resolved auto-load skills, and the store's current snapshot are
injected from the fixture; the live helpers run unmodified with those seams.
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


# Parent-package bootstrap before executing the live module by file path.
hermes_cli = _mod("hermes_cli")
hermes_cli.__path__ = []
_mod("hermes_cli.fallback_config", get_fallback_chain=lambda cfg: None)
rich = _mod("rich")
rich.__path__ = []
_mod("rich.console", Console=lambda *a, **k: None)
session_rules = _mod("session_rules")
session_rules.__path__ = []
_mod("session_rules.session_ids", new_session_id=lambda now: "sid")
_mod("utils", base_url_host_matches=lambda a, b: False,
     base_url_hostname=lambda s: "", is_truthy_value=lambda v: bool(v))

agent = _mod("agent")
agent.__path__ = []
_mod("agent.i18n", t=lambda key, **kw: key)
AUTO = []
_mod("agent.skill_commands", resolve_auto_load_skills=lambda cfg: list(AUTO))
_mod("cli", CLI_CONFIG={})

SRC = Q + "/hermes_cli/cli_init_mixin.py"
spec = importlib.util.spec_from_file_location("w12_cliinitmi_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w12_cliinitmi_live"] = live
spec.loader.exec_module(live)


def _cli(d, printed=None):
    ns = types.SimpleNamespace(**dict(d))
    ns._console_print = lambda line: printed.append(line) if printed is not None else None
    return ns


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
        if func == "explicit_launch_skills":
            AUTO = list(r.get("auto_skills", []))
            py = live._explicit_launch_skills(_cli(r.get("cli", {})))
        elif func == "gateway_launch":
            AUTO = list(r.get("auto_skills", []))
            py = live._gateway_launch(_cli(r.get("cli", {})))
        elif func == "adopt_gateway_toolsets":
            printed = []
            cli = _cli(r.get("cli", {}), printed)
            store = types.SimpleNamespace(current=r.get("store", {}).get("current"))
            live._adopt_gateway_toolsets(cli, store)
            py = {"enabled_toolsets": getattr(cli, "enabled_toolsets", None),
                  "stdout": printed}
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
print(f"ORACLE w12_cliinitmi_95d44d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
