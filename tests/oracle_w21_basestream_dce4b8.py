#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/base_streaming_tts.py (gate w21_basestream_dce4b8).

The live module is pure stdlib (two dataclasses and two helpers), so the oracle
imports it and calls the real code: `AudioFormat(**overrides)` /
`StreamingTTSHandle(**overrides)` through `dataclasses.asdict`, and the two
turn-scoped helpers. A fixture's nested `audio_format` object is the
AudioFormat constructor's kwargs (the C side has no Python objects to hand in).
No seam is stubbed: the module touches no DB, network, config or clock.
"""
import importlib.util
import json
import sys
import types
from dataclasses import asdict
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "platforms" / "base_streaming_tts.py"
GATE = "w21_basestream_dce4b8"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("base_streaming_tts_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["base_streaming_tts_live"] = live
_spec.loader.exec_module(live)


def err_of(exc):
    return {"status": "error", "type": type(exc).__name__, "message": str(exc)}


def audio_kwargs(raw):
    if raw is None:
        return {}
    if not isinstance(raw, dict):
        return raw
    return {k: v for k, v in raw.items()}


def handle_kwargs(raw):
    if not isinstance(raw, dict):
        return {}
    kwargs = dict(raw)
    nested = kwargs.get("audio_format", None)
    if isinstance(nested, dict):
        kwargs["audio_format"] = live.AudioFormat(**nested)
    return kwargs


def event_of(raw):
    """The real callers hand `message_id` / `platform_update_id` attributes; a fixture
    object stands in for that event object."""
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


def expected(r):
    f = r.get("func")
    if f == "audio_format":
        try:
            return {"status": "ok", "value": asdict(live.AudioFormat(**audio_kwargs(r.get("overrides"))))}
        except Exception as exc:  # noqa: BLE001 - surfaced exactly as the C side shapes it
            return err_of(exc)
    if f == "handle":
        try:
            return asdict(live.StreamingTTSHandle(**handle_kwargs(r.get("overrides"))))
        except Exception as exc:  # noqa: BLE001
            return err_of(exc)
    if f == "turn_key":
        return live.streaming_tts_turn_key(r.get("session_key"), r.get("turn_marker"),
                                           event=event_of(r.get("event")))
    if f == "should_skip":
        try:
            return {"status": "ok",
                    "value": live.streaming_tts_should_skip_whole_file(
                        r.get("completed_turns"), r.get("session_key"),
                        r.get("turn_marker"), event=event_of(r.get("event")))}
        except Exception as exc:  # noqa: BLE001
            return err_of(exc)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"audio_format", "handle", "turn_key", "should_skip"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
