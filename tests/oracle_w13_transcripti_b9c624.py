#!/usr/bin/env python3
"""Differential oracle for Hermes tools/transcription_audio.py (w13_transcripti_b9c624).

The live module imports utils.is_truthy_value and tools.transcription_common;
both are stubbed with faithful implementations so the pure _cloud_trim_settings
resolver loads and runs without dragging in ffmpeg/subprocess machinery.
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


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = _mod(name)
    if not hasattr(m, "__path__"):
        m.__path__ = []
    return m


_pkg("tools")
_pkg("hermes_cli")

_TRUTHY = {"1", "true", "yes", "on"}


def is_truthy_value(value, default=False):
    if value is None:
        return default
    if isinstance(value, str):
        return value.strip().lower() in _TRUTHY
    return bool(value)


def _config_number(cfg, key, default, cast=float):
    try:
        return cast(cfg.get(key, default))
    except (TypeError, ValueError):
        return default


u = _mod("utils", is_truthy_value=is_truthy_value)
u.__path__ = []
_mod("hermes_cli._subprocess_compat", windows_hide_flags=lambda: 0)
_mod("tools.transcription_common",
     COMMON_LOCAL_BIN_DIRS=("/opt/homebrew/bin", "/usr/local/bin"),
     LOCAL_NATIVE_AUDIO_FORMATS={".wav", ".aiff", ".aif"},
     MAX_FILE_SIZE=25 * 1024 * 1024,
     SUPPORTED_FORMATS={".mp3", ".mp4", ".mpeg", ".mpga", ".m4a", ".wav", ".webm",
                        ".ogg", ".oga", ".opus", ".aac", ".flac", ".caf"},
     _config_number=_config_number,
     _error_result=lambda error, **extra: {"success": False, "transcript": "", "error": error, **extra},
     _lazy_ensure_quietly=lambda extra: None,
     _process_error_detail=lambda exc: str(exc))

SRC = Q + "/tools/transcription_audio.py"
spec = importlib.util.spec_from_file_location("tools.audio_w13_b9c624_live", SRC)
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
        if func == "cloud_trim_settings":
            py = list(live._cloud_trim_settings(r.get("cfg")))
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

print(f"ORACLE w13_transcripti_b9c624: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
