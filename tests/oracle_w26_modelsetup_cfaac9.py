#!/usr/bin/env python3
"""Differential oracle for hermes_cli/model_setup_flows_azure.py (gate w26_modelsetup_cfaac9).

The wizard's prompts, its ``.env`` read and the endpoint probe are the seams;
each case scripts them:

  * ``_azure_mode_label``   -- the transport label for an api_mode;
  * ``_azure_current``      -- the active settings shown as prompt defaults
    (``get_env_value`` supplies AZURE_FOUNDRY_API_KEY, config arrives parsed);
  * ``_azure_pick_model``   -- the model/deployment step, with ``_ask`` answering
    the fixture's scripted answer ("" = take the default, None = cancelled);
  * ``_azure_detect_transport`` -- the transport pick when the probe finds no
    api_mode, with ``azure_detect.detect`` returning the fixture's detection.
"""
import contextlib
import dataclasses
import io
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_modelsetup_cfaac9"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import model_setup_flows_azure as live  # noqa: E402
import hermes_cli.azure_detect as azure_detect  # noqa: E402
import hermes_cli.config as cli_config  # noqa: E402

WANTED = {"mode_label", "current", "pick_model", "transport", "constants"}


def _quiet(fn, *args):
    with contextlib.redirect_stdout(io.StringIO()):
        return fn(*args)


def mode_label(req):
    return live._azure_mode_label(req.get("mode"))


def current(req):
    env_value = req.get("env_key")
    cli_config.get_env_value = lambda key: env_value if key == "AZURE_FOUNDRY_API_KEY" else None
    cur = live._azure_current(req.get("config") if isinstance(req.get("config"), dict) else {})
    return dataclasses.asdict(cur)


def pick_model(req):
    answer = req.get("answer")
    live._ask = lambda prompt, raw=False, secret=False: answer
    models = req.get("models")
    return _quiet(live._azure_pick_model, models if isinstance(models, list) else [],
                  req.get("current"))


def transport(req):
    detected = req.get("detected") or {}
    answer = req.get("answer")
    azure_detect.detect = lambda url, api_key=None, token_provider=None: types.SimpleNamespace(
        models=list(detected.get("models") or []),
        api_mode=detected.get("api_mode") or "",
        reason=detected.get("reason") or "",
    )
    live._say = lambda *a, **k: None
    live._ask = lambda prompt, raw=False, secret=False: answer
    out = _quiet(live._azure_detect_transport, "https://example.openai.azure.com/openai/v1", "k",
                 None, req.get("current_api_mode") or "")
    if out is None:
        return None
    return [out[0], list(out[1])]


def constants():
    return {
        "provider": "azure-foundry",
        "api_key_env": "AZURE_FOUNDRY_API_KEY",
        "api_modes": ["chat_completions", "anthropic_messages"],
        "auth_modes": ["api_key", "entra_id"],
        "http_prefix": list(live._HTTP),
        "pick_limit": 30,
    }


def expected(r):
    f = r.get("func")
    if f == "mode_label":
        return mode_label(r)
    if f == "current":
        return current(r)
    if f == "pick_model":
        return pick_model(r)
    if f == "transport":
        return transport(r)
    if f == "constants":
        return constants()
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
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
