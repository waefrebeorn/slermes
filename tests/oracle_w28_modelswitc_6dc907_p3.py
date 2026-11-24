#!/usr/bin/env python3
"""Differential oracle for hermes_cli/model_switch_providers.py (gate w28_modelswitc_6dc907_p3).

Drives the live post-passes over row lists.  The seams they touch -- config
``is_provider_enabled`` (real, pure), account-gated model discovery, the
inventory row injectors, the openrouter catalog fetch, unofferable-id dropping
and the usage-window snapshot -- are replaced by the fixture values.
"""
import copy
import json
import sys
from pathlib import Path
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_modelswitc_6dc907_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.model_switch_providers as live  # noqa: E402
import hermes_cli.model_switch as ms_mod  # noqa: E402
import hermes_cli.models as models_mod  # noqa: E402
import hermes_cli.inventory as inventory  # noqa: E402
import hermes_cli.models_validate as models_validate  # noqa: E402
import hermes_cli.model_picker_usage as picker_usage  # noqa: E402
import agent.plan_limits as plan_limits  # noqa: E402

WANTED = {"finalize_rows", "prepend_moa", "picker_filter", "local_runtime_rows",
          "attach_window_notes"}


def finalize_rows(req):
    results = copy.deepcopy(req.get("results") or [])
    user_providers = req.get("user_providers")
    pairs = {(p[0], p[1]) for p in (req.get("requires_discovery") or []) if len(p) >= 2}
    with mock.patch.object(models_mod, "_model_requires_account_discovery",
                           lambda provider, model: (provider, model) in pairs):
        return live._finalize_picker_rows(results, user_providers, req.get("current_model") or "")


def prepend_moa(req):
    providers = copy.deepcopy(req.get("providers") or [])
    moa_row = copy.deepcopy(req.get("moa_row"))
    with mock.patch.object(inventory, "_moa_provider_row", lambda current_provider: moa_row):
        return live._prepend_moa_picker_provider(providers, current_provider="")


def picker_filter(req):
    providers = copy.deepcopy(req.get("providers") or [])
    live_ids = list(req.get("openrouter_live_ids") or [])
    explode = bool(req.get("openrouter_error"))

    def fetch_openrouter_models(cache_only=False):
        if explode:
            raise RuntimeError("catalog unreachable")
        return [(mid, None) for mid in live_ids]

    with mock.patch.object(ms_mod, "list_authenticated_providers", lambda *a, **k: providers), \
         mock.patch.object(live, "_with_local_runtime_row", lambda rows, *a, **k: rows), \
         mock.patch.object(live, "_prepend_moa_picker_provider", lambda rows, **k: rows), \
         mock.patch.object(models_mod, "fetch_openrouter_models", fetch_openrouter_models), \
         mock.patch.object(models_validate, "drop_unofferable_model_ids", lambda rows: None), \
         mock.patch.object(live, "_attach_window_model_notes", lambda rows: None):
        return live.list_picker_providers(max_models=req.get("max_models"))


def local_runtime_rows(req):
    rows = copy.deepcopy(req.get("rows") or [])
    local_row = copy.deepcopy(req.get("local_row"))
    def inject(given_rows, ctx):
        out_rows = list(given_rows)
        if local_row is not None:
            out_rows = out_rows + [local_row]
        return out_rows, local_row

    with mock.patch.object(inventory, "inject_local_runtime_row", inject):
        return live._with_local_runtime_row(rows, "", "", "",
                                            max_models=req.get("max_models"))


def attach_window_notes(req):
    providers = copy.deepcopy(req.get("providers") or [])
    usage = req.get("usage")

    class _Usage:
        def limit_line(self):
            return (usage or {}).get("limit_line") or ""

    fresh = _Usage() if isinstance(usage, dict) and usage.get("fresh") else None
    any_age = _Usage() if isinstance(usage, dict) else None

    def nous_picker_usage(*args, **kwargs):
        return any_age if kwargs.get("any_age") else fresh

    levels = dict((usage or {}).get("levels") or {}) if isinstance(usage, dict) else {}
    tags = dict((usage or {}).get("tags") or {}) if isinstance(usage, dict) else {}

    with mock.patch.object(picker_usage, "nous_picker_usage", nous_picker_usage), \
         mock.patch.object(picker_usage, "usage_tag_levels",
                           lambda models, pricing: {m: levels[m] for m in models if m in levels}), \
         mock.patch.object(picker_usage, "usage_level_label",
                           lambda level, lang=None: tags.get(level, "")), \
         mock.patch.object(plan_limits, "cached_pricing", lambda: {}):
        live._attach_window_model_notes(providers)
    return providers


def compute(r):
    f = r.get("func")
    if f == "finalize_rows":
        return finalize_rows(r.get("req") or {})
    if f == "prepend_moa":
        return prepend_moa(r.get("req") or {})
    if f == "picker_filter":
        return picker_filter(r.get("req") or {})
    if f == "local_runtime_rows":
        return local_runtime_rows(r.get("req") or {})
    if f == "attach_window_notes":
        return attach_window_notes(r.get("req") or {})
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
            py = compute(r)
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
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
