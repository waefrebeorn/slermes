#!/usr/bin/env python3
"""Differential oracle for gate w14_credential_e7e645 (agent/credential_pool_admin.py).

Loads the LIVE module via importlib.spec_from_file_location and drives the
resolve_target / remove_index methods against a fake pool object that supplies
_entries, _current_id, _lock, _persist, _find and _replace_entry.
"""
import contextlib
import dataclasses
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "credential_pool_admin.py").is_file():
    QUARRY = Path("C:/Users/eman5/hermes-agent")
sys.path.insert(0, str(QUARRY))
for _pkg in ("agent", "hermes_cli"):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = []
        sys.modules[_pkg] = _m

_SRC = str(QUARRY / "agent" / "credential_pool_admin.py")
_SPEC = importlib.util.spec_from_file_location("w14_credential_live", _SRC)
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


@dataclasses.dataclass
class Ent:
    id: str = ""
    label: str = ""
    priority: int = 0


class FakePool:
    def __init__(self, entries, current_id=None):
        self._entries = [Ent(**{k: v for k, v in e.items() if k in ("id", "label", "priority")})
                         for e in (entries or [])]
        self._current_id = current_id
        self._lock = contextlib.nullcontext()
        self.persisted = []

    def _persist(self, **kw):
        self.persisted.append(kw)

    def _find(self, pred):
        return next((e for e in self._entries if pred(e)), None)

    def _replace_entry(self, old, new):
        i = self._entries.index(old)
        self._entries[i] = new


def expected(rec):
    f = rec.get("func")
    if f == "resolve_target":
        fake = FakePool(rec.get("entries"))
        idx, ent, err = mod.CredentialPoolAdminMixin.resolve_target(fake, rec.get("target"))
        return {"index": idx, "id": (ent.id if ent is not None else None), "error": err}
    if f == "remove_index":
        fake = FakePool(rec.get("entries"), rec.get("current_id"))
        removed = mod.CredentialPoolAdminMixin.remove_index(fake, int(rec.get("index") or 0))
        return {"removed": (removed.id if removed is not None else None),
                "remaining": [{"id": e.id, "priority": e.priority} for e in fake._entries],
                "current_id": fake._current_id}
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        want = expected(rec)
        got = rec.get("out")
        if got != want:
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w14_credential_e7e645: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
