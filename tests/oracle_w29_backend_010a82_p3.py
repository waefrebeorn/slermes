#!/usr/bin/env python3
"""Differential oracle for plugins/config_backends/remote/backend.py (gate p3).

Only the write decision runs: `_intent` (the key-level edit a save means,
§15.2 steps 1-5) and `_patch_body` (its application to the current document,
with the lock / secret-literal refusals and the writerConfigVersion stamp).
Seams: the plane never answers; `DEFAULT_CONFIG` is the fixture's `latest`, the
read base and private-migration contextvars are the fixture's documents.
"""
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_backend_010a82_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.config_backends.remote import backend as live  # noqa: E402
from hermes_cli.config_backend import Changes  # noqa: E402
import hermes_cli.config_defaults as config_defaults  # noqa: E402

WANTED = {"intent", "patch_body"}


def bare():
    return live.RemoteBackend.__new__(live.RemoteBackend)


def writer_from(req):
    spec = req.get("profile_writer") or {"kind": "unknown"}
    kind = spec.get("kind")
    if kind == "null":
        return None
    if kind == "int":
        return spec.get("value")
    return live._UNKNOWN


def with_latest(latest, fn):
    old = config_defaults.DEFAULT_CONFIG
    config_defaults.DEFAULT_CONFIG = {"_config_version": latest}
    try:
        return fn()
    finally:
        config_defaults.DEFAULT_CONFIG = old


def norm(value):
    """A Python dict's key order is not part of its value; pinned is a set."""
    out = dict(value)
    if out.get("ok"):
        out["sets"] = {tuple(p): v for p, v in out.get("sets") or []}
        out["unsets"] = [tuple(p) for p in out.get("unsets") or []]
        if "pinned" in out:
            out["pinned"] = set(out["pinned"])
    return out


def error(exc):
    return {"ok": False, "error": type(exc).__name__,
            "code": getattr(exc, "code", ""), "message": str(exc)}


def intent(req):
    st = SimpleNamespace(base=req.get("st_base"),
                         server_config=req.get("server_config") or {},
                         locks=[(tuple(p), lv) for p, lv in req.get("locks") or []])
    changes = Changes(document=req.get("document"), set=req.get("set") or {},
                      unset=tuple(req.get("unset") or ()))
    ptok = rtok = ktok = ftok = None
    if req.get("private"):
        ptok = live._PRIVATE.set(SimpleNamespace(st=st, doc={}, serial=1))
    if req.get("read_base") is not None:
        rtok = live._READ_BASE.set({id(st): (st, req.get("read_base"))})
    # pinning_keys() / refused_locked_omission_prefixes() are contextvars too;
    # bind them by name here so this oracle never depends on a module alias.
    from hermes_cli.config_backend import _PINNED_KEYS, _REFUSE_LOCKED_OMISSIONS
    ktok = _PINNED_KEYS.set(tuple(req.get("pinned_keys") or ()))
    ftok = _REFUSE_LOCKED_OMISSIONS.set(tuple(req.get("refused_prefixes") or ()))
    try:
        sets, unsets, pinned = bare()._intent(st, changes)
    except Exception as exc:  # noqa: BLE001
        return error(exc)
    finally:
        _REFUSE_LOCKED_OMISSIONS.reset(ftok)
        _PINNED_KEYS.reset(ktok)
        if rtok is not None:
            live._READ_BASE.reset(rtok)
        if ptok is not None:
            live._PRIVATE.reset(ptok)
    return norm({
        "ok": True,
        "sets": [[list(p), v] for p, v in sets.items()],
        "unsets": [list(p) for p in unsets],
        "pinned": sorted(live.encode(p) for p in pinned),
    })


def patch_body(req):
    st = SimpleNamespace(base=req.get("st_base"),
                         server_config=req.get("server_config") or {},
                         profile_version=req.get("profile_version", 0),
                         profile_writer=writer_from(req),
                         locks=[(tuple(p), lv) for p, lv in req.get("locks") or []])
    sets = {tuple(p): v for p, v in req.get("sets") or []}
    unsets = [tuple(p) for p in req.get("unsets") or []]
    pinned = frozenset(tuple(p) for p in req.get("pinned") or [])
    latest = req.get("latest", 1)
    try:
        out = with_latest(latest, lambda: bare()._patch_body(st, sets, unsets, pinned))
    except Exception as exc:  # noqa: BLE001
        return error(exc)
    if out is None:
        body, got_sets, got_unsets = None, {}, []
    else:
        body, got_sets, got_unsets = out
    return norm({
        "ok": True,
        "body": body,
        "sets": [[list(p), v] for p, v in got_sets.items()],
        "unsets": [list(p) for p in got_unsets],
    })


def expected(r):
    f = r.get("func")
    if f == "intent":
        return intent(r)
    if f == "patch_body":
        return patch_body(r)
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
        if isinstance(got, dict) and r.get("func") in WANTED:
            try:
                got = norm(got)
            except Exception:  # noqa: BLE001
                pass
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if cases == 0:
        print("ORACLE %s: no cases" % GATE, file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
