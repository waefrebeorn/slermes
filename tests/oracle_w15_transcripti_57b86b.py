#!/usr/bin/env python3
"""Differential oracle for tools/transcription_command.py (gate w15_transcripti_57b86b)."""
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
G = "w15_transcripti_57b86b"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _noop(*args, **kwargs):
    return None


_ag = _mod("agent")
_ag.__path__ = []
_mod("agent.model_metadata", CHARS_PER_TOKEN=4)
_to = _mod("tools")
_to.__path__ = []
_mod("tools.tts_command_provider",
     _command_output_format=_noop, _command_timeout=_noop, _is_command_provider_config=_noop,
     _named_provider_config=_noop, _resolve_command_config=_noop, command_env_passthrough=_noop,
     command_failure_detail=_noop, render_command_template=_noop, run_command_provider=_noop)
_mod("tools.transcription_audio", _transcode_audio_for_stt=_noop)
_mod("tools.transcription_common",
     BUILTIN_STT_PROVIDERS=frozenset({"local", "local_command", "groq", "openai", "mistral", "xai"}),
     _error_result=lambda error, **extra: {"success": False, "transcript": "", "error": error, **extra},
     _log_prompt_unsupported=_noop,
     _ok_result=lambda transcript, provider: {"success": True, "transcript": transcript, "provider": provider})
_ut = _mod("utils")
_ut.__path__ = []
_ut.is_truthy_value = lambda v, default=False: bool(v) if v is not None else default

SRC = QUARRY + "/tools/transcription_command.py"
_spec = importlib.util.spec_from_file_location("transcription_command_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["transcription_command_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "enforce_prompt":
        return live._enforce_prompt_length_limit(rec.get("prompt"), rec.get("provider", ""))
    if func == "unregistered_error":
        return live._unregistered_stt_provider_error(rec.get("provider"))
    return None


def main():
    cases = mism = 0
    allowed = ("enforce_prompt", "unregistered_error")
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in allowed:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
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
