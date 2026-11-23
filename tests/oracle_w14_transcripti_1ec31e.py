#!/usr/bin/env python3
"""Differential oracle for Hermes tools/transcription_common.py (gate w14_transcripti_1ec31e).

tools.tts_command_provider is stubbed to its one imported symbol. STT_XAI_MODEL
is injected via an os.getenv shim from the fixture (never mutating the process
environment: Windows CRT setenv("") unsets while Python os.environ[name]="" keeps it).
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
_mod("tools.tts_command_provider",
     _get_provider_section=lambda config, name:
         config.get(name) if isinstance(config, dict) and isinstance(config.get(name), dict) else {})

SRC = Q + "/tools/transcription_common.py"
spec = importlib.util.spec_from_file_location("w14_transcripti_1ec31e_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w14_transcripti_1ec31e_live"] = live
spec.loader.exec_module(live)

_orig_getenv = live.os.getenv
_env = {"value": ""}


def _getenv(name, default=None):
    if name == "STT_XAI_MODEL":
        return _env["value"]
    return _orig_getenv(name, default)


live.os.getenv = _getenv


class _Err:
    def __init__(self, stderr, stdout, text):
        self.stderr = stderr
        self.stdout = stdout
        self._text = text

    def __str__(self):
        return self._text


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
        if func == "error_result":
            py = live._error_result(r.get("error", ""), **(r.get("extra") or {}))
        elif func == "ok_result":
            py = live._ok_result(r.get("transcript", ""), r.get("provider", ""))
        elif func == "normalize_xai":
            _env["value"] = r.get("env_value", "")
            py = live.normalize_xai_stt_model(r.get("model"))
        elif func == "config_number":
            cast = float if r.get("cast", "float") == "float" else int
            py = live._config_number(r.get("cfg", {}), r.get("key", ""),
                                     r.get("default", 0), cast=cast)
        elif func == "process_error_detail":
            py = live._process_error_detail(
                _Err(r.get("stderr"), r.get("stdout"), r.get("exc_str", "")))
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

print(f"ORACLE w14_transcripti_1ec31e: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
