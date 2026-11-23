#!/usr/bin/env python3
"""Differential oracle for Hermes tools/tts_command_provider.py (gate w14_ttscommand_bd97e2).

hermes_cli.process_identity / utils are stubbed (module-level imports only).
os_name is pinned per fixture so the Windows-vs-POSIX quoting branch is
deterministic on this Windows oracle host. Windows-unsafe placeholder values
(live raises ValueError) are not exercised.
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


tools = _mod("tools")
tools.__path__ = []
hcli = _mod("hermes_cli")
hcli.__path__ = []
_mod("hermes_cli.process_identity",
     spawn_owned=lambda *a, **k: None,
     release_process_group_once_empty=lambda *a, **k: None)
_mod("utils", is_truthy_value=lambda v, default=False: bool(v) if v is not None else default)

SRC = Q + "/tools/tts_command_provider.py"
spec = importlib.util.spec_from_file_location("w14_ttscommand_bd97e2_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w14_ttscommand_bd97e2_live"] = live
spec.loader.exec_module(live)


class _Err:
    def __init__(self, stderr, stdout):
        self.stderr = stderr
        self.stdout = stdout


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
    os_name = r.get("os_name", "posix")
    try:
        if func == "quote_context":
            py = live.shell_quote_context(r.get("command_template", ""), r.get("position", 0))
        elif func == "quote_placeholder":
            py = live.quote_command_placeholder(r.get("value", ""), r.get("quote_context"), os_name)
        elif func == "render":
            py = live.render_command_template(r.get("command_template", ""),
                                              r.get("placeholders", {}), os_name)
        elif func == "failure_detail":
            py = live.command_failure_detail(_Err(r.get("stderr"), r.get("stdout")))
        elif func == "provider_section":
            py = live._get_provider_section(r.get("config", {}), r.get("name", "providers"))
        elif func == "is_command_config":
            py = live._is_command_provider_config(r.get("config"))
        elif func == "named_config":
            py = live._named_provider_config(r.get("config", {}), r.get("name", ""),
                                             frozenset(r.get("builtins", [])))
        elif func == "resolve_config":
            py = live._resolve_command_config(r.get("provider"), r.get("config", {}),
                                              frozenset(r.get("reserved", [])))
        elif func == "command_timeout":
            py = live._command_timeout(r.get("config", {}), r.get("default", 120.0))
        elif func == "command_output_format":
            py = live._command_output_format(r.get("config", {}), frozenset(r.get("formats", [])),
                                             r.get("default", "mp3"))
        elif func == "tts_output_format":
            py = live._get_command_tts_output_format(r.get("config", {}), r.get("output_path"))
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

print(f"ORACLE w14_ttscommand_bd97e2: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
