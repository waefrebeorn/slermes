#!/usr/bin/env python3
"""Differential oracle for hermes_cli/model_setup_flows_bedrock.py (gate w19_modelsetup_405db7)."""
import ast, json, sys, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "hermes_cli" / "model_setup_flows_bedrock.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_modelsetup_405db7"
WANTED = {"bedrock_region_geo_prefix", "bedrock_model_routable_from_region", "_bedrock_text_model_ids"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "bedrock_live", "__builtins__": __builtins__}
    for node in tree.body:
        # module-level constants the functions close over, in source order
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             flags=_ANN, dont_inherit=True), g)
            except Exception:
                pass
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


def run_case(r):
    f = r.get("func")
    if f == "geo_prefix":
        return G["bedrock_region_geo_prefix"](r.get("region_name"))
    if f == "routable":
        return bool(G["bedrock_model_routable_from_region"](r.get("model_id"), r.get("region_name")))
    if f == "text_model_ids":
        return G["_bedrock_text_model_ids"](r.get("models") or [], r.get("region"))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in {"geo_prefix", "routable", "text_model_ids"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s args=%r py=%r c=%r" % (f, r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or not cases) else 0)


if __name__ == "__main__":
    main()
