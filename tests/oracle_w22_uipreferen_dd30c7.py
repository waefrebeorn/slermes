#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/ui_preferences.py
(gate w22_uipreferen_dd30c7).

The module is thin HTTP over `hermes_cli.ui_preferences`. The store is the seam
and is stubbed (its document, its busy lock and its write are values the
fixture supplies); everything else runs live:

  model   -- the live pydantic `PreferencesPatch` model from the quarry module.
  routes  -- the live `router.routes` the decorators registered.
  get     -- the live `get_ui_preferences()` handler over the stub store.
  patch   -- the live `patch_ui_preferences()` handler over the stub store; the
             stub's `write_preferences` echoes the call it received, so the
             compared body is exactly what the handler passed down.
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
SRC = QUARRY / "hermes_cli" / "web_routers" / "ui_preferences.py"
GATE = "w22_uipreferen_dd30c7"

WANTED = {"model", "routes", "get", "patch"}

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("ui_preferences_router_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["ui_preferences_router_live"] = live
_spec.loader.exec_module(live)

import fastapi  # noqa: E402

Patch = live.PreferencesPatch
REAL = live.ui_preferences  # the real store module: its exception types are used


def stub_store(kind, doc, msg):
    """`ui_preferences` with the store calls replaced; the lock exceptions stay real."""
    store = types.SimpleNamespace(PreferencesError=REAL.PreferencesError,
                                  PreferencesBusy=REAL.PreferencesBusy)

    def read_preferences():
        if kind == "busy":
            raise REAL.PreferencesBusy(msg)
        if kind == "error":
            raise REAL.PreferencesError(msg)
        return doc

    def write_preferences(values, seed=False):
        if kind == "busy":
            raise REAL.PreferencesBusy(msg)
        if kind == "error":
            raise REAL.PreferencesError(msg)
        return {"wrote_values": values, "wrote_seed": seed}

    store.read_preferences = read_preferences
    store.write_preferences = write_preferences
    return store


def model(body):
    try:
        m = Patch(**body)
    except Exception:  # noqa: BLE001 - pydantic ValidationError (and the TypeError of a non-object body)
        return {"valid": False, "values": None, "seed": None}
    return {"valid": True, "values": dict(m.values), "seed": m.seed}


def get(doc, kind, msg):
    live.ui_preferences = stub_store(kind, doc, msg)
    try:
        return {"status": 200, "body": live.get_ui_preferences(), "detail": None}
    except fastapi.HTTPException as exc:
        return {"status": exc.status_code, "body": None, "detail": exc.detail}
    except Exception:  # noqa: BLE001 - uncaught: FastAPI's default 500
        return {"status": 500, "body": None, "detail": None}


def patch(body, kind, msg):
    live.ui_preferences = stub_store(kind, None, msg)
    try:
        m = Patch(**body) if isinstance(body, dict) else Patch(**body)
    except Exception:  # noqa: BLE001 - request validation fails before the handler
        return {"status": 422, "body": None, "detail": None}
    try:
        return {"status": 200, "body": live.patch_ui_preferences(m), "detail": None}
    except fastapi.HTTPException as exc:
        return {"status": exc.status_code, "body": None, "detail": exc.detail}


def expected(r):
    f = r.get("func")
    if f == "model":
        return model(r.get("body"))
    if f == "routes":
        return [{"method": m, "path": route.path}
                for route in live.router.routes for m in sorted(route.methods)]
    if f == "get":
        return get(r.get("doc"), r.get("error") or "", r.get("error_msg") or "")
    if f == "patch":
        return patch(r.get("body"), r.get("error") or "", r.get("error_msg") or "")
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
