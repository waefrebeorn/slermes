#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_session_model_guard.py (gate
w21_methodsses_5b7963).

The live module is imported and both entry points run for real:
`model_override_conflict` (patched only at the two boundaries it crosses) and
`create_overrides`, which itself calls the live `parse_reasoning_effort`,
`parse_exact_service_tier` and `is_truthy_value`.

Seams, named:
* `hermes_cli.models_validate.static_model_provider_conflict` -> a recorder that
  returns the fixture's `conflict`. That function (curated catalogs + difflib)
  is a different module; what this module owns is the pair it is asked about and
  what it returns, and both are recorded.
* `hermes_cli.runtime_provider.resolve_requested_provider()` -> the fixture's
  `scope_provider` (profile config + env read).
* `_profile_build_scope(...)` -> a context manager that only records entry, so
  the "provider came from the scope" branch is observable.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tui_gateway" / "methods_session_model_guard.py"
GATE = "w21_methodsses_5b7963"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("methods_session_model_guard_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["methods_session_model_guard_live"] = live
_spec.loader.exec_module(live)

import hermes_cli.models_validate as models_validate  # noqa: E402
import hermes_cli.runtime_provider as runtime_provider  # noqa: E402
from agent.fast_mode import parse_exact_service_tier  # noqa: E402
from hermes_constants import parse_reasoning_effort  # noqa: E402
from utils import is_truthy_value  # noqa: E402


def do_model_override(r):
    params = r.get("params")
    if not isinstance(params, dict):
        params = {}
    calls = []
    entered = {"n": 0}

    def fake_conflict(model, provider):
        calls.append((model, provider))
        return r.get("conflict")

    class Scope:
        def __enter__(self):
            entered["n"] += 1
            return None

        def __exit__(self, *exc):
            return False

    models_validate.static_model_provider_conflict = fake_conflict
    runtime_provider.resolve_requested_provider = lambda: r.get("scope_provider", "")
    result = live.model_override_conflict(params, Scope())
    if not calls:
        return {"model": "", "checked": False}
    model, provider = calls[0]
    return {"model": model, "provider": provider, "used_scope": entered["n"] > 0,
            "checked": True, "result": result}


def do_create_overrides(r):
    params = r.get("params")
    if not isinstance(params, dict):
        params = {}
    try:
        model_override, reasoning_override, service_tier_override = live.create_overrides(params)
    except Exception as exc:  # noqa: BLE001 - the port reports raised errors this way
        return {"status": "error", "type": type(exc).__name__, "message": str(exc)}
    return [model_override, reasoning_override, service_tier_override]


def do_service_tier(r):
    try:
        return {"value": parse_exact_service_tier(r.get("raw"))}
    except ValueError as exc:
        return {"raised": "ValueError", "message": str(exc)}


def expected(r):
    f = r.get("func")
    if f == "str":
        return str(r.get("value") or "").strip()
    if f == "truthy":
        return is_truthy_value(r.get("value"))
    if f == "reasoning":
        return parse_reasoning_effort(r.get("effort"))
    if f == "service_tier":
        return do_service_tier(r)
    if f == "model_override":
        return do_model_override(r)
    if f == "create_overrides":
        return do_create_overrides(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"str", "truthy", "reasoning", "service_tier", "model_override",
              "create_overrides"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
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
