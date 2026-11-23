#!/usr/bin/env python3
"""Differential oracle for hermes_cli/setup_tts.py (w7_setuptts_e5dee4)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, pkg=False, **attrs):
    m = types.ModuleType(name)
    if pkg:
        m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_mod("hermes_cli", pkg=True)
_mod("tools", pkg=True)
_mod("tools.tool_backend_helpers", managed_nous_tools_enabled=lambda: False)


class Setup:
    env = {}
    pv = ""

    @staticmethod
    def get_env_value(v):
        return Setup.env.get(v, "")

    @staticmethod
    def prompt(label, password=False, **kwargs):
        return Setup.pv

    @staticmethod
    def save_env_value(*a, **k):
        pass

    @staticmethod
    def print_info(*a, **k):
        pass

    @staticmethod
    def print_success(*a, **k):
        pass

    @staticmethod
    def print_warning(*a, **k):
        pass

    @staticmethod
    def print_error(*a, **k):
        pass


_mod("hermes_cli.setup",
     get_env_value=Setup.get_env_value, prompt=Setup.prompt,
     save_env_value=Setup.save_env_value, print_info=Setup.print_info,
     print_success=Setup.print_success, print_warning=Setup.print_warning,
     print_error=Setup.print_error)

SRC = Q + "/hermes_cli/setup_tts.py"
spec = importlib.util.spec_from_file_location("w7_setuptts_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_setuptts_live"] = live
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
        if func == "provider_label":
            py = live._TTS_PROVIDER_LABELS.get(r.get("key"))
        elif func == "all_labels":
            py = live._TTS_PROVIDER_LABELS
        elif func == "tts_api_key_step":
            Setup.env = r.get("env", {})
            Setup.pv = r.get("prompt_value", "")
            py = live._tts_api_key_step(r.get("selected", ""))
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

print(f"ORACLE w7_setuptts_e5dee4: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
