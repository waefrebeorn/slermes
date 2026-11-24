#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/messaging.py — gate w28_messaging_ede80b_p3.

The Channels page.  The live module is imported BY FILE PATH (its package
__init__ drags the whole web stack); every value the endpoints would read from
disk or from a live gateway is a fixture input instead:

  * ``_messaging_env_info`` — OPTIONAL_ENV_VARS / the fallback table are swapped
    for the fixture's ``info`` row;
  * ``_count_csv`` — called straight;
  * ``_messaging_platform_payload`` — liveness, the enablement arm and the
    retained gateway verdict are stubbed, so the state ladder and the payload
    assembly run for real;
  * ``_messaging_platform_posture`` — load_env/_gate_env/_platform_dm_policy are
    stubbed and gateway.pairing.PairingStore is a count stub;
  * ``_multiplex_port_binding_conflict`` — the mirrored-platform set, the active
    profile name and the multiplex flag are stubbed;
  * ``test_messaging_platform`` — the whole endpoint, with profile scope,
    catalog, payloads and posture stubbed, so the verdict ladder runs for real.
"""
import asyncio
import contextlib
import importlib.util
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_messaging_ede80b_p3"
TRUTH = QUARRY / "hermes_cli" / "web_routers" / "messaging.py"

os.environ["USERPROFILE"] = os.environ.get("USERPROFILE") or str(REPO.parent)
os.environ["HOME"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# This host blocks pydantic's native extension and the router imports fastapi
# at module scope: those are seams, not logic under test.
sys.path.append(str(Path(__file__).resolve().parent))  # stub helper only; must NOT shadow the quarry packages
from hermes_seam_stubs import install as _install_seams  # noqa: E402
_install_seams()

_spec = importlib.util.spec_from_file_location("msg_live_%s" % GATE, TRUTH)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

HTTPException = live.HTTPException
WANTED = {"env_info", "count_csv", "platform_state", "posture",
          "multiplex_conflict", "test_verdict"}

PATCHED = ("resolve_gateway_liveness", "_platform_enablement", "retained_gateway_state",
           "_LISTENER_MIRRORS", "_GATEWAY_HEALTH_URL", "OPTIONAL_ENV_VARS",
           "_MESSAGING_ENV_FALLBACKS", "load_env", "_gate_env", "_platform_dm_policy",
           "_resolve_profile_dir", "_profile_scope", "_require_platform",
           "_platform_authz_env_names", "_platform_payloads",
           "_messaging_platform_posture")
ORIG = {name: getattr(live, name) for name in PATCHED}


def ok(value):
    return {"ok": True, "value": value}


def restore():
    for name, value in ORIG.items():
        setattr(live, name, value)


def call(fn, *args):
    try:
        return ok(fn(*args))
    except HTTPException as exc:
        return {"ok": False, "status": exc.status_code, "detail": exc.detail}


def env_info(req):
    key = req.get("key")
    info = req.get("info")
    if isinstance(info, dict) and info:
        live.OPTIONAL_ENV_VARS = {key: info}
        live._MESSAGING_ENV_FALLBACKS = {}
    else:
        live.OPTIONAL_ENV_VARS = {}
        live._MESSAGING_ENV_FALLBACKS = {}
    return call(live._messaging_env_info, key)


def platform_state(req):
    entry = req.get("entry") if isinstance(req.get("entry"), dict) else {}
    runtime = req.get("runtime") if isinstance(req.get("runtime"), dict) else {}
    gw = bool(req.get("gateway_running"))
    enabled = bool(req.get("enabled"))
    configured = bool(req.get("configured"))
    home_channel = req.get("home_channel")
    live.resolve_gateway_liveness = lambda **kw: types.SimpleNamespace(running=gw)
    live._platform_enablement = lambda *a, **k: (enabled, configured,
                                                home_channel if isinstance(home_channel, dict) else None)
    live.retained_gateway_state = lambda rt: req.get("retained_state")
    live._LISTENER_MIRRORS = frozenset(req.get("mirrors") or ())
    live._GATEWAY_HEALTH_URL = None
    payload = live._messaging_platform_payload(
        dict(entry),
        dict(req.get("env_on_disk") or {}),
        dict(runtime),
        scoped=bool(req.get("scoped")),
        profile_home=None,
        source_keys=frozenset(req.get("source_keys") or ()),
        relay_disabled=frozenset(req.get("relay_disabled") or ()),
        relay_view=(frozenset(req.get("relay_fronted") or ()),
                    bool(req.get("relay_exclusive"))),
    )
    return ok(payload)


def with_pairing_store(approved, pending):
    module = types.ModuleType("gateway.pairing")

    class PairingStore:
        def list_approved(self, platform_id):
            return [object() for _ in range(int(approved))]

        def list_pending(self, platform_id):
            return [object() for _ in range(int(pending))]

    module.PairingStore = PairingStore
    sys.modules["gateway.pairing"] = module
    return module


def posture(req):
    entry = dict(req.get("entry") or {})
    env = dict(req.get("env") or {})
    live.load_env = lambda: dict(env)
    live._gate_env = lambda name: env.get(name, "")
    live._platform_dm_policy = lambda platform_id: req.get("dm_policy") or ""
    store = with_pairing_store(req.get("approved") or 0, req.get("pending") or 0)
    try:
        return call(live._messaging_platform_posture, req.get("platform_id"), entry,
                    bool(req.get("scoped", True)),
                    (req.get("allowed_env") or "", req.get("allow_all_env") or ""))
    finally:
        sys.modules.pop("gateway.pairing", None)
        del store


def multiplex_conflict(req):
    import gateway.config as gateway_config
    from hermes_cli import gateway_multiplex_mode, profiles

    saved = (gateway_config.SHARED_LISTENER_MIRROR_PLATFORMS,
             profiles.get_active_profile_name,
             gateway_multiplex_mode.default_gateway_multiplexes)
    gateway_config.SHARED_LISTENER_MIRROR_PLATFORMS = frozenset(req.get("mirrors") or ())
    profiles.get_active_profile_name = lambda: req.get("active_profile") or "default"
    gateway_multiplex_mode.default_gateway_multiplexes = lambda: bool(req.get("multiplexes"))
    live._resolve_profile_dir = lambda name: Path("/nonexistent") / str(name)
    try:
        return call(live._multiplex_port_binding_conflict, req.get("platform_id"),
                    req.get("requested_profile"))
    finally:
        (gateway_config.SHARED_LISTENER_MIRROR_PLATFORMS,
         profiles.get_active_profile_name,
         gateway_multiplex_mode.default_gateway_multiplexes) = saved


def test_verdict(req):
    entry = dict(req.get("entry") or {})
    payload = dict(req.get("payload") or {})
    posture_value = req.get("posture")
    live._profile_scope = lambda profile: contextlib.nullcontext(None)
    live._require_platform = lambda platform_id: entry
    live._platform_authz_env_names = lambda platform_id: ("", "")
    live._platform_payloads = lambda scoped_dir, entries: [payload]
    live._messaging_platform_posture = lambda *a, **k: posture_value
    try:
        return ok(asyncio.run(live.test_messaging_platform(req.get("platform_id") or "telegram",
                                                           None)))
    except HTTPException as exc:
        return {"ok": False, "status": exc.status_code, "detail": exc.detail}
    finally:
        restore()


def expected(req):
    func = req.get("func")
    if func == "env_info":
        return env_info(req)
    if func == "count_csv":
        return call(live._count_csv, req.get("value"))
    if func == "platform_state":
        return platform_state(req)
    if func == "posture":
        return posture(req)
    if func == "multiplex_conflict":
        return multiplex_conflict(req)
    if func == "test_verdict":
        return test_verdict(req)
    raise AssertionError("unknown func %r" % (func,))


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
        finally:
            restore()
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
