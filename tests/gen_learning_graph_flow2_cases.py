#!/usr/bin/env python3
"""Generate tests/fixtures/learning_graph_flow2.jsonl.

Inputs only (no expected values): the oracle recomputes the LIVE reference from
the extracted agent/learning_graph_render.py defs. Scripted seam results live in
in.io (see the harness header). Representation scope: ASCII labels; node doubles
carry id/kind/label/category/timestamp/useCount/pinned/memorySource; buckets are
{label,ts,rec,nodes}; memory_lookup is an id->card map; format_date is a seam.
Avoid integral-float-only literals where a str() distinction would matter and
non-finite floats.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "learning_graph_flow2.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def nd(id="a", kind="skill", label=None, category=None, timestamp=None,
       useCount=0, pinned=False, memorySource=None):
    d = {"id": id, "kind": kind}
    if label is not None:
        d["label"] = label
    if category is not None:
        d["category"] = category
    if timestamp is not None:
        d["timestamp"] = timestamp
    if useCount:
        d["useCount"] = useCount
    if pinned:
        d["pinned"] = True
    if memorySource is not None:
        d["memorySource"] = memorySource
    return d


def bucket(label="#1", ts=100.0, nodes=None):
    return {"label": label, "ts": ts, "rec": 1.0, "nodes": nodes or []}


def script(ret=None, raise_spec=None):
    s = {"__script__": True, "ret": ret}
    if raise_spec is not None:
        s["raise"] = raise_spec
    return s


DATE = "14 Nov 2023"
RAISE_VAL = {"type": "ValueError", "message": "bad date"}
RAISE_KBI = {"type": "KeyboardInterrupt", "message": "halt"}

# ── _lerp ────────────────────────────────────────────────────────────────
add("_lerp", a=0.0, b=1.0, t=0.5)
add("_lerp", a=2.0, b=4.0, t=0.25)
add("_lerp", a=10.0, b=0.0, t=0.3)
add("_lerp", a=-1.0, b=1.0, t=1.0)
add("_lerp", a=5.0, b=5.0, t=0.9)
add("_lerp", a=0.0, b=3.0, t=0.1)

# ── _is_memory ───────────────────────────────────────────────────────────
add("_is_memory", node=nd("a", kind="memory"))
add("_is_memory", node=nd("a", kind="skill"))
add("_is_memory", node={"id": "a"})
add("_is_memory", node=nd("a", kind="MEMORY"))
add("_is_memory", node=nd("a", kind="memory", label="x"))
add("_is_memory", node={})

# ── _node_id ─────────────────────────────────────────────────────────────
add("_node_id", node=nd("abc"))
add("_node_id", node={"id": ""})
add("_node_id", node={})
add("_node_id", node={"id": None})
add("_node_id", node=nd("memory:memory:0:fp", kind="memory"))
add("_node_id", node=nd("x y"))

# ── _utc ─────────────────────────────────────────────────────────────────
for ts in (0.0, 1700000000.0, 1000000000.0, 86400.0, -1.0, 1609459200.0, 1700000000.75):
    add("_utc", ts=ts)

# ── _lead_in ─────────────────────────────────────────────────────────────
for r in (0.0, 0.5, 1.0, 0.25, 0.75, 2.0):
    add("_lead_in", ratio=r)

# ── _visible_count ───────────────────────────────────────────────────────
add("_visible_count", reveal=0.0, n=10)
add("_visible_count", reveal=1.0, n=10)
add("_visible_count", reveal=0.5, n=5)
add("_visible_count", reveal=0.33, n=10)
add("_visible_count", reveal=1.5, n=4)
add("_visible_count", reveal=-0.2, n=7)

# ── _node_raw_label ──────────────────────────────────────────────────────
add("_node_raw_label", node=nd("id1", label="  Hello  "))
add("_node_raw_label", node=nd("id1"))
add("_node_raw_label", node={"id": "fallback"})
add("_node_raw_label", node={})
add("_node_raw_label", node=nd("id1", label=""))
add("_node_raw_label", node=nd("id1", label="\t spaced \n"))

# ── _node_ts ─────────────────────────────────────────────────────────────
add("_node_ts", node=nd("a", timestamp=1700000000.5))
add("_node_ts", node=nd("a", timestamp="1700000000.25"))
add("_node_ts", node=nd("a", timestamp=None))
add("_node_ts", node=nd("a"))
add("_node_ts", node=nd("a", timestamp="not-a-number"))
add("_node_ts", node=nd("a", timestamp=True))
add("_node_ts", node=nd("a", timestamp="  12.5  "))

# ── _node_card (format_date seam) ────────────────────────────────────────
add("_node_card", node=nd("a", category="coding"), io={"format_date": DATE})
add("_node_card", node=nd("m", kind="memory"), io={"format_date": DATE})
add("_node_card", node=nd("p", kind="memory", memorySource="profile"), io={"format_date": DATE})
add("_node_card", node=nd("a", category="cat", useCount=5, pinned=True), io={"format_date": DATE})
add("_node_card", node=nd("a", label="x" * 30), io={"format_date": DATE})
add("_node_card", node=nd("a"), io={"format_date": script(raise_spec=RAISE_VAL)})
add("_node_card", node=nd("a"), io={"format_date": script(raise_spec=RAISE_KBI)})

# ── _skill_category_counts ───────────────────────────────────────────────
add("_skill_category_counts", nodes=[nd("a", category="coding"), nd("b", category="coding"), nd("c", category="music")])
add("_skill_category_counts", nodes=[nd("a"), nd("b")])
add("_skill_category_counts", nodes=[nd("a", kind="memory", category="coding"), nd("b", category="music")])
add("_skill_category_counts", nodes=[])
add("_skill_category_counts", nodes=[nd("a", category="c1"), nd("b", category="c2"), nd("c", category="c3")])
add("_skill_category_counts", nodes=[nd("a", category=""), nd("b", category="x")])

# ── category (ChartBucket method) ────────────────────────────────────────
add("category", bucket=bucket(nodes=[nd("a", category="coding"), nd("b", category="coding"), nd("c", category="music")]))
add("category", bucket=bucket(nodes=[nd("a", category="music"), nd("b", category="coding"), nd("c", category="music")]))
add("category", bucket=bucket(nodes=[nd("a", kind="memory", category="coding"), nd("b", category="music")]))
add("category", bucket=bucket(nodes=[nd("a", kind="memory")]))
add("category", bucket=bucket(nodes=[]))
add("category", bucket=bucket(nodes=[nd("a"), nd("b", category="music")]))

# ── _period ──────────────────────────────────────────────────────────────
for ts, g in ((1700000000.0, "day"), (1700000000.0, "month"), (1700000000.0, "year"),
              (0.0, "day"), (1609459200.0, "month"), (1700000000.0, "week")):
    add("_period", ts=ts, granularity=g)

# ── _fill_even_bins (mutates buckets -> state) ───────────────────────────
add("_fill_even_bins",
    buckets=[bucket("#1", 0, []), bucket("#2", 1, []), bucket("#3", 2, [])],
    nodes=[nd("a"), nd("b"), nd("c"), nd("d")],
    rec={"rec": {"a": 0.0, "b": 0.5, "c": 0.9, "d": 1.0}, "timed": True})
add("_fill_even_bins",
    buckets=[bucket("#1", 0, []), bucket("#2", 1, [])],
    nodes=[nd("a"), nd("b")],
    rec={"rec": {"a": 0.0, "b": 1.0}, "timed": True})
add("_fill_even_bins",
    buckets=[bucket("#1", 0, [])],
    nodes=[nd("a"), nd("b")],
    rec={"rec": {}, "timed": False})
add("_fill_even_bins",
    buckets=[bucket("#1", 0, []), bucket("#2", 1, []), bucket("#3", 2, []), bucket("#4", 3, [])],
    nodes=[nd("a"), nd("b"), nd("c")],
    rec={"rec": {"a": 0.1, "b": 0.4, "c": 0.6}, "timed": True})
add("_fill_even_bins",
    buckets=[bucket("#1", 0, [nd("seed")]), bucket("#2", 1, [])],
    nodes=[nd("x")],
    rec={"rec": {"x": 0.99}, "timed": True})

# ── node_row (format_date seam) ──────────────────────────────────────────
LOOK = {"a": {"body": "alpha body"}, "m": {"body": "memory text"}}
add("node_row", node=nd("a", category="coding", label="Alpha"), memory_lookup=LOOK, io={"format_date": DATE})
add("node_row", node=nd("m", kind="memory", label="Memo"), memory_lookup=LOOK, io={"format_date": DATE})
add("node_row", node=nd("zz", label="No memo"), memory_lookup=LOOK, io={"format_date": DATE})
add("node_row", node=nd("a", label="x" * 30), memory_lookup=LOOK, io={"format_date": DATE})
add("node_row", node=nd("a"), memory_lookup={}, io={"format_date": DATE})
add("node_row", node=nd("a"), memory_lookup=LOOK, io={"format_date": script(raise_spec=RAISE_VAL)})

# ── bucket_row (format_date seam: bucket date, then node dates in sort order) ──
add("bucket_row", idx=0,
    bucket=bucket("14 Nov", 1700000000.0, [nd("n1", timestamp=50.0, category="coding"),
                                           nd("n2", timestamp=200.0, category="music")]),
    cmap={"coding": "#FF0000", "music": "#00FF00"}, memory_lookup=LOOK,
    io={"format_date": [DATE, "1 Jan 2020", "2 Feb 2021"]})
add("bucket_row", idx=2,
    bucket=bucket("15 Nov", 1700000008.0, [nd("m", kind="memory", timestamp=100.0)]),
    cmap={}, memory_lookup=LOOK,
    io={"format_date": [DATE, DATE]})
add("bucket_row", idx=1,
    bucket=bucket("empty", 1700000000.0, []),
    cmap={"coding": "#FF0000"}, memory_lookup=LOOK,
    io={"format_date": [DATE]})
add("bucket_row", idx=0,
    bucket=bucket("14 Nov", 1700000000.0, [nd("a", category="coding", timestamp=100.0)]),
    cmap={"coding": "#FF0000"}, memory_lookup={},
    io={"format_date": [DATE, DATE]})
add("bucket_row", idx=3,
    bucket=bucket("16 Nov", 1700000016.0, [nd("x", category="z", timestamp=10.0),
                                           nd("y", category="a", timestamp=20.0)]),
    cmap={"a": "#ABCDEF"}, memory_lookup=LOOK,
    io={"format_date": [DATE, DATE, DATE]})

# ── _bar_lengths ─────────────────────────────────────────────────────────
add("_bar_lengths", bucket=bucket(nodes=[nd("a"), nd("b"), nd("c")]), max_total=6, bar_w=20)
add("_bar_lengths", bucket=bucket(nodes=[]), max_total=4, bar_w=12)
add("_bar_lengths", bucket=bucket(nodes=[nd("a", kind="memory"), nd("b", kind="memory")]), max_total=4, bar_w=10)
add("_bar_lengths", bucket=bucket(nodes=[nd("a"), nd("b"), nd("c")]), max_total=6, bar_w=7)
add("_bar_lengths", bucket=bucket(nodes=[nd("a"), nd("b", kind="memory"), nd("c", kind="memory")]), max_total=5, bar_w=9)
add("_bar_lengths", bucket=bucket(nodes=[nd("a")]), max_total=1, bar_w=14)

with open(OUT, "w", encoding="utf-8") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

from collections import Counter
cnt = Counter(c["func"] for c in CASES)
print("wrote %d cases to %s" % (len(CASES), OUT))
for fn in sorted(cnt):
    print("  %-24s %d" % (fn, cnt[fn]))
assert all(v >= 5 for v in cnt.values()), "a function has <5 cases"
assert len(cnt) == 16, "expected 16 functions"
