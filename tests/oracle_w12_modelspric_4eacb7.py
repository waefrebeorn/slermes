#!/usr/bin/env python3
"""Oracle for hermes_cli/models_pricing.py pure helpers."""
import ast, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/models_pricing.py"
source = open(SRC, encoding="utf-8").read()
tree = ast.parse(source)
wanted = ("_strip_v1", "_format_price_per_mtok", "_price_float", "_sale_pct",
          "compute_sale_discount", "_pricing_entry", "_per_token", "_catalog_items",
          "restrict_to_nous_policy")
body = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in wanted]
module = ast.fix_missing_locations(ast.Module(
    body=[ast.ImportFrom(module="__future__", names=[ast.alias(name="annotations")], level=0)] + body,
    type_ignores=[]))
ns = {"_NOUS_POLICY_APPEND_MAX": 64}
exec(compile(module, SRC, "exec"), ns)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func == "strip_v1":
        py = ns["_strip_v1"](r.get("url", ""))
    elif func == "format_price":
        py = ns["_format_price_per_mtok"](r.get("s", ""))
    elif func == "price_float":
        py = ns["_price_float"](r.get("raw"), positive=bool(r.get("positive")))
    elif func == "sale_pct":
        py = ns["_sale_pct"](r.get("current"), r.get("original"))
    elif func == "sale_discount":
        py = ns["compute_sale_discount"](r.get("prompt"), r.get("completion"), r.get("original"))
        if py is not None:
            py = list(py)
    elif func == "pricing_entry":
        py = ns["_pricing_entry"](r.get("pricing", {}), r.get("prompt_key", "prompt"), r.get("completion_key", "completion"))
    elif func == "per_token":
        py = ns["_per_token"](r.get("per_mtok"))
    elif func == "catalog_items":
        py = ns["_catalog_items"](r.get("payload", {}))
    elif func == "restrict":
        py = ns["restrict_to_nous_policy"](r.get("model_ids", []), set(r.get("allowed", [])),
                                          rescue_empty=bool(r.get("rescue_empty")))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} py={py!r} c={got!r} case={r}", file=sys.stderr)
print(f"ORACLE w12_modelspric_4eacb7: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
