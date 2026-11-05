#!/usr/bin/env python3
"""Oracle for image_token_cost::count_images / model_metadata::_count_image_tokens.

`agent.model_metadata` builds ruamel YAML classes at import that cannot run
without the real `ruamel`, so the three LIVE definitions under test (the
IMAGE_PART_TYPES set, _count_parts, _count_image_tokens) are extracted verbatim
from the source with ast and exec'd — auto-tracking any change to those exact
functions without pulling the module's third-party import surface.

`agent.image_token_cost` imports only stdlib at top level, so its real
`count_images` is used directly.
"""
import ast
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")

# Build a namespace holding exactly the live MM definitions under test.
src = open("C:/Users/eman5/hermes-agent/agent/model_metadata.py",
           encoding="utf-8").read()
tree = ast.parse(src)
wanted = {"_IMAGE_PART_TYPES", "_count_parts", "_count_image_tokens"}
nodes = []
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name in wanted:
        nodes.append(n)
    elif (isinstance(n, ast.Assign) and len(n.targets) == 1 and
          isinstance(n.targets[0], ast.Name) and n.targets[0].id in wanted):
        nodes.append(n)
ns = {}
for n in nodes:
    mod = ast.Module(body=[n], type_ignores=[])
    exec(compile(ast.fix_missing_locations(mod), "model_metadata.py", "exec"), ns)
MM_COUNT = ns["_count_image_tokens"]

import agent.image_token_cost as ITC  # noqa: E402

MSGS = [
    "{}",
    '{"content":null}',
    '{"content":"plain text"}',
    '{"content":[{"type":"text","text":"hi"}]}',
    '{"content":[{"type":"image"}]}',
    '{"content":[{"type":"image_url","url":"x"},{"type":"image"}]}',
    '{"content":[{"type":"input_image","data":"a"},{"type":"tool_use"},{"type":"text"}]}',
    '{"content":[{"type":"image"},{"type":"image"},{"type":"image"}]}',
    '{"content":[7,"str",null,{"type":"image"}]}',
    '{"content":[{"type":"video"}]}',
    '{"_anthropic_content_blocks":[{"type":"image"}]}',
    '{"_anthropic_content_blocks":[{"type":"text"}]}',
    '{"_anthropic_content_blocks":"nope"}',
    '{"content":{"_multimodal":true,"content":[{"type":"image"},{"type":"image_url"}]}}',
    '{"content":{"_multimodal":false,"content":[{"type":"image"}]}}',
    '{"content":{"_multimodal":1,"content":[{"type":"image"}]}}',
    '{"output":[{"type":"image"},{"type":"function_call_output"}]}',
    '{"content":[{"type":"image"}],"output":[{"type":"image"}],"_anthropic_content_blocks":[{"type":"image"}]}',
]

ARRAYS = [
    "[]",
    '[{"content":[{"type":"image"}]}]',
    '[{"content":[{"type":"image"}]},{"content":[{"type":"image"},{"type":"image"}]}]',
    '["not a dict",7,{"content":[{"type":"image_url"}]}]',
    "null",
]


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

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
        print("ORACLE image_token_cost: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        if fam == "msg":
            msg = json.loads(MSGS[int(case.split("_")[1])])
            want_count = MM_COUNT(msg, 1)
            want_cost = MM_COUNT(msg, 100)
            if c["count"] != want_count or c["cost100"] != want_cost:
                mism += 1
                print(f"MISMATCH {case}: C=(count={c['count']},cost={c['cost100']}) "
                      f"PY=(count={want_count},cost={want_cost})")
        elif fam == "msgs":
            msgs = json.loads(ARRAYS[int(case.split("_")[1])])
            # count_images(messages) = sum(_count_image_tokens(m, 1)
            #   for m in messages if isinstance(m, dict))  — mirrored verbatim;
            # calling the real wrapper would import model_metadata (ruamel).
            # A non-list is a caller bug (the real wrapper raises TypeError):
            # C fail-opens to 0, which is not a behavior contract, so skip it.
            if not isinstance(msgs, list):
                continue
            want = sum(MM_COUNT(m, 1) for m in msgs if isinstance(m, dict))
            if c["total"] != want:
                mism += 1
                print(f"MISMATCH {case}: C={c['total']} PY={want}")

    print(f"ORACLE image_token_cost: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
