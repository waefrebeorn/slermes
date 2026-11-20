#!/usr/bin/env python3
"""Generate tests/fixtures/config_defaults_flow.jsonl for the config_defaults_flow
FLOW oracle. Real defining modules: hermes_cli/config_defaults_env.py (OptionalEnvVars
+ _env/_base_url/_category.make) and hermes_cli/config_defaults_auxiliary.py (_aux);
config_defaults.py itself is a pure-data leaf (0 funcs). >=5 cases per ported function
(15 funcs), exercising decision branches, error paths, BaseException propagation,
_OMIT/present-null semantics, the lazy-fill state machine and fail-closed missing scripts.

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec; array = per-call queue (last repeats, empty -> None); single
value = constant; scripted null = None; {"__raise__": cls, "msg": ...} = exception.
A seam whose live result is itself a LIST is scripted one level deeper ([[...]]).
ASCII / POSIX-lexical domain; simple-ASCII dict keys (no quote/backslash) so KeyError
repr is predictable. OptionalEnvVars state = {data, fill, published, filling_active}.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "config_defaults_flow.jsonl")

CASES = []


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


def st(data=None, fill=None, published=None, active=False):
    return {"data": data if data is not None else {}, "fill": fill,
            "published": published, "filling_active": active}


def dm(func, state, seams=None, **extra):
    add(func, {"state": state, **extra}, seams)


# ── _aux (reasoning_effort default True / truthiness; extra appended) ──
add("aux", {"timeout": 120})
add("aux", {"timeout": 30, "reasoning_effort": False})
add("aux", {"timeout": 8, "reasoning_effort": None})
add("aux", {"timeout": 120, "reasoning_effort": True})
add("aux", {"timeout": 120, "extra": {"download_timeout": 30}})
add("aux", {"timeout": 120, "extra": {"no_progress_timeout": None}})
add("aux", {"timeout": 0, "reasoning_effort": 0})
add("aux", {"timeout": 120, "reasoning_effort": ""})
add("aux", {"timeout": 120, "reasoning_effort": "x"})
add("aux", {"timeout": 900, "extra": {"a": 1, "b": 2}})
add("aux", {"timeout": None, "reasoning_effort": 1})

# ── _env ({"description","prompt", **keys}) ──
add("env", {"description": "d", "prompt": "p"})
add("env", {"description": "d", "prompt": "p", "keys": {"url": "http://x"}})
add("env", {"description": "d", "prompt": "p", "keys": {"password": True, "category": "provider"}})
add("env", {"description": "d", "prompt": "p", "keys": {"tools": ["web_search"], "advanced": True}})
add("env", {"description": "d", "prompt": "p", "keys": {"url": None, "help": "h"}})
add("env", {"description": "", "prompt": "", "keys": {"a": 1}})

# ── _category.make (bound defaults + _OMIT/present-null + overrides) ──
B = {"category": "provider", "password": True, "advanced": True}
add("make", {"bound": B, "call": {"description": "d", "prompt": "p"}})
add("make", {"bound": B, "call": {"description": "d", "prompt": "p", "url": None}})
add("make", {"bound": B, "call": {"description": "d", "prompt": "p", "url": "http://x", "help": "h", "tools": ["t"]}})
add("make", {"bound": B, "call": {"description": "d", "prompt": "p", "password": False}})
add("make", {"bound": B, "call": {"description": "d", "prompt": "p", "password": None}})
add("make", {"bound": B, "call": {"description": "d", "prompt": "p", "advanced": False}})
add("make", {"bound": B, "call": {"description": "d", "prompt": "p", "advanced": None}})
add("make", {"bound": {"category": "messaging", "password": False, "advanced": False},
             "call": {"description": "d", "prompt": "p"}})
add("make", {"bound": {"category": "setting", "password": None, "advanced": False},
             "call": {"description": "d", "prompt": "p"}})
add("make", {"bound": B, "call": {"description": "d", "prompt": "p", "help": None, "url": None, "tools": None}})
add("make", {"bound": {"category": "tool", "password": False, "advanced": False},
             "call": {"description": "d", "prompt": "p", "password": True, "advanced": True, "url": "u"}})

# ── _base_url (f-strings + CLAIMED _prov closure seam) ──
ENTRY = {"description": "Nous base URL override", "prompt": "Nous base URL (leave empty for default)",
         "url": None, "password": False, "category": "provider", "advanced": True}
add("base_url", {"name": "Nous"}, {"prov": ENTRY})
add("base_url", {"name": "Gemini", "prompt_name": "Gemini Portal"}, {"prov": ENTRY})
add("base_url", {"name": "X", "prompt_name": None}, {"prov": ENTRY})
add("base_url", {"name": "X", "prompt_name": ""}, {"prov": ENTRY})
add("base_url", {"name": "Nous"}, {"prov": R("ValueError", "bad")})
add("base_url", {"name": "Nous"}, {"prov": R("SystemExit", "bye")})
add("base_url", {"name": "Nous"}, {})  # fail-closed
add("base_url", {"name": ""}, {"prov": ENTRY})

# ── OptionalEnvVars.arm (stores fill/published; returns None; no seam) ──
dm("arm", st(), fill="F", published="P")
dm("arm", st(), fill="F", published=None)
dm("arm", st(), fill=None, published="P")
dm("arm", st(), fill=None, published=None)
dm("arm", st({"a": 1}), fill="G", published="Q")
dm("arm", st({}, fill="OLD", published="OLDP"), fill="F", published=None)

# ── OptionalEnvVars._ready (the lazy-fill state machine) ──
dm("ready", st({"A": "1"}, fill=None))                       # not armed -> early return
dm("ready", st({"A": "1"}, fill="F", active=True))           # re-entrant -> early return
dm("ready", st({"A": "1"}, fill="F"), {"fill": {"B": "2"}})  # merge, no published
dm("ready", st({"A": "1"}, fill="F", published="P"), {"fill": {"B": "2"}, "published": None})
dm("ready", st({"A": "1"}, fill="F"), {"fill": R("ValueError", "bad")})
dm("ready", st({"A": "1"}, fill="F"), {"fill": R("SystemExit", "bye")})
dm("ready", st({"A": "1"}, fill="F"), {"fill": R("KeyboardInterrupt")})
dm("ready", st({"A": "1"}, fill="F", published="P"), {"fill": {"B": "2"}, "published": R("RuntimeError", "pub")})
dm("ready", st({"A": "1"}, fill="F", published="P"), {"fill": {"B": "2"}, "published": R("SystemExit", "bye")})
dm("ready", st({"A": "1"}, fill="F"), {"fill": None})        # entries None -> AttributeError
dm("ready", st({"A": "1"}, fill="F"), {"fill": [[1, 2]]})    # entries list -> AttributeError
dm("ready", st({"A": "1"}, fill="F"), {"fill": "x"})         # entries str -> AttributeError
dm("ready", st({"A": "1"}, fill="F"), {"fill": 5})           # entries int -> AttributeError
dm("ready", st({"A": "1"}, fill="F"), {"fill": True})        # entries bool -> AttributeError
dm("ready", st({"A": "1"}, fill="F"), {"fill": 5.5})         # entries float -> AttributeError
dm("ready", st({"A": "1"}, fill="F"), {"fill": {}})          # empty entries -> disarm, no merge
dm("ready", st({"A": "1"}, fill="F"), {})                    # fail-closed missing fill
dm("ready", st({"A": "1"}, fill="F"), {"fill": [{"B": "2"}, {"C": "3"}]})  # array queue (first)
dm("ready", st({"A": "1"}, fill="F"), {"fill": {"A": "9"}})  # overwrite existing key

# ── OptionalEnvVars.__getitem__ (_ready then dict.__getitem__; KeyError) ──
dm("getitem", st({"A": "1"}), key="A")
dm("getitem", st({"A": "1"}), key="Z")
dm("getitem", st({"A": "1"}, fill="F"), {"fill": {"B": "2"}}, key="B")
dm("getitem", st({"A": "1"}, fill="F"), {"fill": {"B": "2"}}, key="Z")
dm("getitem", st({"A": "1"}, fill="F"), {"fill": R("ValueError")}, key="A")
dm("getitem", st({"A": None}), key="A")
dm("getitem", st({"A": "1"}, fill="F", active=True), key="A")

# ── OptionalEnvVars.get (_ready then dict.get; default) ──
dm("get", st({"A": "1"}), key="A")
dm("get", st({"A": "1"}), key="Z")
dm("get", st({"A": "1"}), key="Z", default="dflt")
dm("get", st({"A": "1"}), key="Z", default=None)
dm("get", st({"A": "1"}), key="Z", default=0)
dm("get", st({"A": "1"}, fill="F"), {"fill": {"B": "2"}}, key="B")
dm("get", st({"A": None}), key="A", default="x")
dm("get", st({"A": "1"}, fill="F"), {"fill": R("OSError")}, key="A")

# ── OptionalEnvVars.__contains__ ──
dm("contains", st({"A": "1"}), key="A")
dm("contains", st({"A": "1"}), key="Z")
dm("contains", st({"A": "1"}, fill="F"), {"fill": {"B": "2"}}, key="B")
dm("contains", st({"A": "1"}, fill="F"), {"fill": {"B": "2"}}, key="Z")
dm("contains", st({"A": "1"}, fill="F"), {"fill": R("ValueError")}, key="A")
dm("contains", st({"A": None}), key="A")

# ── OptionalEnvVars.__iter__ (ordered key list) ──
dm("iter", st({"A": 1, "B": 2}))
dm("iter", st({}))
dm("iter", st({"A": 1}, fill="F"), {"fill": {"C": 3}})
dm("iter", st({"A": 1}, fill="F"), {"fill": R("ValueError")})
dm("iter", st({"B": 2, "A": 1}))
dm("iter", st({"A": 1, "B": 2}, fill="F"), {"fill": {"A": 9}})

# ── OptionalEnvVars.__len__ ──
dm("len", st({"A": 1, "B": 2}))
dm("len", st({}))
dm("len", st({"A": 1}, fill="F"), {"fill": {"C": 3}})
dm("len", st({"A": 1}, fill="F"), {"fill": R("ValueError")})
dm("len", st({"A": 1, "B": 2, "C": 3}))

# ── OptionalEnvVars.keys (ordered key list) ──
dm("keys", st({"A": 1, "B": 2}))
dm("keys", st({}))
dm("keys", st({"A": 1}, fill="F"), {"fill": {"C": 3}})
dm("keys", st({"A": 1}, fill="F"), {"fill": R("ValueError")})
dm("keys", st({"B": 2, "A": 1}))

# ── OptionalEnvVars.items ([key, value] pairs) ──
dm("items", st({"A": 1, "B": 2}))
dm("items", st({}))
dm("items", st({"A": None}))
dm("items", st({"A": 1}, fill="F"), {"fill": {"B": 2}})
dm("items", st({"A": 1}, fill="F"), {"fill": R("ValueError")})

# ── OptionalEnvVars.values (ordered value list) ──
dm("values", st({"A": 1, "B": 2}))
dm("values", st({}))
dm("values", st({"A": None, "B": 2}))
dm("values", st({"A": 1}, fill="F"), {"fill": {"B": 2}})
dm("values", st({"A": 1}, fill="F"), {"fill": R("ValueError")})

# ── OptionalEnvVars.copy (plain dict copy) ──
dm("copy", st({"A": 1, "B": 2}))
dm("copy", st({}))
dm("copy", st({"A": 1}, fill="F"), {"fill": {"B": 2}})
dm("copy", st({"A": 1}, fill="F"), {"fill": R("ValueError")})
dm("copy", st({"A": None}))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)
    print("per-func min count: %d" % min(counts.values()))


if __name__ == "__main__":
    main()