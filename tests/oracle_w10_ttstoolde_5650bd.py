#!/usr/bin/env python3
"""Differential oracle for gate w10_ttstoolde_5650bd (tools/tts_tool_delivery.py)."""
import importlib.util
import json
import sys

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/tools/tts_tool_delivery.py"
spec = importlib.util.spec_from_file_location("w10ttsde_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def norm(v):
    if isinstance(v, live.AudioDeliveryProfile):
        return {"platform": v.platform, "max_file_bytes": v.max_file_bytes,
                "safety_ratio": v.safety_ratio, "target_file_bytes": v.target_file_bytes}
    return v


def compute(r):
    f = r.get("func")
    if f == "section":
        return live._section(r.get("cfg"), r.get("key", ""))
    if f == "posint":
        return live._positive_int(r.get("value"))
    if f == "maxlen":
        return live._resolve_max_text_length(r.get("provider", ""), r.get("cfg"))
    if f == "profile":
        return norm(live._resolve_audio_delivery_profile(r.get("platform", ""), r.get("cfg")))
    if f == "pack":
        return live._pack_under_cap(r.get("pieces") or [], r.get("max_chars", 10),
                                    slice_oversized=bool(r.get("slice_oversized", False)))
    if f == "splitlong":
        return live._split_oversized_sentence(r.get("sentence", ""), r.get("max_chars", 10))
    if f == "splittext":
        return live._split_text_for_tts(r.get("text", ""), r.get("max_chars", 4000))
    if f == "sidecar":
        return live._wav_sidecar_path(r.get("path", ""))
    if f == "wav":
        return list(live._wrap_pcm_as_wav(bytes(r.get("pcm") or []),
                                           r.get("rate", 24000), r.get("channels", 1), r.get("width", 2)))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except BaseException as e:
        py = {"__raise__": type(e).__name__}
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w10_ttstoolde_5650bd: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
