#!/usr/bin/env python3
"""Differential oracle for hermes_cli/dashboard_auth/audit.py (gate w22_audit_b943f2).

The live module imports cleanly (stdlib only), so `_bounded_value`,
`AuditEvent`, `_REDACTED_FIELDS` and `_rotate_if_needed` run unmodified. Two
seams are injected:

  ts        -- the clock; the fixture carries the timestamp that would have been
               written, so both sides serialize the same line.
  rotation  -- the filesystem. `_rotate_if_needed` is handed a fake path object
               whose `stat()` reports the fixture size (or raises OSError) and
               whose `exists()` answers from the fixture, plus a fake `os` module
               on the live module whose `replace` records the rename order.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "dashboard_auth" / "audit.py"
GATE = "w22_audit_b943f2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("dashboard_audit_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["dashboard_audit_live"] = live
_spec.loader.exec_module(live)

WANTED = {"bounded", "event_value", "is_redacted", "line", "rotation"}


class FakePath:
    """The stat/exists seam for _rotate_if_needed."""

    def __init__(self, name, size, existing):
        self.name = name
        self._size = size
        self._existing = set(existing or [])

    def stat(self):
        if self._size is None:
            raise OSError("vanished")
        return types.SimpleNamespace(st_size=int(self._size))

    def with_name(self, other):
        return FakePath(other, self._size, self._existing)

    def exists(self):
        suffix = self.name.rsplit(".", 1)[-1]
        return suffix.isdigit() and int(suffix) in self._existing


def rotation(name, size, pending, existing):
    ops = []
    fake_os = types.SimpleNamespace(
        replace=lambda src, dst: ops.append("%s -> %s" % (src.name, dst.name)))
    saved = live.os
    live.os = fake_os
    try:
        live._rotate_if_needed(FakePath(name, size, existing), int(pending or 0))
    finally:
        live.os = saved
    return ops


def line(ts, event, fields):
    member = getattr(live.AuditEvent, event, None)
    if member is None:
        return None
    if not isinstance(fields, dict):
        fields = {}
    entry = {"ts": ts if ts is not None else "", "event": member.value,
             **{k: live._bounded_value(v) for k, v in fields.items()
                if k not in live._REDACTED_FIELDS}}
    return json.dumps(entry, separators=(",", ":"))


def expected(r):
    f = r.get("func")
    if f == "bounded":
        return live._bounded_value(r.get("value"))
    if f == "event_value":
        member = getattr(live.AuditEvent, r.get("name") or "", None)
        return member.value if member is not None else None
    if f == "is_redacted":
        return 1 if (r.get("key") in live._REDACTED_FIELDS) else 0
    if f == "line":
        return line(r.get("ts"), r.get("event"), r.get("fields"))
    if f == "rotation":
        return rotation(r.get("name"), r.get("size"), r.get("pending"), r.get("existing"))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line_ in sys.stdin:
        line_ = line_.strip()
        if not line_:
            continue
        try:
            r = json.loads(line_)
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
