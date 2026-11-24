#!/usr/bin/env python3
"""Differential oracle for plugins/teams_pipeline/runtime.py (gate w22_runtime_49223c).

Three live entry points are exercised:

  delivery -- `_teams_delivery_is_configured` run live over the fixture dicts.
              Non-dict inputs read as {} on both sides (the C side cannot see a
              `.get` on a string either), so a wrong-typed block answers False
              instead of raising.
  config   -- `build_pipeline_runtime_config` over a stub GatewayConfig whose
              `platforms` maps the real `Platform("teams")` to a stub
              PlatformConfig (`.enabled` / `.extra`).  TeamsMeetingPipeline,
              the graph client and the store are not built here.
  bind     -- `bind_gateway_runtime` run live against a stub gateway and adapter;
              `build_pipeline_runtime` is the seam (stubbed to succeed or raise),
              because it constructs the live pipeline/store/graph client.  The
              ladder itself -- adapter lookup, already-bound short circuit, the
              drop-scheduler fallback -- is the real code.

Results are compared as plain JSON: the pipeline config dict, the boolean, and
the bind outcome (result, scheduler name, recorded error, runtime installation).
"""
import importlib.util
import json
import logging
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "teams_pipeline" / "runtime.py"
GATE = "w22_runtime_49223c"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("teams_runtime_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["teams_runtime_live"] = live
_spec.loader.exec_module(live)

WANTED = {"delivery", "config", "bind"}
TEAMS = live.Platform("teams")
MSGRAPH = live.Platform.MSGRAPH_WEBHOOK


def as_dict(value):
    return value if isinstance(value, dict) else {}


def delivery(extra, configured):
    return live._teams_delivery_is_configured(as_dict(extra), as_dict(configured))


def stub_gateway(platforms_json):
    platforms = {}
    if isinstance(platforms_json, dict):
        entry = platforms_json.get("teams")
        if entry is not None:
            entry = entry if isinstance(entry, dict) else {}
            extra = entry.get("extra")
            platforms[TEAMS] = types.SimpleNamespace(
                enabled=entry.get("enabled", False),
                extra=extra if isinstance(extra, dict) else {},
            )
    return types.SimpleNamespace(platforms=platforms)


def config(platforms_json):
    return live.build_pipeline_runtime_config(stub_gateway(platforms_json))


def bind(req):
    req = req if isinstance(req, dict) else {}
    events = {"scheduler": None}

    class Adapter:
        def set_notification_scheduler(self, scheduler):
            events["scheduler"] = getattr(scheduler, "__name__", "callable")

    adapters = {MSGRAPH: Adapter()} if req.get("adapter") else {}
    gateway = types.SimpleNamespace(adapters=adapters)
    if req.get("runtime"):
        gateway._teams_pipeline_runtime = object()

    message = req.get("build_error")
    original = live.build_pipeline_runtime
    if message is None:
        live.build_pipeline_runtime = lambda _gateway: object()
    else:
        def boom(_gateway, _message=message):
            raise RuntimeError(_message)
        live.build_pipeline_runtime = boom

    logger = logging.getLogger("plugins.teams_pipeline.runtime")
    old_level, old_prop = logger.level, logger.propagate
    logger.setLevel(logging.CRITICAL)
    logger.propagate = False
    try:
        result = live.bind_gateway_runtime(gateway)
    finally:
        live.build_pipeline_runtime = original
        logger.setLevel(old_level)
        logger.propagate = old_prop

    runtime = getattr(gateway, "_teams_pipeline_runtime", None)
    return {
        "result": bool(result),
        "scheduler": events["scheduler"],
        "error": getattr(gateway, "_teams_pipeline_runtime_error", None),
        "runtime_installed": runtime is not None,
    }


def expected(r):
    f = r.get("func")
    if f == "delivery":
        return delivery(r.get("teams_extra"), r.get("teams_delivery"))
    if f == "config":
        return config(r.get("platforms"))
    if f == "bind":
        return bind(r)
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
