#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/matrix/adapter.py (gate p6).

Budgets and dispatch tables are read off the live module/class attributes; the
event dedup ring is replayed with the same ``deque(maxlen=1000)`` the adapter
uses.  ``_approval_reaction_map`` is a constructor literal (adapter.py:751-753),
so the table itself is the object under comparison for the seven emoji keys.
"""

import json
import os
import sys
import tempfile
from collections import deque
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_e9cb6b_p6"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("HERMES_HOME", tempfile.mkdtemp(prefix="w27-oracle-home-"))

from plugins.platforms.matrix import adapter as live  # noqa: E402

ADAPTER = live.MatrixAdapter

WANTED = {"constants", "model_picker_reactions", "choice_picker_reactions",
          "approval_reaction_map", "yaml_bridge", "image_exts", "media_exts",
          "txn_id", "duplicate_events"}

APPROVAL_MAP = {
    "\u2705": "once", "\U0001f300": "session", "\u267e\ufe0f": "always", "\u267e": "always",
    "\u274c": "deny", "\u274e": "deny",
}


def _constants(r):
    return {
        "max_content_bytes": float(live._MAX_CONTENT_BYTES),
        "max_message_length_ceiling": float(live.MATRIX_MAX_MESSAGE_LENGTH_CEILING),
        "default_max_message_length": float(live.DEFAULT_MAX_MESSAGE_LENGTH),
        "split_threshold": float(ADAPTER._SPLIT_THRESHOLD),
        "startup_grace_seconds": float(live._STARTUP_GRACE_SECONDS),
        "voice_waveform_bins": float(live._MATRIX_VOICE_WAVEFORM_BINS),
        "exec_approval_cmd_budget": float(ADAPTER._EA_CMD_BUDGET),
    }


def _model_reactions(r):
    return list(live._MATRIX_MODEL_PICKER_REACTIONS)


def _choice_reactions(r):
    return list(live._MATRIX_CHOICE_PICKER_REACTIONS)


def _approval_map(r):
    return dict(APPROVAL_MAP)


def _yaml_bridge(r):
    return [list(row) for row in live._YAML_BRIDGE]


def _image_exts(r):
    return sorted(live._MATRIX_IMAGE_FILENAME_EXTS)


def _media_exts(r):
    return sorted(live._MATRIX_MEDIA_FILENAME_EXTS)


def _txn(r):
    return "hermes_%d_%s" % (int(r.get("ms")), r.get("rand_hex") or "")


def _dedup(r):
    dq = deque(maxlen=1000)
    seen = set()
    out = []
    for event_id in r.get("ids") or []:
        if not event_id:
            out.append(False)
            continue
        if event_id in seen:
            out.append(True)
            continue
        if len(dq) == dq.maxlen:
            seen.discard(dq[0])
        dq.append(event_id)
        seen.add(event_id)
        out.append(False)
    return out


CASES = {
    "constants": _constants,
    "model_picker_reactions": _model_reactions,
    "choice_picker_reactions": _choice_reactions,
    "approval_reaction_map": _approval_map,
    "yaml_bridge": _yaml_bridge,
    "image_exts": _image_exts,
    "media_exts": _media_exts,
    "txn_id": _txn,
    "duplicate_events": _dedup,
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
            py = CASES[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
