#!/usr/bin/env python3
"""
sta_oracle_skills_hub_filter.py — Python oracle for the PURE
_filter_results_by_provider helper (tools/skills_hub.py), ported in
src/skills_hub.c:hub_filter_results_by_provider.

Imports the REAL tools.skills_hub module, builds SkillMeta objects with
extra={"provider": ...}, and calls the genuine function. Output contract
matches tests/t_port_skills_hub_filter.c: one JSON object per case, sorted
keys, ensure_ascii=False, compact separators.

NOTE: the C side was a faithful-divergence bug (it substring-matched
source_url). This oracle drove the fix to an exact, case-insensitive provider
match on a dedicated provider field.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.skills_hub import SkillMeta, _filter_results_by_provider  # noqa: E402


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def main():
    entries = []
    pending_filter = ""

    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if not line.strip():
            continue
        if line.startswith("filter "):
            pending_filter = line[7:]
            continue
        if line.startswith("==="):
            results = []
            for (slug, name, prov) in entries:
                m = SkillMeta(
                    name=name, description="", source="github",
                    identifier=slug, trust_level="community",
                    extra={"provider": prov} if prov else {},
                )
                results.append(m)
            kept = _filter_results_by_provider(results, pending_filter)
            out = [{"slug": m.identifier, "provider": m.extra.get("provider", "")}
                   for m in kept]
            emit({"provider": pending_filter, "matched": len(kept) > 0, "kept": out})
            entries = []
            pending_filter = ""
            continue

        # entry: slug|name|provider
        parts = line.split("|")
        slug = parts[0]
        name = parts[1] if len(parts) > 1 else ""
        prov = parts[2] if len(parts) > 2 else ""
        entries.append((slug, name, prov))

    # flush trailing case
    if entries or pending_filter:
        results = []
        for (slug, name, prov) in entries:
            m = SkillMeta(
                name=name, description="", source="github",
                identifier=slug, trust_level="community",
                extra={"provider": prov} if prov else {},
            )
            results.append(m)
        kept = _filter_results_by_provider(results, pending_filter)
        out = [{"slug": m.identifier, "provider": m.extra.get("provider", "")}
               for m in kept]
        emit({"provider": pending_filter, "matched": len(kept) > 0, "kept": out})


if __name__ == "__main__":
    sys.exit(main())
