#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_session_store.py (gate w31_gatewayses_1bd734_p2).

The live module is imported by file path; the read-shaping decisions are called
on a ``SurfaceLabelReader`` / ``GatewaySessionStore`` made with ``__new__``:

  * ``SurfaceLabelReader._expand`` -- the (live, created_source) labels a source
    filter grows into, or None for a raw filter;
  * ``SurfaceLabelReader._labelled`` -- the argument set a listing call is then
    made with;
  * ``GatewaySessionStore.banner_inventory`` -- the banner fields a snapshot
    yields.

Seam: ``records_created_surface()`` (the schema probe for the ``created_source``
column) is answered from the fixture's ``has_column``; nothing opens SQLite.  The
truth's ``get_toolset_for_tool`` is the bound ``toolset_of.get``, so the oracle
reads that mapping back off the bound method's ``__self__``.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_gatewayses_1bd734_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


def _load_live():
    import importlib.util

    path = QUARRY / "hermes_cli" / "gateway_session_store.py"
    spec = importlib.util.spec_from_file_location("gwss_live_p2", path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


try:
    live = _load_live()
except Exception:  # noqa: BLE001 -- a blocked native extension in the chain
    sys.path.append(str(Path(__file__).resolve().parent))
    try:
        from hermes_seam_stubs import install as _install_seams

        _install_seams()
    except Exception:  # noqa: BLE001
        pass
    live = _load_live()

WANTED = {"expand", "labelled", "banner_inventory"}


def reader(r):
    obj = live.SurfaceLabelReader.__new__(live.SurfaceLabelReader)
    has_column = bool(r.get("has_column"))
    obj._has_column = False
    obj.persistence = types.SimpleNamespace(
        sessions=types.SimpleNamespace(records_created_surface=lambda: has_column))
    return obj


def expand(r):
    got = reader(r)._expand(r.get("requested"))
    if got is None:
        return None
    return {"live": list(got[0]), "created": list(got[1])}


def labelled(r):
    def call(src, lim, off, surfaces):
        return {"sources": list(src) if isinstance(src, (list, tuple)) else src,
                "limit": lim, "offset": off,
                "surfaces": None if surfaces is None else list(surfaces)}

    return reader(r)._labelled(r.get("requested"), r.get("limit"), r.get("offset"), call)


def banner_inventory(r):
    store = live.GatewaySessionStore.__new__(live.GatewaySessionStore)
    store.current = r.get("current")
    out = store.banner_inventory()
    if out is None:
        return None
    mapping = dict(out["get_toolset_for_tool"].__self__)
    shaped = {k: v for k, v in out.items() if k != "get_toolset_for_tool"}
    shaped["toolset_of"] = mapping
    return shaped


def expected(r):
    f = r.get("func")
    if f == "expand":
        return expand(r)
    if f == "labelled":
        return labelled(r)
    if f == "banner_inventory":
        return banner_inventory(r)
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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or cases == 0) else 0)


if __name__ == "__main__":
    main()
