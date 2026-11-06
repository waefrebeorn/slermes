#!/usr/bin/env python3
"""Oracle for agent/learning_graph.py pure-graph transforms.

Harness families (t_port_learning_graph.c), emitted per line:
  {case, in:[...input json...], out:<json>}
Replays the LIVE funcs on the decoded inputs:
  build_edges_{basic,empty}   -> build_edges(nodes_dict)   (in[0] = node list)
  density_{basic,empty}       -> density_stats(nodes_dict, edges) (in[1] = [[a,b],...])
  mem_edges_{basic,empty}     -> _memory_skill_edges(cards, skills)
Ast-extracts SkillNode + _tokenize + memory_fingerprint + memory_node_id +
Counter import + the three transforms. Cards arrive WITHOUT a fingerprint key;
live memory_node_id hard-requires it (memory:<source>:<idx>:<fp>), so the oracle
injects memory_fingerprint(body) per card exactly as live _memory_cards would.
"""
import ast
import hashlib
import json
import re
import sys
from collections import Counter, defaultdict
from dataclasses import field

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/learning_graph.py"
FUNS = ("build_edges", "density_stats", "_memory_skill_edges", "_tokenize",
        "memory_fingerprint", "memory_node_id")


def _canon(x):
    """Normalize JSON values for comparison: numbers compared numerically
    (libjson renders integral doubles like 25.0 as 25 — same value), strings
    and arrays kept structurally strict."""
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, list):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    if x is None:
        return ("null",)
    return ("other", repr(x))


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re, "hashlib": hashlib, "Counter": Counter,
          "field": field, "defaultdict": defaultdict, "Any": object,
          "dataclass": __import__("dataclasses").dataclass}
    body = []
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in FUNS:
            body.append(n)
        elif isinstance(n, ast.ClassDef) and n.name == "SkillNode":
            body.append(n)
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)
    SkillNode = ns["SkillNode"]
    build_edges = ns["build_edges"]
    density_stats = ns["density_stats"]
    mem_skill_edges = ns["_memory_skill_edges"]

    def skillnode(r):
        kw = {}
        for k in r:
            kw.setdefault(k, r[k])
        kw.setdefault("category", "")
        kw.setdefault("source", "profile")
        kw.setdefault("use_count", 0)
        kw.setdefault("created_by", None)
        kw.setdefault("related", [])
        return SkillNode(**kw)

    def nodes_from(rows):
        d = {}
        for r in rows or []:
            d[r["name"]] = skillnode(r)
        return d

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE learning_graph: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        name = c.get("case")
        ins = c.get("in") or []
        got = c.get("out")
        want = None
        if name.startswith("build_edges"):
            want = [list(e) for e in build_edges(nodes_from(ins[0] if len(ins) else []))]
        elif name.startswith("density"):
            nodes_d = nodes_from(ins[0] if len(ins) else [])
            edges = [tuple(e) for e in (ins[1] if len(ins) > 1 else [])]
            d = density_stats(nodes_d, edges)
            want = {k: v for k, v in d.items() if k != "top_categories"}
            want["top_categories"] = [list(x) for x in d["top_categories"]]
        elif name.startswith("mem_edges"):
            cards = [dict(cd) for cd in (ins[0] or [])]
            skills = [skillnode(r) for r in (ins[1] or [])]
            want = [list(e) for e in mem_skill_edges(cards, skills)]
        if want is None:
            mism += 1
            print(f"MISMATCH {name}: oracle produced no expected value")
            continue
        if _canon(want) != _canon(got):
            mism += 1
            print(f"MISMATCH {name}\n  C=  {json.dumps(got, sort_keys=True)}\n  PY= {json.dumps(want, sort_keys=True)}")
    print(f"ORACLE learning_graph: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
