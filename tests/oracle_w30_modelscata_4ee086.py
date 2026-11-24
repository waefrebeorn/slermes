#!/usr/bin/env python3
"""Differential oracle for hermes_cli/models_catalog_static.py (gate w30_modelscata_4ee086).

The live module is imported as a package module (``hermes_cli.models_catalog_static``)
and its value->value surface is called directly.  One seam is stubbed at the
documented boundary and nothing else: ``providers`` is replaced by an empty
registry *before* import, so ``sync_plugin_provider_catalog()`` admits no
plugin rows and the tables compared here are exactly the built-in ones the C
port owns.  Without that stub every table would grow with whatever plugin
profiles happen to sit in the quarry, which is a property of the install, not
of the module.

Rows that the module fills from an outside source are declared, not compared:
``DYNAMIC_ROWS`` are the provider rows whose ids come from the models.dev disk
cache (``xai``/``xai-oauth``) or from ``hermes_cli.codex_models``
(``openai-codex``).  The C port reports them in ``dynamic_model_rows`` and owns
only the static floor they fall back to.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_modelscata_4ee086"

# The quarry package must win: tests/hermes_cli and tests/tools shadow it.
sys.path.insert(0, str(QUARRY))

DYNAMIC_ROWS = ("xai", "xai-oauth", "openai-codex")

_stub = types.ModuleType("providers")
_stub.list_providers = lambda: []
_stub.unlisted_provider_names = lambda: set()
_stub.bound_registry_key = lambda: ()
sys.modules["providers"] = _stub

try:
    from hermes_cli import models_catalog_static as live  # noqa: E402
except ImportError:  # blocked native extension somewhere in the chain
    sys.path.append(str(Path(__file__).resolve().parent))
    from hermes_seam_stubs import install as _install_seams  # noqa: E402
    _install_seams()
    from hermes_cli import models_catalog_static as live  # noqa: E402

WANTED = {
    "constants",
    "xai_promote_top",
    "xai_merge_curated_extras",
    "xai_finalize_catalog",
    "provider_group_for_slug",
    "group_providers",
    "plugin_provider_enters_picker",
    "provider_models",
    "provider_label",
    "provider_alias",
    "canonical_provider_row",
    "group_members",
}


def constants():
    label, desc, members = 0, 1, 2
    return {
        "provider_groups": {
            gid: [row[label], row[desc], list(row[members])]
            for gid, row in live.PROVIDER_GROUPS.items()
        },
        "slug_to_group": dict(live._SLUG_TO_GROUP),
        "provider_models": {
            slug: list(dict.get(live._PROVIDER_MODELS, slug))
            for slug in dict.keys(live._PROVIDER_MODELS)
            if slug not in DYNAMIC_ROWS
        },
        "canonical_providers": [
            [p.slug, p.label, p.tui_desc] for p in live.CANONICAL_PROVIDERS
        ],
        "provider_labels": dict(live._PROVIDER_LABELS),
        "provider_aliases": dict(dict.items(live._PROVIDER_ALIASES)),
        "provider_retired_aliases": {
            slug: list(ids) for slug, ids in live._PROVIDER_RETIRED_ALIASES.items()
        },
        "openrouter_models": [[mid, d] for mid, d in live.OPENROUTER_MODELS],
        "vercel_ai_gateway_models": [[mid, d] for mid, d in live.VERCEL_AI_GATEWAY_MODELS],
        "openrouter_only": sorted(live._OPENROUTER_ONLY),
        "xai_static_fallback": list(live._XAI_STATIC_FALLBACK),
        "xai_curated_extras": list(live._XAI_CURATED_EXTRAS),
        "xai_top_model": live._XAI_TOP_MODEL,
        "preferred_silent_default_model": live.PREFERRED_SILENT_DEFAULT_MODEL,
        "silent_default_providers": sorted(live._SILENT_DEFAULT_PROVIDERS),
        "aggregator_providers": sorted(live._AGGREGATOR_PROVIDERS),
        "borrowed_model_providers": sorted(live._BORROWED_MODEL_PROVIDERS),
        "live_first_picker_providers": sorted(live._LIVE_FIRST_PICKER_PROVIDERS),
        "openai_fast_mode_prefixes": list(live._OPENAI_FAST_MODE_PREFIXES),
        "openai_ultrafast_models": sorted(live._OPENAI_ULTRAFAST_MODELS),
        "models_dev_preferred": sorted(live._MODELS_DEV_PREFERRED),
        "azure_foundry_responses_prefixes": list(live._AZURE_FOUNDRY_RESPONSES_PREFIXES),
        "builtin_slugs": sorted(live._BUILTIN_SLUGS),
        "dynamic_model_rows": list(DYNAMIC_ROWS),
        "openai_chat_models": list(live._OPENAI_CHAT_MODELS),
        "minimax_models": list(live._MINIMAX_MODELS),
        "tencent_models": list(live._TENCENT_MODELS),
        "alibaba_models": list(live._ALIBABA_MODELS),
        "alibaba_coding_plan_models": list(live._ALIBABA_CODING_PLAN_MODELS),
        "alibaba_token_plan_models": list(live._ALIBABA_TOKEN_PLAN_MODELS),
    }


def xai_promote_top(r):
    return live._xai_promote_top(list(r.get("ids") or []))


def xai_merge_curated_extras(r):
    return live._xai_merge_curated_extras(list(r.get("ids") or []))


def xai_finalize_catalog(r):
    return live._xai_finalize_catalog(list(r.get("ids") or []))


def provider_group_for_slug(r):
    return live.provider_group_for_slug(r.get("slug"))


def group_providers(r):
    return live.group_providers(list(r.get("slugs") or []))


def plugin_provider_enters_picker(r):
    return bool(live._plugin_provider_enters_picker(types.SimpleNamespace(name=r.get("name"))))


def provider_models(r):
    return dict.get(live._PROVIDER_MODELS, r.get("slug"))


def provider_label(r):
    return live._PROVIDER_LABELS.get(r.get("slug"))


def provider_alias(r):
    return live._PROVIDER_ALIASES.get(r.get("key"))


def canonical_provider_row(r):
    for p in live.CANONICAL_PROVIDERS:
        if p.slug == r.get("slug"):
            return [p.slug, p.label, p.tui_desc]
    return None


def group_members(r):
    row = live.PROVIDER_GROUPS.get(r.get("gid"))
    if row is None:
        return None
    return [row[0], row[1], list(row[2])]


def constants_row(r):
    return constants()


EXPECTED = {
    "constants": constants_row,
    "xai_promote_top": xai_promote_top,
    "xai_merge_curated_extras": xai_merge_curated_extras,
    "xai_finalize_catalog": xai_finalize_catalog,
    "provider_group_for_slug": provider_group_for_slug,
    "group_providers": group_providers,
    "plugin_provider_enters_picker": plugin_provider_enters_picker,
    "provider_models": provider_models,
    "provider_label": provider_label,
    "provider_alias": provider_alias,
    "canonical_provider_row": canonical_provider_row,
    "group_members": group_members,
}


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
        name = r["func"]
        try:
            py = EXPECTED[name](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (name, r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (name, r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
