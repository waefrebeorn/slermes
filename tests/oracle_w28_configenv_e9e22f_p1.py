#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/config_env.py — gate w28_configenv_e9e22f_p1.

The env-var / Keys-tab surface of the dashboard config router.  The live
module is imported BY FILE PATH (its package __init__ and its fastapi/pydantic
imports are seams) and only its pure value functions are called:

  * ``redacted_credential_preview`` / ``is_redacted_credential_preview``
  * ``_get_env_vars_sync`` — with load_env, the profile scope, the channel-key
    set, the catalog metadata map and OPTIONAL_ENV_VARS patched per case, so
    the .env file and the provider catalog are never read from the host
  * ``_env_write_errors`` over a raised exception (ValueError / HTTPException /
    anything else)
  * ``_custom_endpoint_id``, ``_models_from_custom_endpoint_entry``,
    ``_api_key_display``, ``_endpoint_api_mode``, ``_model_names_provider``

Envelope note: the raising helpers are reported as
``{"ok": false, "status": ..., "detail": ...}`` (what an HTTPException becomes).
"""
import contextlib
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_configenv_e9e22f_p1"
TRUTH = QUARRY / "hermes_cli" / "web_routers" / "config_env.py"

# hermes_constants resolves Path.home() at import time; give it the native
# profile directory instead of the harness's MSYS $HOME.
os.environ["USERPROFILE"] = os.environ.get("USERPROFILE") or str(REPO.parent)
os.environ["HOME"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# This host blocks pydantic's native extension and the router imports fastapi
# at module scope: those are seams, not logic under test.  APPEND so the
# quarry's own hermes_cli package is never shadowed by tests/hermes_cli.
sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location("ce_p1_%s" % GATE, TRUTH)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

HTTPException = live.HTTPException

WANTED = {
    "env_preview", "is_redacted_preview", "env_vars", "env_error",
    "custom_endpoint_id", "models_from_entry", "api_key_display",
    "endpoint_api_mode", "model_names_provider",
}


def _dict(v):
    return v if isinstance(v, dict) else {}


def _list(v):
    return v if isinstance(v, list) else []


def env_vars(req):
    """_get_env_vars_sync with every I/O seam patched from the fixture."""
    live._profile_scope = lambda profile=None: contextlib.nullcontext()
    live.load_env = lambda: dict(_dict(req.get("env_on_disk")))
    keys = _list(req.get("channel_keys"))
    live._channel_managed_env_keys = lambda: set(keys)
    live._catalog_provider_env_metadata = lambda: dict(_dict(req.get("catalog_meta")))
    live.OPTIONAL_ENV_VARS = dict(_dict(req.get("optional_env_vars")))
    return live._get_env_vars_sync(None)


def env_error(req):
    kind = req.get("kind")
    try:
        with live._env_write_errors("test"):
            if kind == "value_error":
                raise ValueError(req.get("message"))
            if kind == "http":
                raise HTTPException(status_code=req.get("status"), detail=req.get("detail"))
            raise RuntimeError("unexpected")
        return {"ok": True, "value": "no-raise"}
    except HTTPException as exc:
        return {"ok": False, "status": exc.status_code, "detail": exc.detail}


def expected(req):
    func = req.get("func")
    if func == "env_preview":
        return live.redacted_credential_preview(req.get("value"))
    if func == "is_redacted_preview":
        return live.is_redacted_credential_preview(req.get("submitted"))
    if func == "env_vars":
        return env_vars(req)
    if func == "env_error":
        return env_error(req)
    if func == "custom_endpoint_id":
        return live._custom_endpoint_id(req.get("raw"), req.get("fallback", "custom"))
    if func == "models_from_entry":
        return live._models_from_custom_endpoint_entry(_dict(req.get("entry")))
    if func == "api_key_display":
        return list(live._api_key_display(_dict(req.get("entry"))))
    if func == "endpoint_api_mode":
        return live._endpoint_api_mode(_dict(req.get("entry")))
    if func == "model_names_provider":
        entry = req.get("entry")
        return live._model_names_provider(_dict(req.get("model_cfg")),
                                          req.get("provider_key"),
                                          entry if isinstance(entry, dict) else None)
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
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
