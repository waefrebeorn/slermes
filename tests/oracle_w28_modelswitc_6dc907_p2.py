#!/usr/bin/env python3
"""Differential oracle for hermes_cli/model_switch_providers.py (gate w28_modelswitc_6dc907_p2).

Drives the live ``_PickerBuild`` decision core: a bare instance (``__new__``, no
dataclass init) is given exactly the fixture state, so no config load, network
probe or registry lookup happens.  The live catalog fetch, ``get_label`` and
``_declared_model_ids`` are replaced by the values the fixture declares.
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
GATE = "w28_modelswitc_6dc907_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.model_switch_providers as live  # noqa: E402
import hermes_cli.model_switch as ms_mod  # noqa: E402
import hermes_cli.models as models_mod  # noqa: E402
import hermes_cli.anon_auth as anon  # noqa: E402

WANTED = {
    "discover_models", "resolved_fast_probe", "can_probe_custom", "endpoint_is_current",
    "record_section3_pair", "add_builtin_row", "add_endpoint_row", "discover_endpoint",
    "lmstudio_row",
}


def build(**attrs):
    b = live._PickerBuild.__new__(live._PickerBuild)
    defaults = dict(
        current_provider="", current_base_url="", current_model="", max_models=None,
        for_picker=False, force_fresh_nous_tier=False, probe_custom_providers=True,
        probe_current_custom_provider=False, refresh=False, excluded=set(), curated={},
        fast_custom_probe=None, non_blocking_catalogs=False, results=[], seen_slugs=set(),
        builtin_endpoints=set(), section3_pairs=set(),
    )
    defaults.update(attrs)
    for k, v in defaults.items():
        setattr(b, k, v)
    return b


def discover_models(req):
    live_models = req.get("live_models")
    opts = req.get("opts") or {}
    native = bool(opts.get("native"))
    py_live = (None if live_models is None else
               (live._NativePickerModelList(live_models) if native else list(live_models)))
    with mock.patch.object(live, "_fetch_picker_live_models", lambda *a, **k: py_live), \
         mock.patch.object(models_mod, "cached_fetch_api_models", lambda *a, **k: py_live):
        models, empty = live._discover_endpoint_models(
            "probe-key", "http://127.0.0.1:1234/v1", "custom",
            bool(opts.get("has_explicit_models")), headers=None, api_mode=None,
            probe_live=bool(opts.get("probe_live")),
            discovery_allowed=bool(opts.get("discovery_allowed")),
            fast_custom_probe=False)
    return {"models": None if models is None else list(models),
            "native_catalog_empty": bool(empty)}


def add_builtin_row(req):
    b = build(max_models=req.get("max_models"))
    b.record_builtin_endpoint = lambda slug: None
    flags = req.get("flags") or {}
    with mock.patch.multiple(
            anon,
            has_guest=lambda: bool(flags.get("guest")),
            guest_allowed=lambda: bool(flags.get("guest_allowed")),
            free_inference_active=lambda state=None: bool(flags.get("free_inference")),
            FREE_TIER_LABEL=flags.get("label", "Nous \u00b7 free tier"),
            GUEST_MODEL=flags.get("guest_model", "nous/welcome")):
        uncapped = req.get("uncapped_ok")
        b.add_builtin_row(req.get("slug"), req.get("name"), bool(req.get("is_current")),
                          list(req.get("model_ids") or []), req.get("source") or "",
                          uncapped_ok=True if uncapped is None else bool(uncapped))
    slug = req.get("slug") or ""
    return {"row": b.results[0] if b.results else None, "seen": [slug.lower()]}


def lmstudio_row(req):
    b = build(excluded=set(req.get("excluded") or []), seen_slugs=set(req.get("seen") or []),
              curated={"lmstudio": list(req.get("curated") or [])},
              current_provider=req.get("current_provider") or "",
              max_models=req.get("max_models"))
    configured = req.get("configured")
    user_providers = {"lmstudio": configured} if configured is not None else {}
    label = req.get("label") or "lmstudio"
    declared = list(req.get("declared_models") or [])
    with mock.patch.object(live, "get_label", lambda slug: label), \
         mock.patch.object(ms_mod, "_declared_model_ids", lambda models: list(declared)):
        live._lap_lmstudio_row(b, user_providers)
    return {"row": b.results[0] if b.results else None}


def compute(r):
    f = r.get("func")
    if f == "discover_models":
        return discover_models(r)
    if f == "resolved_fast_probe":
        b = build(for_picker=r.get("for_picker"), fast_custom_probe=r.get("fast_custom_probe"))
        return b.resolved_fast_custom_probe
    if f == "can_probe_custom":
        b = build(probe_custom_providers=r.get("probe_custom_providers"),
                  probe_current_custom_provider=r.get("probe_current_custom_provider"))
        return b.can_probe_custom(row_is_current=bool(r.get("row_is_current")))
    if f == "endpoint_is_current":
        cfg = r.get("cfg") or {}
        b = build(current_provider=cfg.get("current_provider") or "",
                  current_base_url=cfg.get("current_base_url") or "")
        match_ok = cfg.get("url_match_ok")
        return b.endpoint_is_current(r.get("slug"), set(r.get("aliases") or []),
                                     r.get("url_norm") or "",
                                     url_match_ok=True if match_ok is None else bool(match_ok))
    if f == "record_section3_pair":
        b = build()
        ok = b.record_section3_pair(r.get("name") or "", r.get("url_norm"))
        pair = [str(r.get("name") or "").strip().lower(), r.get("url_norm")] if ok else None
        return {"added": ok, "pair": pair}
    if f == "add_builtin_row":
        return add_builtin_row(r.get("req") or {})
    if f == "add_endpoint_row":
        req = r.get("req") or {}
        b = build()
        b.add_endpoint_row(req.get("slug"), req.get("name"), req.get("api_url"),
                           req.get("models") or [], bool(req.get("is_current")),
                           bool(req.get("native_catalog_empty")),
                           source=req.get("source") or "user-config",
                           shown=req.get("shown"))
        slug = req.get("slug") or ""
        return {"row": b.results[0] if b.results else None, "seen": [slug.lower()]}
    if f == "discover_endpoint":
        req = r.get("req") or {}
        b = build(probe_custom_providers=req.get("probe_custom_providers"),
                  probe_current_custom_provider=req.get("probe_current_custom_provider"),
                  fast_custom_probe=None)
        with mock.patch.object(live, "_discover_endpoint_models", lambda *a, **k: (None, False)):
            _models, _empty, probe_live = b.discover_endpoint(
                req.get("api_key"), req.get("api_url") or "",
                req.get("native_provider") or "custom",
                bool(req.get("has_explicit_models")), headers=None, api_mode=None,
                discovery_allowed=bool(req.get("discovery_allowed")),
                is_current=bool(req.get("is_current")))
        return {"probe_live": bool(probe_live)}
    if f == "lmstudio_row":
        return lmstudio_row(r.get("req") or {})
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
