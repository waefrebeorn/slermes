#!/usr/bin/env python3
"""Differential oracle for tools/process_registry_results.py
(gate w22_processreg_606e65).

Live code paths and their seams:

  constants -- live RESULT_RETENTION_SECONDS / MAX_RETAINED_RESULTS.
  select    -- live `_result_paths` with the process-results directory replaced by
               a fake listing and `time.time` frozen to the fixture "now" (clock
               and filesystem seams). Expiry, the (mtime, name) reverse sort and
               the retention cap all run live.
  owns      -- live `_owns_result` with a fake `persistence_assembly` (the lineage
               read seam).
  admissible-- live `load_completed_results` with `_result_paths` bound to one
               fake receipt and the session env / hosted scope / ProcessSession
               seams stubbed; the prefix, id and ownership filters run live.
"""
import fnmatch
import importlib.util
import json
import sys
import types
from contextlib import contextmanager, nullcontext
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tools" / "process_registry_results.py"
GATE = "w22_processreg_606e65"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("prr_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["prr_live"] = live
_spec.loader.exec_module(live)

WANTED = {"fields", "constants", "select", "owns", "admissible"}


@contextmanager
def patched(obj, name, value):
    original = getattr(obj, name)
    setattr(obj, name, value)
    try:
        yield
    finally:
        setattr(obj, name, original)


@contextmanager
def stub_module(name, module):
    saved = sys.modules.get(name)
    sys.modules[name] = module
    try:
        yield
    finally:
        if saved is None:
            sys.modules.pop(name, None)
        else:
            sys.modules[name] = saved


class FakeEntry:
    def __init__(self, row):
        self.name = row.get("name")
        self.mtime = int(row.get("mtime") or 0)
        self.removed = False

    def stat(self):
        return types.SimpleNamespace(st_mtime=self.mtime)

    def unlink(self, missing_ok=False):
        self.removed = True


class FakeDir:
    def __init__(self, entries):
        self.entries = entries

    def glob(self, pattern):
        return [e for e in self.entries if fnmatch.fnmatchcase(e.name or "", pattern)]


class FakeHome:
    """Path-like stand-in for `get_hermes_home() / "logs" / "process-results"`."""

    def __init__(self, directory):
        self.directory = directory

    def __truediv__(self, _part):
        return self

    def glob(self, pattern):
        return self.directory.glob(pattern)


def select(rows, now):
    entries = [FakeEntry(r) for r in rows if isinstance(r, dict)] if isinstance(rows, list) else []
    directory = FakeDir(entries)
    clock = types.SimpleNamespace(time=lambda: float(now or 0))
    with patched(live, "get_hermes_home", lambda: FakeHome(directory)), \
         patched(live, "time", clock):
        kept = live._result_paths()
    return {"retained": [path.name for path in kept],
            "pruned": sorted(e.name for e in entries if e.removed)}


def lineage_module(lineage):
    fake = types.ModuleType("persistence_assembly")
    fake.profile_scope = lambda: types.SimpleNamespace(profile_dir="/tmp/profile")
    fake.open_persistence_reader = lambda _dir: nullcontext(
        types.SimpleNamespace(lineage=types.SimpleNamespace(tip=lambda parent: lineage.get(parent))))
    return fake


def owns(owner, parent, lineage):
    with stub_module("persistence_assembly", lineage_module(lineage or {})):
        return 1 if live._owns_result(owner if isinstance(owner, str) else "", parent) else 0


class FakeSession:
    def __init__(self, **fields):
        self.__dict__.update(fields)
        self._completion_event = types.SimpleNamespace(set=lambda: None)


def admissible(prefix, stem, record_id, owner, parent, lineage):
    record = {name: "" for name in live._RESULT_FIELDS}
    record.update({"id": record_id, "parent_session_id": parent, "output": "",
                   "exit_code": 0})
    path = types.SimpleNamespace(stem=stem, name="%s.json" % stem,
                                 read_text=lambda encoding="utf-8": json.dumps(record))
    registry = types.ModuleType("tools.process_registry")
    registry.ProcessSession = FakeSession
    context = types.ModuleType("gateway.session_context")
    context.get_session_env = lambda _key, default="": owner if isinstance(owner, str) else default
    sandbox = types.ModuleType("tools.environments.sandbox_scope")
    sandbox.hosted_scope = lambda: None
    with patched(live, "_result_paths", lambda: [path]), \
         stub_module("persistence_assembly", lineage_module(lineage or {})), \
         stub_module("tools.process_registry", registry), \
         stub_module("gateway.session_context", context), \
         stub_module("tools.environments.sandbox_scope", sandbox):
        results = live.load_completed_results(prefix if isinstance(prefix, str) else "")
    return 1 if stem in results else 0


def expected(r):
    f = r.get("func")
    if f == "fields":
        return list(live._RESULT_FIELDS)
    if f == "constants":
        return {"retention_seconds": str(live.RESULT_RETENTION_SECONDS),
                "max_retained": str(live.MAX_RETAINED_RESULTS)}
    if f == "select":
        return select(r.get("rows"), r.get("now"))
    if f == "owns":
        return owns(r.get("owner"), r.get("parent"), r.get("lineage"))
    if f == "admissible":
        return admissible(r.get("prefix"), r.get("stem"), r.get("record_id"),
                          r.get("owner"), r.get("parent"), r.get("lineage"))
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
