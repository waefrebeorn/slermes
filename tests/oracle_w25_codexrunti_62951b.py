#!/usr/bin/env python3
"""Differential oracle for agent/codex_runtime_history_seed.py
(gate w25_codexrunti_62951b).

The live module is pure: ``render_history_seed`` reads only the message list it
is handed, so the whole renderer (including the 32k tail cap) is compared
verbatim against the C port.  Nothing is stubbed: the codex app-server seam is
the caller, not the function.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_codexrunti_62951b"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent import codex_runtime_history_seed as live  # noqa: E402

WANTED = {"text_of", "render_row", "render_seed", "constants"}


def text_of(req):
    return live._text_of(req.get("content"))


def render_row(req):
    return live._render_row(req.get("msg"))


def render_seed(req):
    return live.render_history_seed(req.get("messages"))


def constants(_req):
    return {
        "max_history_seed_chars": live.MAX_HISTORY_SEED_CHARS,
        "tool_result_preview_chars": live._TOOL_RESULT_PREVIEW_CHARS,
        "header": live._HEADER,
    }


HANDLERS = {
    "text_of": text_of,
    "render_row": render_row,
    "render_seed": render_seed,
    "constants": constants,
}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = HANDLERS[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), str(py)[:400], str(got)[:400]),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
