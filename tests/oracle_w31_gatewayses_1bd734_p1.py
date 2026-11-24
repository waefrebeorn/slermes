#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_session_store.py (gate w31_gatewayses_1bd734_p1).

The live module is imported by file path; the pure decisions are then called on
objects made with ``__new__`` so no gateway connection, no store handle and no
reader ever start:

  * ``session_label`` -- the banner label of a canonical durable id;
  * ``_window`` + ``SessionWindow.__post_init__`` -- a listing window;
  * ``_transcript_view`` -- the view a read's flags select;
  * ``GatewaySessionStore.__bool__`` -- truthy until the connection is final;
  * ``GatewaySessionStore.__getattr__`` -- which family a missing name is in;
  * ``GatewaySessionStore._retry_state`` -- what an owner-restart wait froze.

Seam: a name in ``_READS`` that is neither ``sanitize_title`` nor
``session_yolo_enabled`` would open the read-only reader inside ``__getattr__``,
so those are classified from the live ``_READS`` set instead of by calling it.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_gatewayses_1bd734_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


def _load_live():
    import importlib.util

    path = QUARRY / "hermes_cli" / "gateway_session_store.py"
    spec = importlib.util.spec_from_file_location("gwss_live_p1", path)
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

WANTED = {"session_label", "window", "transcript_view", "store_truthy", "attr_dispatch", "retry_state"}


def bare(final=None):
    store = live.GatewaySessionStore.__new__(live.GatewaySessionStore)
    store.connection = types.SimpleNamespace(final=final)
    return store


def session_label(r):
    return live.session_label(r.get("session_id"))


def window(r):
    try:
        w = live._window(r.get("limit"), r.get("offset"))
    except ValueError as exc:
        return {"error": str(exc)}
    return {"limit": w.limit, "offset": w.offset}


def transcript_view(r):
    return live._transcript_view(r.get("include_inactive"), r.get("include_compacted")).name


def store_truthy(r):
    return bool(bare(final=r.get("final")))


def attr_dispatch(r):
    name = r.get("name")
    store = bare(final=False)
    if name.startswith("__"):
        try:
            getattr(store, name)
        except AttributeError:
            return "attribute_error"
        raise AssertionError("dunder served: %r" % (name,))
    if name in live._READS:
        if name == "sanitize_title":
            return "sanitize_title"
        if name == "session_yolo_enabled":
            return "session_yolo_enabled"
        return "read"
    value = getattr(store, name)
    if value is live._noop:
        return "noop"
    if value is live._zero:
        return "zero"
    try:
        value()
    except Exception:  # noqa: BLE001 -- the refusal closure
        return "refused"
    raise AssertionError("served without refusing: %r" % (name,))


def retry_state(r):
    raw = r.get("retry")
    store = bare(final=False)
    if raw is None:
        store.retry = None
    else:
        prepared = raw[0] if isinstance(raw, list) and raw else None
        store.retry = (prepared, (lambda: None) if (len(raw) > 1 and raw[1]) else None)
    prepared, abandon = store._retry_state()
    return {"prepared": prepared, "abandon": abandon is not None}


def expected(r):
    f = r.get("func")
    if f == "session_label":
        return session_label(r)
    if f == "window":
        return window(r)
    if f == "transcript_view":
        return transcript_view(r)
    if f == "store_truthy":
        return store_truthy(r)
    if f == "attr_dispatch":
        return attr_dispatch(r)
    if f == "retry_state":
        return retry_state(r)
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
