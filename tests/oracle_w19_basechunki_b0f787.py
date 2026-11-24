#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/base_chunking.py (gate w19_basechunki_b0f787)."""
import ast, json, re, sys, types
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "gateway" / "platforms" / "base_chunking.py"
HELPERS = REPO.parent / "hermes-agent" / "gateway" / "platforms" / "helpers.py"
BASE = REPO.parent / "hermes-agent" / "gateway" / "platforms" / "base.py"
GATE = "w19_basechunki_b0f787"
WANTED_FUNCS = {"_open_link_bracket", "chunk_markdown_message"}
HELPER_FUNCS = {"fence_state_after", "table_header_to_repeat", "table_rollback_point",
                "table_row_start", "is_table_row", "_split_inside_table", "_table_header_above"}
HELPER_VALUES = {"TABLE_SEPARATOR_RE"}
BASE_FUNCS = {"_custom_unit_to_cp", "utf16_len"}


def load_funcs(path, funcs, values, g):
    tree = ast.parse(Path(path).read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in funcs:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         dont_inherit=True), g)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            names = {t.id for t in targets if isinstance(t, ast.Name)}
            if names & values:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                             dont_inherit=True), g)
    return g


H = load_funcs(HELPERS, HELPER_FUNCS, HELPER_VALUES,
               {"__name__": "helpers_live", "re": re, "Optional": object, "Callable": object})
BF = load_funcs(BASE, BASE_FUNCS, set(), {"__name__": "base_live"})


def stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


stub_module("gateway.platforms.helpers", **H)
stub_module("gateway.platforms.base", _custom_unit_to_cp=BF["_custom_unit_to_cp"],
            utf16_len=BF["utf16_len"])
stub_module("gateway.platforms")

G = {"__name__": "chunk_live", "Optional": object, "Callable": object, "list": list}
G.update({k: H[k] for k in ("fence_state_after", "table_header_to_repeat", "table_rollback_point",
                            "table_row_start")})
load_funcs(SRC, WANTED_FUNCS, set(), G)
utf16_len = BF["utf16_len"]


def run_case(r):
    f = r.get("func")
    if f == "chunk":
        len_fn = utf16_len if r.get("utf16") else None
        return list(G["chunk_markdown_message"](r.get("content") or "",
                                                int(r.get("max_length", 4096)), len_fn))
    if f == "open_link":
        return G["_open_link_bracket"](r.get("text") or "")
    if f == "fence_state":
        code, lang = H["fence_state_after"](r.get("text") or "", bool(r.get("in_code")),
                                            r.get("lang") or "")
        return {"in_code": code, "lang": lang}
    if f == "row_start":
        return H["table_row_start"](r.get("text") or "", int(r.get("split_at", 0)))
    if f == "rollback":
        text = r.get("text") or ""
        max_units = int(r.get("max_units", 0))
        len_fn = utf16_len if r.get("utf16") else len
        return H["table_rollback_point"](text, int(r.get("split_at", 0)),
                                         lambda t: len_fn(t) <= max_units)
    if f == "header_repeat":
        len_fn = utf16_len if r.get("utf16") else len
        return H["table_header_to_repeat"](r.get("chunk") or "", r.get("rest") or "",
                                           int(r.get("max_units", 0)), len_fn)
    return None


FUNCS = {"chunk", "open_link", "fence_state", "row_start", "rollback", "header_repeat"}


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
        if f not in FUNCS:
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
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
