#!/usr/bin/env python3
"""Oracle for what is now agent/model_metadata.py:_count_image_tokens +
agent/image_token_cost.py:count_images (ported to include/port_image_token_cost.h,
object src/cli/port_image_token_cost.o). The live modules import cleanly; replay
directly.

Harness (now emit-input) families:
  msg:  {family:"msg", case, in:<msg json>, count:<per-image=1>, cost100:<per=100>}
  msgs: {family:"msgs", case, in:<array json>, total:<count_images sum>}
"""
import ast
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def main() -> int:
    # agent/model_metadata.py cannot be imported (hermes_yaml -> ruamel missing),
    # and image_token_cost.py inlines `from agent.model_metadata import
    # _count_image_tokens`. Ast-extract the needed defs + constant and expose the
    # extracted fn as a fake agent.model_metadata module so count_images works.
    md_src = open("C:/Users/eman5/hermes-agent/agent/model_metadata.py",
                  encoding="utf-8").read()
    mns = {}
    body = [n for n in ast.parse(md_src).body
            if (isinstance(n, ast.FunctionDef) and n.name in
                ("_count_image_tokens", "_count_parts"))
            or (isinstance(n, ast.Assign)
                and getattr(n.targets[0], "id", "") == "_IMAGE_PART_TYPES")]
    exec(compile(ast.Module(body=body, type_ignores=[]), "model_metadata", "exec"), mns)

    fake_md = types.ModuleType("agent.model_metadata")
    fake_md._count_image_tokens = mns["_count_image_tokens"]

    sys.path.insert(0, "C:/Users/eman5/hermes-agent")
    import agent  # real package; do NOT pre-shadow it with a bare fake

    sys.modules["agent.model_metadata"] = fake_md
    import agent.image_token_cost as itc

    def count_tokens(msg, cost):
        return mns["_count_image_tokens"](msg if isinstance(msg, dict) else None, cost)

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        if fam == "msg":
            msg = json.loads(c.get("in")) if isinstance(c.get("in"), str) else c.get("in")
            w1 = count_tokens(msg, 1)
            w100 = count_tokens(msg, 100)
            if w1 != c.get("count") or w100 != c.get("cost100"):
                mism += 1
                print(f"MISMATCH msg {c.get('case')} counted(C)={c.get('count')},{c.get('cost100')} "
                      f"PY={w1},{w100}")
        elif fam == "msgs":
            arr_raw = c.get("in")
            arr = json.loads(arr_raw) if isinstance(arr_raw, str) else arr_raw
            # live count_images has no None guard; the C port's NULL -> 0
            # convention maps "null" input to an empty sum.
            if arr is None:
                w = 0
            elif isinstance(arr, list):
                w = itc.count_images(arr)
            else:
                w = itc.count_images(arr)
            if w != c.get("total"):
                mism += 1
                print(f"MISMATCH msgs {c.get('case')} C={c.get('total')} PY={w}")
    if total == 0:
        print("ORACLE image_token_cost: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE image_token_cost: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
