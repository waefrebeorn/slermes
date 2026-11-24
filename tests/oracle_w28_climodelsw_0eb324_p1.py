#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_model_switch_mixin.py — gate
w28_climodelsw_0eb324_p1 (route/value helpers).

The live mixin module is imported BY FILE PATH and its pure helpers are called
with every store stubbed out.  Each answer a store would have produced arrives
in the request object instead, so what is compared is exactly the decision the
store's answer feeds:

  * ``_heal_bare_custom_provider`` — with ``_main_slot_serves`` (config.yaml
    model.default) and the two custom-identity lookups as seams;
  * ``_runtime_fields`` / ``_snapshot_model_runtime`` — over an attribute bag;
  * ``_configured_default_route`` — with ``load_config_readonly`` stubbed to
    the fixture's ``model`` section;
  * ``stored_session_route`` — with ``parse_model_config``/``resume_route``
    (the session row reader), ``is_foreign_provider_endpoint``,
    ``model_derived_api_mode`` and ``normalize_opencode_base_url`` as seams;
  * the ``_MODEL_USAGE_ROWS`` usage block, with the i18n catalog stubbed.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_climodelsw_0eb324_p1"
MIN_CASES = 10

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
if os.name == "nt" and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)

# stub helper only; appended so it can never shadow a quarry package
sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location(
    "live_cli_model_switch_mixin", QUARRY / "hermes_cli" / "cli_model_switch_mixin.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

import hermes_cli.config as config_mod  # noqa: E402
import hermes_cli.model_switch as switch_mod  # noqa: E402
import hermes_cli.models as models_mod  # noqa: E402
import hermes_cli.runtime_provider as rp_mod  # noqa: E402
import hermes_cli.runtime_provider_custom as rpc_mod  # noqa: E402
import persistence.conversation.sessions as sessions_mod  # noqa: E402
import persistence.gateway.routing as routing_mod  # noqa: E402

WANTED = {"heal_provider", "runtime_fields", "snapshot_runtime",
          "configured_route", "stored_session_route", "usage_rows"}


def _patch_identity(main_serves, canonical, empty):
    """Config-yaml + custom-provider registry seams of the bare-custom heal."""
    live._main_slot_serves = lambda model: bool(main_serves)
    rp_mod.canonical_custom_identity = lambda **kw: canonical
    rpc_mod.empty_provider_identity = lambda base_url, model: empty


def heal_provider(r):
    _patch_identity(r.get("main_slot_serves"), r.get("canonical"), r.get("empty_identity"))
    return live._heal_bare_custom_provider(
        r.get("provider"), base_url=r.get("base_url"), model=r.get("model"))


def runtime_fields(r):
    state = r.get("state") or {}
    return live._runtime_fields(SimpleNamespace(**state))


def snapshot_runtime(r):
    state = r.get("state") or {}
    ns = SimpleNamespace(**state)
    agent = r.get("agent")
    ns.agent = SimpleNamespace(**agent) if agent is not None else None
    return live.CLIModelSwitchMixin._snapshot_model_runtime(ns)


def configured_route(r):
    cfg = r.get("model_cfg")
    config_mod.load_config_readonly = lambda: {"model": cfg}
    return list(live._configured_default_route())


def stored_session_route(r):
    resume = r.get("resume") or {}
    _patch_identity(r.get("main_slot_serves"), r.get("canonical"), r.get("empty_identity"))
    sessions_mod.parse_model_config = lambda v: v if isinstance(v, dict) else {}

    def _resume(model_config, **kw):
        return resume.get("model"), SimpleNamespace(
            provider=resume.get("provider"), base_url=resume.get("base_url"),
            api_mode=resume.get("api_mode"))

    routing_mod.resume_route = _resume
    foreign = bool(r.get("foreign"))
    rp_mod.is_foreign_provider_endpoint = lambda provider, base_url: foreign
    derived = r.get("derived")
    switch_mod.model_derived_api_mode = lambda provider, model, api_key="": derived
    normalized = r.get("normalized_base_url")
    models_mod.normalize_opencode_base_url = lambda provider, api_mode, base_url: normalized

    route = live.stored_session_route(
        r.get("session") or {}, current_model=r.get("current_model"),
        current_provider=r.get("current_provider"))
    return list(route) if route else None


def usage_rows(r):
    descs = r.get("descriptions") or {}
    live.t = lambda key, **kw: descs.get(key, key)
    col = live._MODEL_USAGE_COL
    out = []
    for form, desc_key in live._MODEL_USAGE_ROWS:
        text = live.t(desc_key)
        out.append({"form": form, "desc": text, "line": "  " + form.ljust(col) + " " + text})
    return out


def expected(r):
    f = r.get("func")
    if f == "heal_provider":
        return heal_provider(r)
    if f == "runtime_fields":
        return runtime_fields(r)
    if f == "snapshot_runtime":
        return snapshot_runtime(r)
    if f == "configured_route":
        return configured_route(r)
    if f == "stored_session_route":
        return stored_session_route(r)
    if f == "usage_rows":
        return usage_rows(r)
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
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if cases < MIN_CASES:
        print("ORACLE %s: only %d cases (< %d) — gate not exercised"
              % (GATE, cases, MIN_CASES), file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
