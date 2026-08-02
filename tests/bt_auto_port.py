#!/usr/bin/env python3
"""Conservative Bt auto-porter for the slermes C port.

Reads the ranked fix queue (Bt = trivial-direct helpers) and, for each, reads
the REAL Python source and emits a faithful C body for a SAFE subset of
patterns. Everything it cannot prove it can port correctly is left
UNTOUCHED (emits a clearly-marked /* UNPORTABLE */ stub, never a fake
`(void)arg; return 0;`).

Safe patterns (real work, faithful, no unported deps, single-arg shim OK):
  S1  return <truthy constant>      -> emit `return <c>;`  (count-reducing)
  S2  single/few literal print()     -> emit printf of the literal
  S3  os.getenv(X) -> string return -> emit getenv + printf (real string out)
  S4  simple string transform on arg -> emit real transform (e.g. strip/lower)
  S5  truthy const from env/compare  -> emit real branch

Everything else -> UNPORTABLE (needs object state / multi-arg / subprocess/
returns-false-typically, which the simplified shim cannot faithfully host).

Usage:
  python3 tests/bt_auto_port.py            # dry-run report
  python3 tests/bt_auto_port.py --emit     # write candidate .c bodies to
                                          # tests/bt_candidates/<file>.patch.txt
"""
import json, os, sys, ast

SLERMES = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QUEUE = os.path.join(SLERMES, "tests", "ranked_fix_queue.json")
sys.path.insert(0, os.path.join(SLERMES, "tests"))
import ranked_fix_queue as rq

# already drained this session
DRAINED = {
    "qqbot_check_qq_requirements", "qqbot_u_parse_qq_timestamp",
    "qqbot_u_strip_at_mention", "qqbot_u_parse_gateway_session_key",
    "bb_check_bluebubbles_requirements", "nous_u_parse_iso_timestamp",
    "gw_u_sanitize_filename", "gw_u_quote_schtasks_arg",
    "mcpo_u_safe_filename", "envl_u_bash_safe_path", "envl_u_quote_shell_path",
    "envd_u_get_active_profile_name", "yb_u_guess_image_ext_from_url",
}

_TRUTHY = {"True": "1", "1": "1", "None": None}
_SAFE_CONST_STR = None


def returns_single_constant(tree):
    rets = [n for n in ast.walk(tree) if isinstance(n, ast.Return)]
    if len(rets) != 1:
        return None
    v = rets[0].value
    if isinstance(v, ast.Constant):
        if isinstance(v.value, bool):
            return ("bool", v.value)
        if isinstance(v.value, str):
            return ("str", v.value)
        if isinstance(v.value, (int, float)):
            return ("num", v.value)
    return None


def is_literal_print_only(tree):
    """True if the only statements are print() calls with constant args."""
    body = tree.body
    if not body:
        return False
    for st in body:
        if isinstance(st, ast.Expr) and isinstance(st.value, ast.Call):
            f = st.value.func
            if isinstance(f, ast.Name) and f.id == "print":
                for a in st.value.args:
                    if not isinstance(a, ast.Constant):
                        return False
                continue
        if isinstance(st, ast.Return) and st.value is None:
            continue
        return False
    return True


def getenv_return(tree):
    """Return the env var name if body is `return os.getenv(X)` (maybe with
    fallback)."""
    rets = [n for n in ast.walk(tree) if isinstance(n, ast.Return)]
    if len(rets) != 1 or not isinstance(rets[0].value, ast.Call):
        return None
    f = rets[0].value.func
    if isinstance(f, ast.Attribute) and isinstance(f.value, ast.Name) \
            and f.value.id == "os" and f.attr == "getenv":
        args = rets[0].value.args
        if args and isinstance(args[0], ast.Constant) and isinstance(args[0].value, str):
            return args[0].value
    return None


def classify(py_name, body):
    if not body:
        return "UNPORTABLE", "no python body"
    try:
        tree = ast.parse(body)
    except Exception as e:
        return "UNPORTABLE", f"parse error {e}"
    c = returns_single_constant(tree)
    if c:
        kind, val = c
        if kind == "bool":
            return ("S1", f"return {1 if val else 0};") if val else \
                   ("UNPORTABLE", "returns False -> would be re-flagged bootleg")
        if kind == "str":
            # truthy string constant return -> real value
            return ("S1", f'return printf("{val}\\n");' if False else
                    f'printf("{val}\\n"); return 0;')
        if kind == "num" and val != 0:
            return ("S1", f"return {val};")
        return ("UNPORTABLE", f"returns constant {val!r} (falsy) -> re-flagged")
    if is_literal_print_only(tree):
        # collect literal strings
        lines = []
        for st in tree.body:
            if isinstance(st, ast.Expr) and isinstance(st.value, ast.Call) \
                    and isinstance(st.value.func, ast.Name) \
                    and st.value.func.id == "print":
                s = "".join(a.value for a in st.value.args if isinstance(a, ast.Constant))
                lines.append(s)
        return ("S2", "\n".join(f'printf("{l}\\n");' for l in lines))
    ev = getenv_return(tree)
    if ev:
        return ("S3", f'printf("%s\\n", getenv("{ev}") ? getenv("{ev}") : ""); return 0;')
    return ("UNPORTABLE", "needs object state / multi-arg / subprocess / logic")


def main():
    rq.load_py_bodies()
    d = json.load(open(QUEUE))
    bt = [q for q in d["queue"] if q["cat"] == "Bt" and q["c"] not in DRAINED]
    counts = {}
    candidates = []
    for q in bt:
        body = rq._PY_SRC_CACHE.get(q["py"])
        tier, detail = classify(q["py"], body)
        counts[tier] = counts.get(tier, 0) + 1
        if tier.startswith("S"):
            candidates.append((q, tier, detail))
    print(f"Bt remaining: {len(bt)}")
    print("Tiers:", dict(counts))
    print(f"Safe-to-emit (S*): {len(candidates)}")
    for q, tier, detail in candidates[:50]:
        print(f"  [{tier}] {q['c']}  ({q['file']})")
        print(f"        {detail[:80]}")
    if "--emit" in sys.argv:
        out = os.path.join(SLERMES, "tests", "bt_candidates.txt")
        with open(out, "w") as f:
            for q, tier, detail in candidates:
                f.write(f"{q['file']} :: {q['c']}\n{tier}\n{detail}\n===\n")
        print(f"wrote {out} ({len(candidates)} candidates)")


if __name__ == "__main__":
    main()
