#!/usr/bin/env python3
"""
sta_oracle_learning_graph.py — Oracle for agent/learning_graph.py pure-transform
ports (build_edges, density_stats, _memory_skill_edges).

Reads the C harness JSON lines from stdin, recomputes each case against LIVE
Python, and compares structurally (Python edge/stat semantics are the truth).
"""
import sys
import os
import json

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import agent.learning_graph as LG  # noqa: E402
from agent.learning_graph import SkillNode  # noqa: E402


def norm(x):
    """Canonicalize: tuples -> lists, recurse; leave dicts/lists intact."""
    if isinstance(x, tuple):
        return [norm(v) for v in x]
    if isinstance(x, list):
        return [norm(v) for v in x]
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    return x


def node_from(d):
    return SkillNode(
        name=d["name"],
        category=d.get("category", "general"),
        use_count=int(d.get("use_count", 0) or 0),
        created_by=d.get("created_by"),
        related=list(d.get("related", [])),
    )


def main():
    data = sys.stdin.read()
    c_cases = {}
    for ln in data.splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            obj = json.loads(ln)
        except Exception:
            continue
        if "case" in obj:
            c_cases[obj["case"]] = obj.get("out")

    # ---- recompute Python truths ----
    py = {}

    # build_edges basic
    nodes1 = {
        "alpha": node_from({"name": "alpha", "related": ["beta", "gamma", "missing", "alpha"]}),
        "beta": node_from({"name": "beta", "related": ["alpha"]}),
        "gamma": node_from({"name": "gamma", "related": ["beta"]}),
        "delta": node_from({"name": "delta", "related": []}),
    }
    py["build_edges_basic"] = LG.build_edges(nodes1)
    py["build_edges_empty"] = LG.build_edges({})

    # density_stats basic
    dnodes_list = [
        {"name": "alpha", "category": "web", "use_count": 3, "created_by": "agent"},
        {"name": "beta", "category": "web", "use_count": 0, "created_by": "user"},
        {"name": "gamma", "category": "data", "use_count": 5, "created_by": "agent"},
        {"name": "delta", "category": "ops", "use_count": 0, "created_by": None},
    ]
    dnodes = {d["name"]: node_from(d) for d in dnodes_list}
    dedges = [("alpha", "beta"), ("beta", "gamma")]
    py["density_basic"] = LG.density_stats(dnodes, dedges)
    py["density_empty"] = LG.density_stats({}, [])

    # memory_skill_edges basic
    cards = [
        {"source": "memory", "title": "web scraping notes", "body": "alpha helps with scraping web pages"},
        {"source": "profile", "title": "data pipeline", "body": "gamma runs the data pipeline nightly"},
    ]
    mskills = [node_from({"name": n}) for n in ("alpha", "gamma", "scraping", "pipeline")]
    py["mem_edges_basic"] = LG._memory_skill_edges(cards, mskills)
    py["mem_edges_empty"] = LG._memory_skill_edges([], mskills)

    order = ["build_edges_basic", "build_edges_empty",
             "density_basic", "density_empty",
             "mem_edges_basic", "mem_edges_empty"]
    mism = 0
    for case in order:
        c_out = c_cases.get(case)
        if case not in c_cases:
            print(f"MISSING C case {case}")
            mism += 1
            continue
        py_out = norm(py[case])
        c_norm = norm(c_out)
        if c_norm == py_out:
            print(f"ok [{case}]")
        else:
            mism += 1
            print(f"MISMATCH [{case}]")
            print(f"  C ={json.dumps(c_norm)[:300]}")
            print(f"  PY={json.dumps(py_out)[:300]}")
    print(f"\nRESULT: {len(order)-mism}/{len(order)} match, {mism} mismatch")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
