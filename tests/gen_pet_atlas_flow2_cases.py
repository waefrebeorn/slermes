#!/usr/bin/env python3
"""Deterministic case generator for pet_atlas_flow2 (6 cases per UNCLAIMED fn).

Each case is {"func": name, "in": {args..., "io": {seam: script}}}. The harness
replays it through the C port and the oracle replays the SAME scripts through
the LIVE atlas.py def; their {ret,trace,state} must agree. Run:

    python tests/gen_pet_atlas_flow2_cases.py
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "pet_atlas_flow2.jsonl"


def c(func, inp, io=None):
    rec = dict(inp)
    rec["io"] = io or {}
    return {"func": func, "in": rec}


def seq(*items):
    return {"$seq": list(items)}


def raises(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def img(h, size=None):
    d = {"handle": h}
    if size is not None:
        d["size"] = list(size)
    return d


def bools(n, pattern=(True, False, True, True, False)):
    return [pattern[i % len(pattern)] for i in range(n)]


CASES = []

# ── _median (pure; empty -> IndexError) ────────────────────────────────────
CASES += [
    c("_median", {"values": [5]}),
    c("_median", {"values": [3, 1, 2]}),
    c("_median", {"values": [1, 2, 3, 4]}),
    c("_median", {"values": [10, 20, 30, 40, 50]}),
    c("_median", {"values": [-5, -1, -3]}),
    c("_median", {"values": []}),
]

# ── _median_box_size (composes _median) ───────────────────────────────────
CASES += [
    c("_median_box_size", {"boxes": [[0, 0, 10, 20]]}),
    c("_median_box_size", {"boxes": [[0, 0, 10, 20], [0, 0, 20, 40], [0, 0, 4, 8]]}),
    c("_median_box_size", {"boxes": [[0, 0, 2, 2], [0, 0, 4, 4], [0, 0, 6, 6], [0, 0, 8, 8]]}),
    c("_median_box_size", {"boxes": [[0, 0, 0, 0], [0, 0, 10, 10]]}),
    c("_median_box_size", {"boxes": [[5, 5, 15, 25], [0, 0, 7, 9]]}),
    c("_median_box_size", {"boxes": [[1, 2, 9, 30], [3, 4, 11, 40], [2, 1, 10, 20]]}),
]

# ── _blank (Image.new seam) ───────────────────────────────────────────────
for i, sz in enumerate([[8, 5], [192, 208], [0, 0], [1, 1], [3, 7], [10, 10]]):
    CASES.append(c("_blank", {"size": sz},
                   {"Image.new": img("blank%d" % i, sz)}))

# ── _place (_blank + alpha_composite) ──────────────────────────────────────
for i, (sz, off, sp) in enumerate([
    ([8, 5], [0, 0], img("sp0", [4, 3])),
    ([192, 208], [10, 20], img("sp1", [40, 50])),
    ([10, 10], [2, 3], img("sp2", [6, 6])),
    ([1, 1], [0, 0], img("sp3", [1, 1])),
    ([20, 30], [5, 7], img("sp4", [8, 9])),
    ([7, 7], [1, 1], img("sp5", [3, 3])),
]):
    CASES.append(c("_place", {"sprite": sp, "size": sz, "offset": off},
                   {"Image.new": img("canv%d" % i, sz), "alpha_composite": None}))

# ── _clear_region (_blank + paste) ────────────────────────────────────────
for i, (im, box) in enumerate([
    (img("im0", [20, 20]), [0, 0, 5, 5]),
    (img("im1", [20, 20]), [2, 3, 10, 12]),
    (img("im2", [40, 30]), [10, 5, 30, 25]),
    (img("im3", [8, 8]), [0, 0, 8, 8]),
    (img("im4", [192, 208]), [50, 60, 100, 140]),
    (img("im5", [5, 5]), [1, 1, 2, 2]),
]):
    w, h = im["size"]
    CASES.append(c("_clear_region", {"image": im, "box": box},
                   {"Image.new": img("clr%d" % i, [box[2] - box[0], box[3] - box[1]]),
                    "paste": None}))

# ── _load_rgba (5 handle + 1 path) ────────────────────────────────────────
for i in range(5):
    src = img("raw%d" % i, [10 + i, 20 + i])
    CASES.append(c("_load_rgba", {"image": src},
                   {"convert": img("rgba%d" % i, [10 + i, 20 + i])}))
CASES.append(c("_load_rgba", {"image": "C:/pics/strip.png"},
               {"Image.open": img("opened5", [12, 12]),
                "convert": img("rgba5", [12, 12])}))

# ── _flood ────────────────────────────────────────────────────────────────
CASES += [
    c("_flood", {"w": 1, "h": 1, "visited": [0], "seeds": [[0, 0]]},
      {"accept": seq(*bools(12))}),
    c("_flood", {"w": 2, "h": 1, "visited": [0, 0], "seeds": [[0, 0]]},
      {"accept": seq(*bools(12))}),
    c("_flood", {"w": 3, "h": 3, "visited": [0] * 9, "seeds": [[1, 1]]},
      {"accept": seq(*([True] * 20))}),
    c("_flood", {"w": 3, "h": 3, "visited": [1] * 9, "seeds": []},
      {"accept": seq(*bools(12))}),
    c("_flood", {"w": 3, "h": 2, "visited": [0] * 6, "seeds": [[0, 0], [2, 1]]},
      {"accept": seq(*bools(20))}),
    c("_flood", {"w": 2, "h": 2, "visited": [0] * 4, "seeds": [[0, 0]]},
      {"accept": raises("ValueError", "no")}),
]

# ── _border_flood ─────────────────────────────────────────────────────────
CASES += [
    c("_border_flood", {"w": 1, "h": 1, "visited": [0]},
      {"accept": seq(*bools(12))}),
    c("_border_flood", {"w": 2, "h": 2, "visited": [0] * 4},
      {"accept": seq(*bools(24))}),
    c("_border_flood", {"w": 3, "h": 2, "visited": [0] * 6},
      {"accept": seq(*bools(24))}),
    c("_border_flood", {"w": 3, "h": 3, "visited": [1] * 9},
      {"accept": seq(*bools(24))}),
    c("_border_flood", {"w": 2, "h": 3, "visited": [0] * 6},
      {"accept": seq(*([False] * 20))}),
    c("_border_flood", {"w": 2, "h": 2, "visited": [0] * 4},
      {"accept": raises("SystemExit", "halt")}),
]

# ── _unvisited_components ─────────────────────────────────────────────────
CASES += [
    c("_unvisited_components", {"w": 1, "h": 1, "visited": [0]},
      {"accept": seq(*bools(12))}),
    c("_unvisited_components", {"w": 1, "h": 1, "visited": [0]},
      {"accept": seq(False, *bools(10))}),
    c("_unvisited_components", {"w": 2, "h": 1, "visited": [0, 0]},
      {"accept": seq(*bools(16))}),
    c("_unvisited_components", {"w": 3, "h": 3, "visited": [1] * 9},
      {"accept": seq(*bools(12))}),
    c("_unvisited_components", {"w": 2, "h": 2, "visited": [0, 1, 1, 0]},
      {"accept": seq(*bools(20))}),
    c("_unvisited_components", {"w": 2, "h": 2, "visited": [0] * 4},
      {"accept": raises("KeyboardInterrupt", "int")}),
]

# ── _remove_masked ────────────────────────────────────────────────────────
for i in range(6):
    rgba = img("rgba%d" % i, [8 + i, 12 + i])
    mask = img("mask%d" % i, [8 + i, 12 + i])
    CASES.append(c("_remove_masked", {"rgba": rgba, "mask": mask}, {
        "Image.new": img("rmblank%d" % i, [8 + i, 12 + i]),
        "composite": img("rmout%d" % i, [8 + i, 12 + i]),
        "getchannel": img("rmch%d" % i, [8 + i, 12 + i]),
        "filter": img("rmflt%d" % i, [8 + i, 12 + i]),
        "putalpha": None,
    }))

# ── _thin_groups ──────────────────────────────────────────────────────────
CASES += [
    c("_thin_groups", {"indices": [1, 2, 3]}),
    c("_thin_groups", {"indices": [1, 2, 3, 4]}),
    c("_thin_groups", {"indices": [1, 2, 3, 4, 5]}),
    c("_thin_groups", {"indices": [1, 3, 4, 7, 8]}),
    c("_thin_groups", {"indices": []}),
    c("_thin_groups", {"indices": [0, 1, 5, 6, 7]}),
]

# ── _has_margin ───────────────────────────────────────────────────────────
CASES += [
    c("_has_margin", {"size": [192, 208], "box": [12, 16, 180, 192], "fx": 0.01, "fy": 0.015}),
    c("_has_margin", {"size": [100, 100], "box": [5, 5, 95, 95], "fx": 0.01, "fy": 0.015}),
    c("_has_margin", {"size": [100, 100], "box": [2, 2, 98, 98], "fx": 0.01, "fy": 0.015}),
    c("_has_margin", {"size": [100, 100], "box": [0, 0, 100, 100], "fx": 0.01, "fy": 0.015}),
    c("_has_margin", {"size": [192, 208], "box": [50, 50, 150, 150], "fx": 0.01, "fy": 0.015}),
    c("_has_margin", {"size": [192, 208], "box": [10, 10, 180, 190], "fx": 0.5, "fy": 0.5}),
]

# ── _is_multi_pose_outlier ────────────────────────────────────────────────
for w, h, mw, mh in [(300, 50, 60, 40), (100, 50, 60, 40), (200, 100, 60, 40),
                    (157, 40, 60, 50), (0, 0, 0, 0), (500, 20, 100, 80)]:
    CASES.append(c("_is_multi_pose_outlier",
                   {"width": w, "height": h, "med_w": mw, "med_h": mh}))

# ── _clean_slot ───────────────────────────────────────────────────────────
for i in range(6):
    CASES.append(c("_clean_slot", {"image": img("cs_in%d" % i, [10, 10])}, {
        "_isolate_slot_subject": img("cs_iso%d" % i, [10, 10]),
        "_drop_side_bleed": img("cs_out%d" % i, [10, 10]),
    }))

# ── _salvage_frames ───────────────────────────────────────────────────────
_clean = {"_isolate_slot_subject": img("sl_iso"), "_drop_side_bleed": img("sl_out")}
CASES += [
    c("_salvage_frames", {"strip": img("strip0", [10, 4]), "frame_count": 2},
      dict(_clean, **{"_frame_x_ranges": [[0, 5], [5, 10]],
                      "crop": seq(img("sl0a"), img("sl0b"))})),
    c("_salvage_frames", {"strip": img("strip1", [8, 4]), "frame_count": 2},
      dict(_clean, **{"_frame_x_ranges": seq(None, [[0, 8]]),
                      "_sever_expected_gutters": img("sev1", [8, 4]),
                      "crop": img("sl1")})),
    c("_salvage_frames", {"strip": img("strip2", [9, 4]), "frame_count": 3},
      {"_frame_x_ranges": seq(None, None),
       "_sever_expected_gutters": img("sev2", [9, 4]),
       "_slot_crops": [img("sc2a"), img("sc2b"), img("sc2c")]}),
    c("_salvage_frames", {"strip": img("strip3", [9, 4]), "frame_count": 3},
      {"_frame_x_ranges": seq(None, None),
       "_sever_expected_gutters": img("sev3", [9, 4]),
       "_slot_crops": None}),
    c("_salvage_frames", {"strip": img("strip4", [6, 5]), "frame_count": 1},
      dict(_clean, **{"_frame_x_ranges": [[1, 5]], "crop": img("sl4")})),
    c("_salvage_frames", {"strip": img("strip5", [6, 5]), "frame_count": 2},
      {"_frame_x_ranges": raises("ValueError", "bad")}),
]

# ── _check_atlas_cells ────────────────────────────────────────────────────
def atlas_io(bbox, hist, data=None):
    return {
        "crop": img("cell"),
        "getchannel": img("chA"),
        "histogram": hist,
        "getbbox": bbox,
        "tobytes": data if data is not None else [0] * 16,
    }


CASES += [
    c("_check_atlas_cells", {"atlas": img("atlas0", [1536, 1872])},
      atlas_io([10, 20, 50, 180], [0, 5])),
    c("_check_atlas_cells", {"atlas": img("atlas1", [1536, 1872])},
      atlas_io([10, 20, 50, 180], [0, 0])),
    c("_check_atlas_cells", {"atlas": img("atlas2", [1536, 1872])},
      atlas_io([10, 20, 50, 60], [0, 5])),
    c("_check_atlas_cells", {"atlas": img("atlas3", [1536, 1872])},
      atlas_io([10, 20, 30, 40], [0, 5])),
    c("_check_atlas_cells", {"atlas": img("atlas4", [1536, 1872])},
      atlas_io([10, 20, 50, 180], [0, 5], [10, 20, 30, 0] + [0] * 12)),
    c("_check_atlas_cells", {"atlas": img("atlas5", [1536, 1872])},
      atlas_io(None, [0, 5])),
]

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text("\n".join(json.dumps(r, ensure_ascii=False) for r in CASES) + "\n",
               encoding="utf-8")

from collections import Counter
counts = Counter(r["func"] for r in CASES)
print(f"wrote {len(CASES)} cases to {OUT}")
for f, n in counts.items():
    print(f"  {f}: {n}")
assert all(n >= 5 for n in counts.values()), "every function needs >=5 cases"
