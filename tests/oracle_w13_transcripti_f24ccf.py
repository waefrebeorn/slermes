#!/usr/bin/env python3
"""Differential oracle for Hermes tools/transcription_cloud.py (w13_transcripti_f24ccf).

Only pure helpers are exercised. Heavy module imports (utils, transcription_audio,
transcription_common) are stubbed with faithful minimal surfaces so the live
module loads. STTResponseError is stubbed and used for the error-envelope cases.
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


class STTResponseError(ValueError):
    pass


u = _mod("utils", is_truthy_value=is_truthy_value)
u.__path__ = []
_mod("tools.transcription_audio", _transcode_audio_for_stt=lambda *a, **kw: (None, "stub"))
_mod("tools.transcription_common",
     DEFAULT_GROQ_STT_MODEL="whisper-large-v3-turbo",
     DEFAULT_STT_MODEL="whisper-1",
     ELEVENLABS_STT_BASE_URL="https://api.elevenlabs.io/v1",
     GROQ_BASE_URL="https://api.groq.com/openai/v1",
     GROQ_MODELS=frozenset({"whisper-large-v3-turbo", "whisper-large-v3"}),
     OPENAI_BASE_URL="https://api.openai.com/v1",
     OPENAI_MODELS=frozenset({"whisper-1", "gpt-4o-mini-transcribe",
                              "gpt-4o-transcribe", "gpt-transcribe"}),
     RETIRED_GROQ_MODELS=frozenset({"distil-whisper-large-v3-en"}),
     STTResponseError=STTResponseError,
     XAI_STT_BASE_URL="https://api.x.ai/v1",
     _config_number=_config_number,
     _error_result=lambda error, **extra: {"success": False, "transcript": "", "error": error, **extra},
     _get_stt_section=lambda cfg, name: (cfg or {}).get(name) or {},
     _lazy_ensure_quietly=lambda extra: None,
     _log_prompt_unsupported=lambda label: None,
     _ok_result=lambda text, provider: {"success": True, "transcript": text, "provider": provider},
     normalize_xai_stt_model=lambda m: str(m or "") or "grok-voice-transcribe-2.0")

SRC = Q + "/tools/transcription_cloud.py"
spec = importlib.util.spec_from_file_location("tools.cloud_w13_f24ccf_live", SRC)
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
        if func == "sdk_prompt_kwargs":
            py = live._sdk_prompt_kwargs(r.get("language"), r.get("prompt"))
        elif func == "elevenlabs_error_detail":
            py = live._elevenlabs_error_detail(r.get("body"))
        elif func == "extract_transcript_text":
            try:
                py = live._extract_transcript_text(r.get("value"))
            except live.STTResponseError as exc:
                py = {"error": str(exc)}
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

print(f"ORACLE w13_transcripti_f24ccf: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
