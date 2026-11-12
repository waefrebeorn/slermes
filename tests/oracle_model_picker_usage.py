#!/usr/bin/env python3
"""Oracle for hermes_cli/model_picker_usage.py — the reusable plan-usage chrome
helpers (gauge style/bar/percent, reset countdowns, recommended-first split,
credits-pay rule, free-model test).

The harness (t_port_model_picker_usage.c) emits one JSON line per case with a
"fn" discriminator; "out" is the serialized C result.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from hermes_cli.model_picker_usage import (          # noqa: E402
    _capitalize, _filled_cells, _get, clamp_fraction, compact_duration,
    featured_first, format_percent_used, format_resets_in, gauge_style,
    is_free_model, render_bar, split_recommended,
)
from hermes_cli.model_picker_usage import _credits_pay   # noqa: E402


def main() -> int:
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        try:
            if fn == "clamp_fraction":
                want = clamp_fraction(i["value"])
            elif fn == "gauge_style":
                want = gauge_style(i["left"])
            elif fn == "filled_cells":
                want = _filled_cells(i["fraction"], i["width"])
            elif fn == "render_bar":
                want = render_bar(i["fraction"], i["width"])
            elif fn == "format_percent_used":
                want = format_percent_used(i["used"])
            elif fn == "compact_duration":
                want = compact_duration(i.get("resets"), i.get("now"))
            elif fn == "format_resets_in":
                want = format_resets_in(i.get("resets"), i.get("now"))
            elif fn == "split_recommended":
                h, r = split_recommended(i["ids"], i["featured"])
                want = {"head": h, "rest": r}
            elif fn == "featured_first":
                want = featured_first(i["ids"], i["featured"])
            elif fn == "credits_pay":
                v = _credits_pay(i.get("info"), i.get("window"))
                want = None if v is None else bool(v)
            elif fn == "is_free_model":
                want = is_free_model(i.get("row"))
            elif fn == "capitalize":
                want = _capitalize(i["text"])
            elif fn == "obj_get":
                want = _get(i.get("obj"), i["name"], i.get("default"))
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        raw = c["out"]
        if isinstance(raw, dict) and set(raw) == {"str"}:
            got = raw["str"]                 # a C string result
        elif isinstance(raw, str):
            got = json.loads(raw)            # a serialized JSON value
        else:
            got = raw
        if fn == "credits_pay" and isinstance(got, int):
            got = None if got < 0 else bool(got)
        if got != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={c['out'][:200]} "
                  f"PY={want}")
    if total == 0:
        print("ORACLE model_picker_usage: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE model_picker_usage: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
