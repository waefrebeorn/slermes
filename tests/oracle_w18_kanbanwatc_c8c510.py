#!/usr/bin/env python3
"""Differential oracle for gateway/kanban_watchers_common.py (gate w18_kanbanwatc_c8c510)."""
import ast, json, sys, types, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "gateway" / "kanban_watchers_common.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w18_kanbanwatc_c8c510"


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "kwc_live"}
    import asyncio, contextlib, logging
    from pathlib import Path as PPath
    g.update(dict(asyncio=asyncio, contextlib=contextlib, logging=logging,
                  logger=logging.getLogger("oracle"), Path=PPath,
                  Any=object, Callable=object, Optional=object,
                  Context=object))
    wanted = {"_list_boards", "_board_slugs", "_positive_int_setting",
              "_resolve_auto_decompose_settings", "_gc_retention_days",
              "_kanban_dispatch_allowed"}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


def stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn); m.__path__ = []; sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items(): setattr(m, k, v)
    return m


def run_case(r):
    f = r.get("func")
    if f in ("list_boards", "board_slugs"):
        boards = r.get("boards") or []
        metadata = r.get("metadata")
        default = r.get("default_board", "")
        class KB:
            DEFAULT_BOARD = default
            def list_boards(self, include_archived):
                if r.get("list_raises"): raise RuntimeError("boom")
                return boards
            def read_board_metadata(self, name): return metadata
        if f == "list_boards":
            return G["_list_boards"](KB())
        return G["_board_slugs"](KB())
    if f == "positive_int":
        return G["_positive_int_setting"](r.get("cfg") or {}, r.get("key", ""))
    if f == "auto_decompose":
        cfg = r.get("cfg") or {}
        stub_module("hermes_cli.config", load_config=lambda: (_ for _ in ()).throw(RuntimeError("x")) if r.get("config_error") else cfg)
        v = G["_resolve_auto_decompose_settings"](lambda: (_ for _ in ()).throw(RuntimeError("x")) if r.get("config_error") else cfg)
        return {"enabled": bool(v[0]), "per_tick": v[1]}
    if f == "gc_retention":
        cfg = r.get("cfg") or {}
        def lc():
            if r.get("config_error"): raise RuntimeError("x")
            return cfg
        stub_module("hermes_cli.config", load_config=lc)
        return G["_gc_retention_days"]()
    if f == "dispatch_allowed":
        # model `from agent.estop import check_paused` failing (ImportError path)
        if r.get("import_error"):
            sys.modules["agent.estop"] = None
        else:
            stub_module("agent.estop", check_paused=lambda *a, **k: bool(r.get("paused")))
        return G["_kanban_dispatch_allowed"]()
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: r = json.loads(line)
        except ValueError: continue
        f = r.get("func")
        if f not in {"list_boards", "board_slugs", "positive_int", "auto_decompose",
                     "gc_retention", "dispatch_allowed"}: continue
        cases += 1
        try: py = run_case(r)
        except BaseException as e:
            mism += 1; print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr); continue
        got = r.get("out")
        if py != got:
            mism += 1; print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__": main()
