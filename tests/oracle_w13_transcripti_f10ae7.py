#!/usr/bin/env python3
"""Differential oracle for Hermes tools/transcription_local.py (w13_transcripti_f10ae7).

Pure local helpers only. Imports of transcription_audio/transcription_common are
stubbed with faithful constants and _config_number so the live module loads.
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
_mod("tools.transcription_audio",
     _find_whisper_binary=lambda: None,
     _prepare_local_audio=lambda p, w: (p, None),
     _run_quiet=lambda *a, **kw: None)

_catalog = {
    "local": ["base", "tiny", "small", "medium", "large-v3", "turbo"],
    "groq": ["whisper-large-v3-turbo", "whisper-large-v3"],
    "openai": ["whisper-1", "gpt-4o-mini-transcribe", "gpt-4o-transcribe", "gpt-transcribe"],
    "mistral": ["voxtral-mini-latest", "voxtral-mini-2602"],
    "xai": ["grok-voice-transcribe-2.0", "grok-voice-transcribe-1.0"],
    "elevenlabs": ["scribe_v2", "scribe_v1"]}
_mod("tools.transcription_common",
     DEFAULT_LOCAL_MODEL="base",
     DEFAULT_LOCAL_STT_LANGUAGE="en",
     GROQ_MODELS=frozenset(_catalog["groq"]),
     LOCAL_STT_COMMAND_ENV="HERMES_LOCAL_STT_COMMAND",
     RETIRED_GROQ_MODELS=frozenset({"distil-whisper-large-v3-en"}),
     OPENAI_MODELS=frozenset(_catalog["openai"]),
     STT_MODEL_CATALOG=_catalog,
     _config_number=_config_number,
     _error_result=lambda error, **extra: {"success": False, "transcript": "", "error": error, **extra},
     _log_prompt_unsupported=lambda label: None,
     _ok_result=lambda text, provider: {"success": True, "transcript": text, "provider": provider},
     _process_error_detail=lambda exc: str(exc))

SRC = Q + "/tools/transcription_local.py"
spec = importlib.util.spec_from_file_location("tools.local_w13_f10ae7_live", SRC)
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
        if func == "normalize_language":
            py = live._normalize_local_stt_language(r.get("language"), r.get("supported"))
        elif func == "normalize_model":
            py = live._normalize_local_model(r.get("model"))
        elif func == "idle_unload":
            py = live._get_idle_unload_seconds(r.get("cfg"))
        elif func == "cuda_error":
            py = live._looks_like_cuda_lib_error(r.get("msg"))
        elif func == "confidence_thresholds":
            py = list(live._confidence_thresholds(r.get("cfg")))
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

print(f"ORACLE w13_transcripti_f10ae7: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
