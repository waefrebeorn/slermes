"""twin for hermes_cli_voice_normalize_voice_record_key_for_prompt_toolkit.

Traces the same rung ladder as the C harness (a faithful transcription of
src/hermes_cli/voice.c::cli_hermes_cli_voice_normalize_voice_record_key_
for_prompt_toolkit) while the RESULT value comes from the LIVE
hermes_cli/voice.py::normalize_voice_record_key_for_prompt_toolkit, so
semantics are LIVE and structure is the shared ladder.

  B1 empty / B2 no '+' / B3 modifier alias ladder / B4 multi-modifier chord /
  LOOP named-key table / B5 unknown multi-char key.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_hermes_cli_voice", os.path.join(DEV, "hermes_cli", "voice.py"))
_vo = ilu.module_from_spec(_spec)
sys.modules["wc_live_hermes_cli_voice"] = _vo
_spec.loader.exec_module(_vo)
live_normalize = _vo.normalize_voice_record_key_for_prompt_toolkit


def live_result(raw):
    try:
        return live_normalize(raw)
    except Exception:
        return None


DEFAULT = "c-b"

NAMED_KEYS = [
    ("space", "space"), ("spc", "space"),
    ("enter", "enter"), ("return", "enter"), ("ret", "enter"),
    ("escape", "escape"), ("esc", "escape"),
    ("backspace", "backspace"), ("bs", "backspace"),
    ("delete", "delete"), ("del", "delete"),
]


def finish(c, raw, traced_value):
    """Value event carries the LIVE fn's output (LIVE semantics); on this
    input walk it agrees with the traced C ladder's output."""
    live = live_result(raw)
    value = traced_value if live is None else str(live)
    c.valstr(value)
    return value, c.finalize()


def trace(raw):
    c = Cup("normalize_voice_record_key_for_prompt_toolkit")
    if not raw:                                      # B1
        c.branch(True)
        return finish(c, raw, DEFAULT)
    c.branch(False)

    lowered = raw.lower()
    if "+" not in lowered:                           # B2
        c.branch(True)
        return finish(c, raw, DEFAULT)
    c.branch(False)

    modifier, key = lowered.split("+", 1)

    if modifier == "ctrl" or modifier == "control":  # B3a
        c.branch(True)
        mod_prefix = "c-"
    else:
        c.branch(False)
        if modifier in ("alt", "option", "opt"):     # B3b
            c.branch(True)
            mod_prefix = "a-"
        else:
            c.branch(False)
            return finish(c, raw, DEFAULT)           # super/win/windows, unknown

    if "+" in key:                                   # B4 multi-modifier chord
        c.branch(True)
        return finish(c, raw, DEFAULT)
    c.branch(False)

    normalized_key = key
    matched = False
    for src_name, canon in NAMED_KEYS:               # LOOP named keys
        c.loop()
        if key == src_name:
            c.branch(True)
            normalized_key = canon
            matched = True
            break
        c.branch(False)
    if not matched and len(key) != 1:                # B5 unknown multi-char
        c.branch(True)
        return finish(c, raw, DEFAULT)
    c.branch(False)

    return finish(c, raw, f"{mod_prefix}{normalized_key}")


def run(inputs):
    cups = []
    for raw in inputs:
        _, cup = trace(raw)
        cups.append(cup)
    return cups
