#!/usr/bin/env python3
"""Differential oracle for hermes_cli/backup_sqlite.py (gate w10_backupsqli_6ad966).

The live _uses_wal opens a file; a BytesIO-backed `open` seam feeds it the header
bytes supplied by the fixture. Rate arithmetic uses the live _CopyRate class.
"""
import importlib.util
import io
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/backup_sqlite.py"
sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

spec = importlib.util.spec_from_file_location("w10_backupsqli_6ad966_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_backupsqli_6ad966_live"] = live
spec.loader.exec_module(live)


class _BytesCM:
    def __init__(self, blob):
        self.blob = blob

    def __enter__(self):
        return io.BytesIO(self.blob)

    def __exit__(self, *a):
        return False


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "uses_wal":
        blob = bytes(int(x) & 0xFF for x in (r.get("head") or []))[:19]
        old_open = getattr(live, "open", None)
        live.open = lambda path, mode: _BytesCM(blob)
        try:
            return live._uses_wal(r.get("path") or "state.db")
        finally:
            if old_open is None:
                del live.open
            else:
                live.open = old_open
    if func in ("pages_per_second", "rollback_hold"):
        rate = live._CopyRate()
        rate.pages = r.get("pages") or 0
        rate.seconds = r.get("seconds") or 0
        rate.total = r.get("total") or 0
        pps = rate.pages_per_second()
        if func == "pages_per_second":
            return pps
        return rate.total / max(pps, 1.0)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except json.JSONDecodeError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w10_backupsqli_6ad966: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH w10_backupsqli_6ad966 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w10_backupsqli_6ad966: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
