#!/usr/bin/env python3
"""Oracle for gateway/profile_routing.py pure discriminators.

The harness emits four families (spec / botkey / coerce / whatsapp), each with
the inputs inline so the oracle replays the live python on the SAME values:
  spec     {g:c:t:u, ret}            -> ProfileRoute.specificity
  botkey   {in:<name|null>, out:<>}   -> _bot_profile_key
  coerce   {in:<json value>, out:<>} -> _coerce_route_id (json decoded)
  whatsapp {in:<chat|null>, ret:}    -> _is_whatsapp_non_user_chat
"""
import ast
import json
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/gateway/profile_routing.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"logger": types.SimpleNamespace(warning=lambda *a, **k: None)}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign):
            for t in n.targets:
                if isinstance(t, ast.Name) and t.id == "_WHATSAPP_NON_USER_SUFFIXES":
                    exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in ("_bot_profile_key",
                                                           "_coerce_route_id",
                                                           "_is_whatsapp_non_user_chat"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.ClassDef) and n.name == "ProfileRoute":
            # extract ONLY the specificity method
            spec = next((f for f in n.body if isinstance(f, ast.FunctionDef) and f.name == "specificity"), None)
            if spec:
                exec(compile(ast.Module(body=[spec], type_ignores=[]), SRC, "exec"), ns)

    def spec(g, c, t, u):
        obj = types.SimpleNamespace(guild_id=g, chat_id=c, thread_id=t, user_id=u)
        # specificity is @property; ast-extract leaves the bare function
        fn = ns["specificity"].fget if isinstance(ns["specificity"], property) else ns["specificity"]
        return fn(obj)

    botkey = ns["_bot_profile_key"]
    coerce_ = ns["_coerce_route_id"]
    whatsapp = ns["_is_whatsapp_non_user_chat"]

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
        print("ORACLE gateway_profile_routing: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        fam = c.get("family")
        if fam == "spec":
            total += 1
            want = spec(c["g"], c["c"], c["t"], c["u"])
            if c.get("ret") != want:
                mism += 1
                print(f"MISMATCH spec {c.get('case')} C={c.get('ret')!r} PY={want!r}")
        elif fam == "botkey":
            total += 1
            want = botkey(c.get("in"))
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH botkey {c.get('case')} in={c.get('in')!r} C={c.get('out')!r} PY={want!r}")
        elif fam == "coerce":
            total += 1
            val = c.get("in")  # already decoded by the outer json.loads of the line
            want = coerce_(val)
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH coerce {c.get('case')} in={c['in']!r} C={c.get('out')!r} PY={want!r}")
        elif fam == "whatsapp":
            total += 1
            want = whatsapp(c.get("in"))
            if c.get("ret") != want:
                mism += 1
                print(f"MISMATCH whatsapp {c.get('case')} in={c.get('in')!r} C={c.get('ret')!r} PY={want!r}")
    print(f"ORACLE gateway_profile_routing: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
