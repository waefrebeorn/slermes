#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/tools.py — gate w32_tools_f39c80_p1.

The terminal-backend decision surface plus the toolset model catalog.  The
live module is imported BY FILE PATH (its fastapi/pydantic imports are seams)
and the routes are driven for real with their I/O seams patched from the
fixture, so what is compared is the live code's own decision:

  * ``_terminal_cfg_value`` / ``_probe_ssh_backend`` / ``_probe_modal_backend``
    / ``_probe_daytona_backend`` with ``_env_value`` patched to the fixture map
  * ``_probe_docker_backend`` with find_docker()/docker_runtime_name()/
    docker_runtime_start_hint()/run_capture() stubbed
  * ``get_terminal_backends`` (row build + active normalisation) with the
    backend rows and _probe_terminal_backend patched
  * ``select_terminal_backend`` (PUT validation) with the write seams closed
  * ``get_toolset_models`` / ``select_toolset_model`` with row resolution and
    the plugin catalogs patched
  * ``_no_models``, ``_resolve_toolset_model_plugin``, ``_find_toolset_provider_row``,
    ``_require_known_toolset`` and the two dispatch maps

Envelope note: a raising helper is reported as
``{"ok": false, "status": ..., "detail": ...}`` -- what its HTTPException is.
"""
import asyncio
import contextlib
import copy
import importlib.util
import json
import os
import subprocess
import sys
import types
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_tools_f39c80_p1"
TRUTH = QUARRY / "hermes_cli" / "web_routers" / "tools.py"

os.environ["USERPROFILE"] = os.environ.get("USERPROFILE") or str(REPO)
os.environ["HOME"] = str(REPO)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# This host blocks pydantic's native extension and the router imports fastapi
# at module scope: those are seams, not logic under test.  APPEND so the
# quarry's own hermes_cli package is never shadowed by tests/hermes_cli.
sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location("w32p1_%s" % GATE, TRUTH)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

import hermes_cli.tools_config as tc  # noqa: E402

HTTPException = live.HTTPException

WANTED = {
    "cfg_value", "probe_ssh", "probe_modal", "probe_daytona", "probe_docker",
    "backends_payload", "select_backend", "no_models", "model_plugin",
    "catalog_section", "models_payload", "model_select", "provider_row",
    "require_known", "constants",
}

_MISSING = object()


class Patcher:
    """Patch module attributes for one case, then put them all back."""

    def __init__(self):
        self.saved = []

    def set(self, obj, attr, value):
        self.saved.append((obj, attr, getattr(obj, attr, _MISSING)))
        setattr(obj, attr, value)
        return self

    def close_scopes(self, config):
        """Every write/read scope a route opens, collapsed to a no-op."""
        self.set(live, "_profile_scope", lambda profile=None: contextlib.nullcontext())
        self.set(live, "config_write_scope", lambda profile=None: contextlib.nullcontext())
        self.set(live, "load_config", lambda: copy.deepcopy(config))
        self.set(live, "save_config", lambda cfg: None)
        return self

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        for obj, attr, old in reversed(self.saved):
            if old is _MISSING:
                delattr(obj, attr)
            else:
                setattr(obj, attr, old)
        return False


def fake_module(name, **attrs):
    mod = types.ModuleType(name)
    for key, value in attrs.items():
        setattr(mod, key, value)
    sys.modules[name] = mod
    return mod


async def _direct(fn, *a, **k):
    return fn(*a, **k)


def _dict(v):
    return v if isinstance(v, dict) else {}


def _list(v):
    return v if isinstance(v, list) else []


def envelope(fn, *a, **k):
    """Call a route, reporting its HTTPException as the 400 envelope."""
    try:
        return fn(*a, **k)
    except HTTPException as exc:
        return {"ok": False, "status": exc.status_code, "detail": exc.detail}


def _pair(result):
    status, detail = result
    return {"status": status, "detail": detail}


# ── per-function cases ──────────────────────────────────────────────────────

def cfg_value(req):
    env = _dict(req.get("env_map"))
    with Patcher() as p:
        p.set(live, "_env_value", lambda name: env.get(name, ""))
        return live._terminal_cfg_value(_dict(req.get("terminal_cfg")),
                                        req.get("cfg_key"), req.get("env_var"))


def probe_ssh(req):
    env = _dict(req.get("env_map"))
    with Patcher() as p:
        p.set(live, "_env_value", lambda name: env.get(name, ""))
        return _pair(live._probe_ssh_backend(_dict(req.get("terminal_cfg"))))


def probe_modal(req):
    env = _dict(req.get("env_map"))

    def _direct_creds():
        if req.get("direct_raises"):
            raise RuntimeError("credential store unavailable")
        return bool(req.get("has_direct"))

    with Patcher() as p:
        p.set(live, "_env_value", lambda name: env.get(name, ""))
        fake_module("tools.tool_backend_helpers",
                    has_direct_modal_credentials=_direct_creds,
                    NOUS_MANAGED_PROVIDER="nous")
        return _pair(live._probe_modal_backend(None))


def probe_daytona(req):
    env = _dict(req.get("env_map"))
    with Patcher() as p:
        p.set(live, "_env_value", lambda name: env.get(name, ""))
        return _pair(live._probe_daytona_backend(None))


def probe_docker(req):
    state = _dict(req.get("state"))
    found = bool(state.get("found"))
    runtime = state.get("runtime", "")
    hint = state.get("hint", "")
    outcome = state.get("outcome", "ok")
    returncode = state.get("returncode", 0)

    def _run_capture(argv, timeout=None):
        if outcome == "timeout":
            raise subprocess.TimeoutExpired(argv, timeout or 2)
        if outcome == "error":
            raise RuntimeError(state.get("error", ""))
        return SimpleNamespace(returncode=returncode)

    fake_module("tools.environments.docker",
                find_docker=lambda: ("/usr/bin/docker" if found else None),
                docker_runtime_name=lambda exe: runtime,
                docker_runtime_start_hint=lambda exe: hint)
    fake_module("tools.environments.remote_common", run_capture=_run_capture)
    return _pair(live._probe_docker_backend(None))


def backends_payload(req):
    rows = _list(req.get("rows"))
    probes = _dict(req.get("probes"))

    def _probe(name, terminal_cfg):
        hit = probes.get(name)
        if hit is None:
            return ("unavailable", "Unknown backend: %s" % name)
        return (hit.get("status"), hit.get("detail"))

    with Patcher() as p:
        p.close_scopes(_dict(req.get("config")))
        p.set(live, "run_in_threadpool", _direct)
        p.set(live, "_terminal_backend_rows", lambda: rows)
        p.set(live, "_probe_terminal_backend", _probe)
        return asyncio.run(live.get_terminal_backends(profile=None))


def select_backend(req):
    rows = _list(req.get("rows"))
    with Patcher() as p:
        p.close_scopes(_dict(req.get("config")))
        p.set(live, "_terminal_backend_rows", lambda: rows)
        body = SimpleNamespace(backend=req.get("backend"), profile=None)
        return envelope(lambda: asyncio.run(live.select_terminal_backend(body, profile=None)))


def no_models(req):
    return live._no_models(req.get("name"))


def model_plugin(req):
    return live._resolve_toolset_model_plugin(req.get("ts_key"), _dict(req.get("row")))


def catalog_section(req):
    return dict(live._MODEL_CATALOG_TOOLSETS).get(req.get("name"))


def models_payload(req):
    row = req.get("row")
    catalog = _dict(req.get("catalog"))
    default_model = req.get("default_model")

    with Patcher() as p:
        p.close_scopes(_dict(req.get("config")))
        p.set(live, "run_in_threadpool", _direct)
        p.set(live, "_find_toolset_provider_row",
              lambda name, config, provider: copy.deepcopy(row))
        p.set(live, "_toolset_model_catalog",
              lambda name, plugin, config: (copy.deepcopy(catalog), default_model))
        return asyncio.run(live.get_toolset_models(req.get("name"), provider=None, profile=None))


def model_select(req):
    row = req.get("row")
    catalog = _dict(req.get("catalog"))

    with Patcher() as p:
        p.close_scopes(_dict(req.get("config")))
        p.set(live, "_find_toolset_provider_row",
              lambda name, config, provider: copy.deepcopy(row))
        p.set(live, "_toolset_model_catalog",
              lambda name, plugin, config: (copy.deepcopy(catalog), None))
        body = SimpleNamespace(model=req.get("model"), provider=None, profile=None)
        return envelope(lambda: asyncio.run(live.select_toolset_model(body=body,
                                                                      name=req.get("name"),
                                                                      profile=None)))


def provider_row(req):
    rows = _list(req.get("rows"))
    flags = _dict(req.get("active_flags"))
    with Patcher() as p:
        p.set(live, "_category_providers", lambda name, config: copy.deepcopy(rows))
        p.set(tc, "_is_provider_active",
              lambda prov, config, force_fresh=True: bool(flags.get(prov.get("name"), False)))
        return live._find_toolset_provider_row(req.get("ts_key", "image_gen"), {}, req.get("provider"))


def require_known(req):
    known = _list(req.get("known"))
    with Patcher() as p:
        p.set(tc, "_get_effective_configurable_toolsets",
              lambda: [(n, n, n) for n in known])
        return envelope(live._require_known_toolset, req.get("name")) or {"ok": True}


def constants(_req):
    return {
        "model_catalog_toolsets": dict(live._MODEL_CATALOG_TOOLSETS),
        "backend_probes": list(live._BACKEND_PROBES),
    }


HANDLERS = {
    "cfg_value": cfg_value,
    "probe_ssh": probe_ssh,
    "probe_modal": probe_modal,
    "probe_daytona": probe_daytona,
    "probe_docker": probe_docker,
    "backends_payload": backends_payload,
    "select_backend": select_backend,
    "no_models": no_models,
    "model_plugin": model_plugin,
    "catalog_section": catalog_section,
    "models_payload": models_payload,
    "model_select": model_select,
    "provider_row": provider_row,
    "require_known": require_known,
    "constants": constants,
}


def expected(req):
    handler = HANDLERS.get(req.get("func"))
    if handler is None:
        raise AssertionError("unknown func %r" % (req.get("func"),))
    return handler(req)


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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
