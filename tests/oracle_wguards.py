#!/usr/bin/env python3
"""Oracle for persistence/sqlite/conversation/_store/write_guards.py.

Injects a deterministic _check_transcript_write_guards that raises the real
guard exceptions according to each case's `outcome`, so C's injected-outcome
model is compared against the live filtering/set semantics.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

ROOT = "C:/Users/eman5/hermes-agent"
SRC = ROOT + "/persistence/sqlite/conversation/_store/write_guards.py"
DOTTED = "persistence.sqlite.conversation._store.write_guards"
sys.path.insert(0, ROOT)
spec = importlib.util.spec_from_file_location(DOTTED, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[DOTTED] = live
spec.loader.exec_module(live)

from persistence.sqlite._state.errors import (
    SessionCompressionInProgressError,
    SessionTurnLeaseLostError,
)


def make_fake(outcome):
    def fake(holder, conn, sid, **kwargs):
        if outcome == 1:
            raise SessionCompressionInProgressError()
        if outcome == 2:
            raise SessionTurnLeaseLostError()
        return None
    return fake


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
    if func == "reject":
        live._check_transcript_write_guards = make_fake(int(r.get("outcome", 0)))
        py = bool(live._write_guards_reject(None, None, "s"))
    elif func == "guarded_ids":
        ids = list(r.get("ids", []))
        codes = iter([int(x) for x in r.get("outcomes", [])])

        def fake(holder, conn, sid, **kwargs):
            code = next(codes)
            if code == 1:
                raise SessionCompressionInProgressError()
            if code == 2:
                raise SessionTurnLeaseLostError()
            return None

        live._check_transcript_write_guards = fake
        py = sorted(live._guarded_ids(None, None, ids))
    else:
        continue

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE wguards: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
