#!/usr/bin/env python3
"""Differential oracle for tui_gateway/profile_roster_cache.py (gate w26_profileros_c45dc1).

The live module's only I/O seams are ``Path.stat()`` and ``time.time_ns()``. Both
are replaced with fixed inputs taken from the request: a fake ``Path`` whose
``stat()`` answers from the request's file records (raising ``FileNotFoundError``
for a file the profile does not have), and a ``mtime_is_racy`` that compares the
request's ``now_ns`` against the module's own ``RACY_MTIME_WINDOW_NS``. The code
that runs is the live ``store_signature`` / ``profile_yaml_signature`` /
``_cached`` -- only the stat and the clock are supplied.

  * ``store_signature``  over the profile's state.db/-wal stat pair;
  * ``yaml_signature``   over profile.yaml's (mtime, size, inode);
  * ``cached``           the bounded memo, given its prior state;
  * ``constants``        ``_MAX_ENTRIES`` and ``_STORE_FILES``.
"""
import copy
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_profileros_c45dc1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import profile_roster_cache as live  # noqa: E402

WANTED = {"store_signature", "yaml_signature", "cached", "constants"}


class _FakeStat:

    def __init__(self, rec):
        self.st_mtime_ns = rec.get("mtime_ns", 0)
        self.st_size = rec.get("size", 0)
        self.st_ino = rec.get("inode", 0)


class _FakePath:
    """A Path whose stat() answers from the request's file records."""

    def __init__(self, files, p=""):
        self._files = files
        self._p = str(p)

    def __truediv__(self, other):
        return _FakePath(self._files, self._p.rstrip("/") + "/" + str(other))

    @property
    def name(self):
        return self._p.rsplit("/", 1)[-1]

    def stat(self):
        rec = self._files.get(self.name)
        if rec is None:
            raise FileNotFoundError(self._p)
        return _FakeStat(rec)


def _files(raw):
    out = {}
    for entry in raw if isinstance(raw, list) else []:
        if not isinstance(entry, dict) or not isinstance(entry.get("name"), str):
            continue
        if entry.get("exists") is False:
            continue
        out[entry["name"]] = entry
    return out


def _patch(req):
    live.Path = lambda p: _FakePath(_files(req.get("files")), p)
    now = req.get("now_ns", 0)

    def _racy(mtime_ns):
        return (now - mtime_ns) < live.RACY_WINDOW

    live.mtime_is_racy = _racy


# utils/files.py's window; kept in sync with the C port without importing utils.
live.RACY_WINDOW = 2_000_000_000


def _sig_out(v):
    if v is None:
        return None
    return [list(part) for part in v]


def store_signature(req):
    _patch(req)
    return _sig_out(live.store_signature("/fake/profiles/alpha"))


def yaml_signature(req):
    _patch(req)
    v = live.profile_yaml_signature("/fake/profiles/alpha")
    return list(v) if v is not None else None


def _tup_key(v):
    if isinstance(v, list):
        return tuple(_tup_key(x) for x in v)
    return v


def _lst_key(v):
    if isinstance(v, tuple):
        return [_lst_key(x) for x in v]
    if isinstance(v, list):
        return [_lst_key(x) for x in v]
    return v


def _tup_sig(v):
    if v is None:
        return None
    return tuple(tuple(p) if isinstance(p, list) else p for p in v)


def _lst_sig(v):
    if v is None:
        return None
    return [list(p) if isinstance(p, tuple) else p for p in v]


def cached(req):
    key = _tup_key(req.get("key") or [])
    sig = _tup_sig(req.get("signature"))
    cache = {}
    for entry in req.get("cache") or []:
        if not isinstance(entry, dict) or "key" not in entry:
            continue
        cache[_tup_key(entry["key"])] = (_tup_sig(entry.get("signature")), entry.get("fields"))
    live._MAX_ENTRIES = req.get("max_entries", 512)
    copier = copy.deepcopy if req.get("copier") == "deep" else dict
    fields = req.get("fields")
    out = live._cached(cache, key, sig, lambda: fields, copier)
    return {
        "fields": out,
        "cache": [
            {"key": _lst_key(k), "signature": _lst_sig(v[0]), "fields": v[1]}
            for k, v in cache.items()
        ],
    }


def constants():
    return {
        "max_entries": live._MAX_ENTRIES,
        "store_files": list(live._STORE_FILES),
    }


def expected(r):
    f = r.get("func")
    if f == "store_signature":
        return store_signature(r)
    if f == "yaml_signature":
        return yaml_signature(r)
    if f == "cached":
        return cached(r)
    if f == "constants":
        return constants()
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
