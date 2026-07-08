#!/usr/bin/env python3
"""Faithfulness oracle for port_learning_graph_render.c.

Reads the JSON the C harness dumped to /tmp/lgr_c.json, recomputes every
value from the LIVE source agent/learning_graph_render.py, and does an exact
byte comparison. Prints PYCOMPARE OK or MISMATCH <key>.
"""
import json, sys
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from agent.learning_graph_render import (
    _clamp, _smoothstep, recency_ink, _to_ts, hex_to_rgb, rgb_to_hex,
    mix_rgb, _rgb_to_hsl, _hsl_to_rgb, _complementary_ink, _node_score,
    _node_label, _node_meta, derive_palette, _period_label, format_date,
)

C = json.load(open("/tmp/lgr_c.json"))

def hx(c): return "#%02X%02X%02X" % (c[0], c[1], c[2])

exp = {}
exp["clamp_lo"]   = round(_clamp(-1, 0, 1), 6)
exp["clamp_hi"]   = round(_clamp(5, 0, 1), 6)
exp["clamp_mid"]  = _clamp(0.4, 0, 1)
exp["smooth_0"]   = _smoothstep(0)
exp["smooth_1"]   = _smoothstep(1)
exp["smooth_05"]  = _smoothstep(0.5)
exp["smooth_ov"]  = _smoothstep(2)
exp["rec_0"]      = round(recency_ink(0), 6)
exp["rec_mid"]    = round(recency_ink(0.52), 6)
exp["rec_1"]      = round(recency_ink(1), 6)
exp["rec_ov"]     = round(recency_ink(2), 6)
exp["to_ts_none"] = 1 if _to_ts(None) is None else 0
exp["to_ts_bad"]  = 1 if _to_ts("abc") is None else 0
exp["to_ts_int"]  = _to_ts("1700000000.5")
exp["hex3"]       = hx(hex_to_rgb("#abc"))
exp["hex6"]       = hx(hex_to_rgb("#1a2b3c"))
exp["hexbad"]     = hx(hex_to_rgb("zz"))
exp["rgb2hex"]    = rgb_to_hex((26, 43, 60))
exp["mix_half"]   = hx(mix_rgb((0, 0, 0), (100, 200, 50), 0.5))
exp["hsl_r"]      = round(_rgb_to_hsl((255, 0, 0))[0], 6)
exp["hsl2rgb_g"]  = hx(_hsl_to_rgb(120, 1.0, 0.5))
exp["comp"]       = hx(_complementary_ink((255, 0, 0)))
exp["node_mem"]   = _node_score({"kind": "memory"}, 0.5)
exp["node_skill"] = _node_score({"kind": "skill", "useCount": 4, "pinned": True}, 0.5)
exp["lab_short"]  = _node_label({"label": "hello", "id": "X"})
exp["lab_long"]   = _node_label({"label": "a" * 30, "id": "X"})
exp["lab_id"]     = _node_label({"label": None, "id": "node-9"})
exp["lab_long_bytelen"] = len(_node_label({"label": "a" * 30, "id": "X"}).encode("utf-8"))
exp["meta_mem"]   = _node_meta({"kind": "memory", "memorySource": "profile", "timestamp": 1700000000})
exp["meta_skill"] = _node_meta({"kind": "skill", "category": "python", "timestamp": 1700000000, "useCount": 3, "pinned": True})
pal = derive_palette("#00C2A8", dark=True)
exp["pal_primary"] = pal["primary"]
exp["pal_memory"]  = pal["memory"]
exp["pal_skill"]   = pal["skill"]
exp["pal_label"]   = pal["label"]
exp["pal_dim"]     = pal["dim"]
exp["pal_bg"]      = pal["bg"]
exp["per_day"]     = _period_label(1700000000, "day")
exp["per_month"]   = _period_label(1700000000, "month")
exp["fmt_date"]    = format_date(1700000000.0)
exp["fmt_none"]    = format_date(0)

ok = True
for k in exp:
    cval = C.get(k)
    eval_ = exp[k]
    # numeric keys: compare as floats (tolerates 1 vs 1.0, formatting)
    if isinstance(cval, (int, float)) or (
        isinstance(cval, str) and cval.replace(".", "", 1).replace("e", "", 1).lstrip("-").isdigit()
    ):
        try:
            cf = float(cval); ef = float(eval_)
            if cf != ef:
                ok = False
                print("MISMATCH", k, "C=", repr(cval), "PY=", repr(eval_))
            continue
        except (ValueError, TypeError):
            pass
    # string keys: exact byte compare
    if str(cval) != str(eval_):
        ok = False
        print("MISMATCH", k, "C=", repr(cval), "PY=", repr(eval_))
print("PYCOMPARE", "OK" if ok else "BAD", f"({len(exp)} keys checked)")
sys.exit(0 if ok else 1)
