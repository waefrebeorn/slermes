"""Oracle for tools/voice_mode.py pure helpers + staticmethods.

Reads a JSON array fixture from argv[1]; each element is {"op":<fn>, ...args}.
Recomputes from the LIVE Python source and emits each result on its own line.
"""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT") or os.path.expanduser("~/.hermes/hermes-agent")
PYAGENT = os.path.join(DEV_ROOT, "tools")
if PYAGENT not in sys.path:
    sys.path.insert(0, PYAGENT)
PYROOT = DEV_ROOT
if PYROOT not in sys.path:
    sys.path.insert(0, PYROOT)

import importlib
voice_mode = importlib.import_module("tools.voice_mode")

# Monkey-patch load_config so the *pure* helpers (which read config internally
# via `from hermes_cli.config import load_config`) pick up fixture data.
_current_voice_cfg = {}


class _MockConfigModule:
    def get(self, key, default=None):
        if key == "voice":
            return _current_voice_cfg
        return default


class _MockConfigPackage:
    load_config = staticmethod(lambda: _MockConfigModule())


import sys, types
_hc_mod = types.ModuleType("hermes_cli")
_hc_mod.__path__ = []
_cfg_mod = types.ModuleType("hermes_cli.config")
_cfg_mod.load_config = lambda: _MockConfigModule()
_hc_mod.config = _cfg_mod
sys.modules["hermes_cli"] = _hc_mod
sys.modules["hermes_cli.config"] = _cfg_mod


def run(c):
    op = c.get("op")
    vc = c.get("voice_cfg")
    if vc is not None:
        global _current_voice_cfg
        _current_voice_cfg = vc
    if op == "is_nan":
        return voice_mode._is_nan(c.get("value", 0.0))
    if op == "get_beep_volume":
        return voice_mode._get_beep_volume()
    if op == "_sounddevice_output_allowed":
        return voice_mode._sounddevice_output_allowed()
    if op == "_is_wsl":
        return voice_mode._is_wsl()
    if op == "_is_wsl2_env":
        return voice_mode._is_wsl2_env()
    if op == "_wsl_powershell_tts_available":
        return voice_mode._wsl_powershell_tts_available()
    if op == "_voice_debug_enabled":
        return voice_mode._voice_debug_enabled()
    if op == "_vad_log":
        voice_mode._vad_log(c.get("msg", ""))
        return None
    if op == "_load_voice_stop_phrases":
        phrases = voice_mode._load_voice_stop_phrases()
        return list(phrases)
    if op == "is_voice_stop_phrase":
        trans = c.get("transcript", "")
        pf = c.get("phrases")
        if pf:
            phrases = tuple(json.loads(pf))
        else:
            phrases = voice_mode._load_voice_stop_phrases()
        return voice_mode.is_voice_stop_phrase(trans, phrases)
    if op == "voice_stop_hint":
        pf = c.get("phrases")
        if pf:
            phrase_list = json.loads(pf)
            _current_voice_cfg["stop_phrases"] = phrase_list
        phrases = voice_mode._load_voice_stop_phrases()
        return voice_mode.voice_stop_hint()
    if op == "thinking_sound_enabled":
        return voice_mode.thinking_sound_enabled()
    if op == "mark_audio_output_active":
        active = c.get("active", False)
        voice_mode.mark_audio_output_active(active)
        return voice_mode.is_audio_output_active()
    if op == "audio_recorder_max_duration_reached":
        cap = c.get("cap", 0.0)
        elapsed = c.get("elapsed", 0.0)
        # AudioRecorder() may trigger heavy imports; fall back to a bare object
        # that borrows the real _max_duration_reached method.
        try:
            rec = voice_mode.AudioRecorder.__new__(voice_mode.AudioRecorder)
        except Exception:
            rec = object.__new__(object)
        rec._max_recording_seconds = cap
        return voice_mode.AudioRecorder._max_duration_reached(rec, elapsed)
    return None


def main():
    with open(sys.argv[1]) as f:
        cases = json.load(f)
    for c in cases:
        r = run(c)
        if r is None:
            print("null")
        elif isinstance(r, bool):
            print("true" if r else "false")
        elif isinstance(r, (int, float)):
            if isinstance(r, float) and r == int(r):
                print(int(r))
            else:
                print(r)
        elif isinstance(r, str):
            # escape like json.dumps string
            print(json.dumps(r))
        elif isinstance(r, list):
            print(json.dumps(r, sort_keys=True))
        else:
            print(json.dumps(r, sort_keys=True))


if __name__ == "__main__":
    main()
