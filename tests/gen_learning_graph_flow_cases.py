#!/usr/bin/env python3
"""Generate tests/fixtures/learning_graph_flow.jsonl.

Inputs only (no expected values): the oracle recomputes the live reference.
Scripted seam results live in in.io (see the harness header). Representation
scope: ASCII text; SkillNode doubles carry all nine fields; memory cards carry
{source,timestamp,title,body,fingerprint}; _tokenize results are token arrays
(live sets); no integral-float JSON literals / non-finite floats. _to_int_ts
domain: None/int/float/numeric-string/ISO (naive, Z, +HH:MM).
"""
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "learning_graph_flow.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_spec=None):
    s = {"__script__": True, "ret": ret}
    if raise_spec is not None:
        s["raise"] = raise_spec
    return s


def node(name, category="general", source="profile", timestamp=None, use_count=0,
         state="active", created_by=None, pinned=False, related=None):
    return {"name": name, "category": category, "source": source,
            "timestamp": timestamp, "use_count": use_count, "state": state,
            "created_by": created_by, "pinned": pinned,
            "related": related if related is not None else []}


def card(source="memory", timestamp=100, title="Title", body="Body", fingerprint="fp0"):
    return {"source": source, "timestamp": timestamp, "title": title,
            "body": body, "fingerprint": fingerprint}


def tok(text):
    return sorted({t for t in re.split(r"[^a-z0-9]+", text.lower()) if len(t) >= 3})


RAISE_EXC = {"type": "RuntimeError", "message": "boom"}
RAISE_KBI = {"type": "KeyboardInterrupt", "message": "interrupted"}
RAISE_OSE = {"type": "OSError", "message": "disk"}

# ── _fm_field ──────────────────────────────────────────────────────────────
add("_fm_field", fm={"name": "x"}, key="name")
add("_fm_field", fm={"metadata": {"hermes": {"category": "c"}}}, key="category")
add("_fm_field", fm={"name": ""}, key="name")
add("_fm_field", fm={"name": "x", "metadata": {"hermes": {"name": "y"}}}, key="name")
add("_fm_field", fm={"metadata": "string"}, key="name")
add("_fm_field", fm={"metadata": {"hermes": {"related_skills": ["a"]}}}, key="related_skills")
add("_fm_field", fm={}, key="name")
add("_fm_field", fm={"category": 0}, key="category")

# ── memory_fingerprint ─────────────────────────────────────────────────────
add("memory_fingerprint", entry="hello")
add("memory_fingerprint", entry="  hello  ")
add("memory_fingerprint", entry="")
add("memory_fingerprint", entry="a\nb")
add("memory_fingerprint", entry="Multiple words here")
add("memory_fingerprint", entry="\t\n")

# ── memory_node_id ─────────────────────────────────────────────────────────
add("memory_node_id", card={"source": "memory", "fingerprint": "abc123"}, index=0)
add("memory_node_id", card={"source": "profile", "fingerprint": "def"}, index=3)
add("memory_node_id", card={"fingerprint": "x"}, index=0)
add("memory_node_id", card={"source": "memory"}, index=1)
add("memory_node_id", card={"source": "memory", "fingerprint": "fp"}, index=12)
add("memory_node_id", card={}, index=0)

# ── _has_learning_signal ───────────────────────────────────────────────────
add("_has_learning_signal", node=node("a", created_by="agent"))
add("_has_learning_signal", node=node("a", created_by="learn"))
add("_has_learning_signal", node=node("a", use_count=5))
add("_has_learning_signal", node=node("a", source="external", created_by="agent", use_count=5))
add("_has_learning_signal", node=node("a"))
add("_has_learning_signal", node=node("a", source="base", created_by="agent"))
add("_has_learning_signal", node=node("a", created_by="user"))

# ── _related ───────────────────────────────────────────────────────────────
add("_related", fm={"related_skills": ["a", "b"]})
add("_related", fm={"related_skills": "a, b, c"})
add("_related", fm={"related_skills": "[a, b]"})
add("_related", fm={"related_skills": ["a", "", "  ", "b"]})
add("_related", fm={"metadata": {"hermes": {"related_skills": ["x"]}}})
add("_related", fm={"related_skills": None})
add("_related", fm={"related_skills": "a,,b,"})
add("_related", fm={"related_skills": ["  spaced  "]})

# ── _tokenize ──────────────────────────────────────────────────────────────
add("_tokenize", text="Hello World")
add("_tokenize", text="abc de fgh")
add("_tokenize", text="a-b-c")
add("_tokenize", text="foo123 bar!!baz")
add("_tokenize", text="")
add("_tokenize", text="AAA BBB aaa")
add("_tokenize", text="the quick brown fox")

# ── build_edges ────────────────────────────────────────────────────────────
add("build_edges", nodes={"a": node("a", related=["b"]), "b": node("b")})
add("build_edges", nodes={"a": node("a", related=["b"]), "b": node("b", related=["a"])})
add("build_edges", nodes={"a": node("a", related=["x"])})
add("build_edges", nodes={"a": node("a", related=["a"])})
add("build_edges", nodes={"b": node("b", related=["a"]), "a": node("a", related=["c"]), "c": node("c")})
add("build_edges", nodes={})
add("build_edges", nodes={"a": node("a", related=["b", "c"]), "b": node("b"), "c": node("c")})

# ── density_stats ──────────────────────────────────────────────────────────
add("density_stats", nodes={"a": node("a", category="x", created_by="agent", use_count=2)}, edges=[])
add("density_stats", nodes={"a": node("a", category="x"), "b": node("b", category="y")}, edges=[["a", "b"]])
add("density_stats", nodes={}, edges=[])
add("density_stats", nodes={"a": node("a", category="x"), "b": node("b", category="x"),
                            "c": node("c", category="y")}, edges=[["a", "b"]])
add("density_stats", nodes={chr(97 + i): node(chr(97 + i), category="c%d" % i) for i in range(10)}, edges=[])
add("density_stats", nodes={"a": node("a", category="x"), "b": node("b", category="y"),
                            "c": node("c", category="z")}, edges=[["a", "b"], ["b", "c"]])
# stable-sort tie regression: first-seen c0,c2,c1,c3; counts c3=3,c0=2,c2=2,c1=1
add("density_stats", nodes={("n%d" % i): node("n%d" % i, category=c) for i, c in
                            enumerate(["c0", "c2", "c1", "c3", "c3", "c2", "c3", "c0"])}, edges=[])

# ── _to_int_ts ─────────────────────────────────────────────────────────────
add("_to_int_ts", value=None)
add("_to_int_ts", value=1700000000)
add("_to_int_ts", value="1700000000")
add("_to_int_ts", value="1700000000.9")
add("_to_int_ts", value="2023-11-14T22:13:20Z")
add("_to_int_ts", value="2023-11-14")
add("_to_int_ts", value="abc")
add("_to_int_ts", value="")
add("_to_int_ts", value="   ")
add("_to_int_ts", value=1700000000.9)
add("_to_int_ts", value="2023-11-14T22:13:20+00:00")
add("_to_int_ts", value="2023-11-14T17:13:20-05:00")
add("_to_int_ts", value="1.7e9")
add("_to_int_ts", value=True)

# ── _memory_skill_edges (seams _tokenize: len(skills)+len(cards) calls) ────
def mse(skills, cards):
    queue = [tok(s["name"]) for s in skills]
    queue += [tok((c.get("title", "") + "\n" + c.get("body", "")).lower()) for c in cards]
    add("_memory_skill_edges", memory_cards=cards, skills=skills, io={"_tokenize": queue})

mse([node("alpha"), node("beta")],
    [card(title="alpha notes", body="alpha beta text", fingerprint="fp1")])
mse([node("zzz")], [card(title="abc", body="def", fingerprint="fp2")])
mse([node("al")], [card(title="alpha", body="", fingerprint="fp3")])
mse([node("alpha"), node("beta"), node("gamma"), node("delta"), node("epsilon")],
    [card(title="alpha beta gamma delta epsilon", body="all", fingerprint="fp4")])
mse([node("alpha")],
    [card(title="alpha one", body="x", fingerprint="f1"), card(title="alpha two", body="y", fingerprint="f2")])
mse([], [card(title="abc", body="def", fingerprint="fp5")])
mse([node("alpha")], [])

# ── build_learning_graph (seams: project_root_str, get_hermes_home,
#    build_skill_nodes, build_edges, _memory_cards, _memory_skill_edges,
#    density_stats) ─────────────────────────────────────────────────────────
def blg(nodes_map, edges, cards, mem_edges, stats, io_extra=None):
    io = {
        "project_root_str": "/repo",
        "get_hermes_home": "/home/u/.hermes",
        "build_skill_nodes": nodes_map,
        "build_edges": [edges],
        "_memory_cards": [cards],
        "_memory_skill_edges": [mem_edges],
        "density_stats": stats,
    }
    if io_extra:
        io.update(io_extra)
    add("build_learning_graph", io=io)

STATS1 = {"nodes": 1, "related_edges": 0, "edges_per_node": 0.0, "linked_nodes": 0,
          "isolated_pct": 100.0, "categories": 1, "agent_created": 1, "used": 0,
          "top_categories": [["cat1", 1]]}
blg({"alpha": node("alpha", category="cat1", created_by="agent"),
     "base_skill": node("base_skill", category="cat0", source="base", created_by="agent"),
     "unused": node("unused", category="cat2"),
     "ext": node("ext", category="cat3", source="external", created_by="agent")},
    [], [card(title="T", body="B", fingerprint="fp", timestamp=100)],
    [["memory:memory:0:fp", "alpha"]], STATS1)

STATS0 = {"nodes": 0, "related_edges": 0, "edges_per_node": 0.0, "linked_nodes": 0,
          "isolated_pct": 100.0, "categories": 0, "agent_created": 0, "used": 0,
          "top_categories": []}
blg({"base_skill": node("base_skill", source="base", created_by="agent")},
    [], [], [], STATS0)

STATS2 = {"nodes": 2, "related_edges": 1, "edges_per_node": 0.5, "linked_nodes": 2,
          "isolated_pct": 0.0, "categories": 1, "agent_created": 1, "used": 1,
          "top_categories": [["cat1", 2]]}
blg({"alpha": node("alpha", category="cat1", created_by="agent", use_count=3, related=["beta"]),
     "beta": node("beta", category="cat1", use_count=1)},
    [["alpha", "beta"]],
    [card(title="T1", body="B1", fingerprint="fa", timestamp=10),
     card(title="T2", body="B2", fingerprint="fb", timestamp=11, source="profile")],
    [["memory:memory:0:fa", "alpha"], ["memory:profile:1:fb", "beta"]], STATS2)

# seam raise: build_skill_nodes raises -> trace stops there
blg({}, [], [], [], STATS0, io_extra={"build_skill_nodes": script(raise_spec=RAISE_EXC)})
# BaseException passthrough: project_root_str raises KeyboardInterrupt
blg({}, [], [], [], STATS0, io_extra={"project_root_str": script(raise_spec=RAISE_KBI)})
# memory card missing fingerprint -> memory_node_id KeyError during graph_nodes
blg({"alpha": node("alpha", category="cat1", created_by="agent")},
    [], [{"source": "memory", "timestamp": 5, "title": "T", "body": "B"}],
    [], STATS1)
# cluster stable-sort tie: cat1=2, cat2=2, memory=2 -> first-seen [cat1,cat2,memory]
STATS4 = {"nodes": 4, "related_edges": 0, "edges_per_node": 0.0, "linked_nodes": 0,
          "isolated_pct": 100.0, "categories": 2, "agent_created": 4, "used": 0,
          "top_categories": [["cat1", 2], ["cat2", 2]]}
blg({"a1": node("a1", category="cat1", created_by="agent"),
     "a2": node("a2", category="cat1", created_by="agent"),
     "b1": node("b1", category="cat2", created_by="agent"),
     "b2": node("b2", category="cat2", created_by="agent")},
    [], [card(title="T1", fingerprint="f1"), card(title="T2", fingerprint="f2")],
    [], STATS4)

with open(OUT, "w", encoding="utf-8") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

# per-function counts
from collections import Counter
cnt = Counter(c["func"] for c in CASES)
print("wrote %d cases to %s" % (len(CASES), OUT))
for fn in sorted(cnt):
    print("  %-24s %d" % (fn, cnt[fn]))
assert all(v >= 5 for v in cnt.values()), "a function has <5 cases"
assert len(cnt) == 11, "expected 11 functions"