#!/usr/bin/env python3
"""Differential oracle for w17_modelpicke_b36083 / hermes_cli/model_picker_usage.py."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/model_picker_usage.py"
sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

spec = importlib.util.spec_from_file_location("mpu_live_w17", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["mpu_live_w17"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "clamp_fraction": return live.clamp_fraction(r.get("value", 0.0))
    if f == "gauge_style": return live.gauge_style(r.get("value", 0.0))
    if f == "filled_cells": return live._filled_cells(r.get("value", 0.0), int(r.get("width", 10)))
    if f == "render_bar": return live.render_bar(r.get("value", 0.0), int(r.get("width", 10)))
    if f == "format_percent_used": return live.format_percent_used(r.get("value", 0.0))
    if f == "compact_duration": return live.compact_duration(r.get("resets_at_ms"), r.get("now_ms"))
    if f == "format_resets_in": return live.format_resets_in(r.get("resets_at_ms"), r.get("now_ms"))
    if f == "split_recommended":
        head, rest = live.split_recommended(r.get("model_ids") or [], r.get("featured") or [])
        return {"recommended": head, "other": rest}
    if f == "featured_first": return live.featured_first(r.get("model_ids") or [], r.get("featured") or [])
    if f == "credits_pay": return live._credits_pay(r.get("info") or {}, r.get("window") or {})
    if f == "is_free_model": return live.is_free_model(r.get("pricing_row"))
    if f == "capitalize": return live._capitalize(r.get("text", ""))
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None: continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w17_modelpicke_b36083: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w17_modelpicke_b36083 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_modelpicke_b36083: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
