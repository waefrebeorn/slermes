#!/usr/bin/env python3
"""Differential oracle for upstream Hermes local_migration.py."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# Stub the only external dependency before executing the live module.
class AuthorityRefusal(Exception):
    pass

_pkg = types.ModuleType("persistence"); _pkg.__path__ = []
_core = types.ModuleType("persistence.core"); _core.__path__ = []
_err = types.ModuleType("persistence.core.errors")
_err.AuthorityRefusal = AuthorityRefusal
sys.modules["persistence"] = _pkg
sys.modules["persistence.core"] = _core
sys.modules["persistence.core.errors"] = _err

SRC = "C:/Users/eman5/hermes-agent/persistence/sqlite/conversation/_store/local_migration.py"
spec = importlib.util.spec_from_file_location("locmig_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

class FakeConn:
    def __init__(self, stored):
        self.stored = stored

    def execute(self, sql, params):
        stored = self.stored

        class Result:
            def fetchone(self):
                if stored is None:
                    return None
                return (json.dumps(stored),)

        return Result()

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "legacy_lineage_root":
        continue
    receipt = {
        "session_id": r.get("session_id"),
        "profile_id": r.get("profile_id"),
        "principal_id": r.get("principal_id"),
    }
    if "legacy_session_id" in r:
        receipt["legacy_session_id"] = r["legacy_session_id"]
    try:
        py = live.legacy_lineage_root(FakeConn(r.get("stored")), receipt)
        py_refused = False
    except AuthorityRefusal:
        py = None
        py_refused = True
    got = r.get("out")
    got_refused = bool(r.get("refused"))
    cases += 1
    if py != got or py_refused != got_refused:
        mism += 1
        print(f"MISMATCH in={r} py={py!r}/{py_refused} c={got!r}/{got_refused}",
              file=sys.stderr)
print(f"ORACLE locmig: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
