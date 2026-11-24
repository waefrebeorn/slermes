#!/usr/bin/env python3
"""Differential oracle for tools/skills_guard_credentials.py
(gate w21_skillsguar_8a9cfb).

The module is a leaf (io / re / tokenize only), so everything runs live:

  pattern_source -- `_closed(depth)` and `_open_prefix(depth)`, the exact regex
                    text the module builds.
  dotenv         -- `SKILL_ONLY_PATTERNS[0][0].search(view)`, the compiled
                    Hermes-.env pattern, on the same view a .py scan hands it.
  read_names     -- `_ENV_READ_NAME_RE.finditer(line)` group 1, in order.
  raise          -- `raise_provider_key_reads` over real Finding objects, with
                    `model_provider_key_names` bound to the fixture's provider
                    set (that reader is the config seam: PROVIDER_REGISTRY +
                    config + the child-env blocklist) and `in_text` rebuilt as
                    the 0/1 bytearray mask `code_views` produces.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tools" / "skills_guard_credentials.py"
GATE = "w21_skillsguar_8a9cfb"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("skills_guard_credentials_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["skills_guard_credentials_live"] = live
_spec.loader.exec_module(live)

WANTED = {"pattern_source", "dotenv", "read_names", "raise"}

DOTENV_RE = live.SKILL_ONLY_PATTERNS[0][0]
READ_RE = live._ENV_READ_NAME_RE


class Finding:
    def __init__(self, d):
        self.pattern_id = d.get("pattern_id")
        self.line = d.get("line")
        self.severity = d.get("severity")
        self.description = d.get("description")

    def as_dict(self):
        return {"pattern_id": self.pattern_id, "line": self.line,
                "severity": self.severity, "description": self.description}


def raise_reads(findings, lines, providers, in_text):
    objs = [Finding(f) for f in findings or []]
    mask = None
    if isinstance(in_text, list):
        mask = [bytearray(1 if ch == "1" else 0 for ch in row) for row in in_text]
    original = live.model_provider_key_names
    live.model_provider_key_names = lambda: frozenset(providers or [])
    try:
        live.raise_provider_key_reads(objs, lines or [], mask)
    finally:
        live.model_provider_key_names = original
    return [o.as_dict() for o in objs]


def expected(r):
    f = r.get("func")
    if f == "pattern_source":
        d = int(r.get("depth") or 1)
        return {"closed": live._closed(d), "open_prefix": live._open_prefix(d)}
    if f == "dotenv":
        return 1 if DOTENV_RE.search(r.get("text") or "") else 0
    if f == "read_names":
        return [m.group(1) for m in READ_RE.finditer(r.get("line") or "")]
    if f == "raise":
        return raise_reads(r.get("findings"), r.get("lines"), r.get("providers"),
                           r.get("in_text"))
    raise AssertionError("unknown func %r" % (f,))


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
