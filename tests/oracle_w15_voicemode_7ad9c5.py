#!/usr/bin/env python3
"""Differential oracle for tools/voice_mode_transcript.py (gate w15_voicemode_7ad9c5).

Loads the LIVE module via importlib and compares the C harness `out` for:
  is_whisper_hallucination, _normalize_for_echo_compare, is_tts_echo,
  is_voice_stop_phrase (explicit stop phrases), voice_stop_hint (phrases
  injected by patching the module's _load_voice_stop_phrases).
"""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

_spec = importlib.util.spec_from_file_location(
    "vm_live", QUARRY + "/tools/voice_mode_transcript.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["vm_live"] = live
_spec.loader.exec_module(live)

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
        if func == "hallucination":
            py = live.is_whisper_hallucination(r.get("text") or "")
        elif func == "normalize":
            py = live._normalize_for_echo_compare(r.get("text") or "")
        elif func == "echo":
            py = live.is_tts_echo(r.get("text") or "", r.get("spoken") or "",
                                  r.get("threshold", 0.6))
        elif func == "stop_phrase":
            py = live.is_voice_stop_phrase(r.get("text") or "", tuple(r.get("phrases") or []))
        elif func == "stop_hint":
            phrases = tuple(r.get("phrases") or [])
            saved = live._load_voice_stop_phrases
            live._load_voice_stop_phrases = lambda: phrases
            try:
                py = live.voice_stop_hint()
            finally:
                live._load_voice_stop_phrases = saved
        else:
            continue
    except Exception as e:
        print("PY-ERROR %r: %s" % (r, e), file=sys.stderr)
        mism += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w15_voicemode_7ad9c5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
