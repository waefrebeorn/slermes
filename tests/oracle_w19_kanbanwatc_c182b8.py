#!/usr/bin/env python3
"""Differential oracle for gateway/kanban_watchers_dispatcher.py (gate w19_kanbanwatc_c182b8)."""
import ast, contextlib, dataclasses, json, sqlite3, sys, types, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = Path("C:/Users/eman5/hermes-agent")
if str(Q) not in sys.path:
    sys.path.insert(0, str(Q))
GATE = "w19_kanbanwatc_c182b8"
SRC = Q / "gateway" / "kanban_watchers_dispatcher.py"
_ANN = __future__.annotations.compiler_flag

import gateway.kanban_watchers_common as COMMON  # real seam: _positive_int_setting/_board_slugs/logger


class CapLog:
    def __init__(self):
        self.msgs = []

    def _rec(self, msg, *a, **k):
        try:
            self.msgs.append(msg % a if a else str(msg))
        except Exception:
            self.msgs.append(str(msg))

    info = warning = error = debug = exception = _rec


LOG = CapLog()


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    _mod = types.ModuleType("kbd_live")
    _mod.__path__ = []
    sys.modules["kbd_live"] = _mod
    g = _mod.__dict__
    g.update({
        "__name__": "kbd_live",
        "asdict": dataclasses.asdict,
        "dataclass": dataclasses.dataclass,
        "sqlite3": sqlite3,
        "contextlib": contextlib,
        "logger": LOG,
        "Path": Path,
        "Any": object,
        "Optional": object,
        "_positive_int_setting": COMMON._positive_int_setting,
        "_board_slugs": COMMON._board_slugs,
    })
    wanted_fn = {"_resolve_dispatcher_settings", "_log_spawn_results"}
    wanted_cls = {"_DispatcherSettings", "_KanbanDispatcher"}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted_fn:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in wanted_cls:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.Assign):
            try:
                value = ast.literal_eval(node.value)
            except Exception:
                continue
            for tgt in node.targets:
                if isinstance(tgt, ast.Name):
                    g[tgt.id] = value
    return g


G = extract()


class KanbanDbCorruptError(Exception):
    pass


def _kbc_mod():
    return types.SimpleNamespace(KanbanDbCorruptError=KanbanDbCorruptError)


def _kbd_mod():
    return types.SimpleNamespace(resolve_max_in_progress=lambda m: None)


G["_kbc"] = _kbc_mod
G["_kbd"] = _kbd_mod


def dispatcher_obj():
    cls = G["_KanbanDispatcher"]
    return cls.__new__(cls)


def run_case(r):
    f = r.get("func")
    LOG.msgs = []
    if f == "resolve_settings":
        cfg = r.get("kanban_cfg")
        if not isinstance(cfg, dict):
            cfg = {}
        eff = r.get("effective_max_in_progress")
        kbd = types.SimpleNamespace(resolve_max_in_progress=lambda m: eff)
        G["_kbd"] = lambda: kbd
        kb = types.SimpleNamespace(DEFAULT_FAILURE_LIMIT=r.get("default_failure_limit") or 0)
        settings = G["_resolve_dispatcher_settings"](cfg, kb)
        return dataclasses.asdict(settings)
    if f == "is_corrupt_error":
        if r.get("kanban_corrupt"):
            exc = KanbanDbCorruptError(r.get("msg") or "")
        elif r.get("sqlite_db_error"):
            exc = sqlite3.DatabaseError(r.get("msg") or "")
        else:
            exc = RuntimeError(r.get("msg") or "")
        obj = dispatcher_obj()
        return bool(obj.is_corrupt_board_db_error(exc))
    if f == "quarantine_lifted":
        slug = r.get("slug") or "b"
        fp = tuple(r.get("fingerprint") or ())
        obj = dispatcher_obj()
        obj.disabled_corrupt_boards = {}
        if r.get("has_entry"):
            obj.disabled_corrupt_boards[slug] = (tuple(r.get("disabled_fp") or ()),
                                                float(r.get("disabled_at") or 0))
        G["time"] = types.SimpleNamespace(monotonic=lambda: float(r.get("now_mono") or 0))
        lifted = obj._quarantine_lifted(slug, fp)
        joined = " | ".join(LOG.msgs)
        if "fingerprint unchanged" in joined:
            log = "unchanged"
        elif "database changed" in joined:
            log = "changed"
        else:
            log = "none"
        return {"lifted": bool(lifted), "log": log,
                "entry_after": slug in obj.disabled_corrupt_boards}
    if f == "log_spawn_results":
        raw = r.get("results") or []
        pairs = []
        for item in raw:
            slug, res = item[0], item[1]
            if isinstance(res, dict):
                base = {"reclaimed": 0, "promoted": 0, "crashed": [], "timed_out": [],
                        "auto_blocked": []}
                base.update(res)
                res = types.SimpleNamespace(**base)
            pairs.append((slug, res))
        return {"any_spawned": bool(G["_log_spawn_results"](pairs))}
    if f == "board_slugs":
        return list(COMMON._board_slugs(types.SimpleNamespace(
            list_boards=lambda include_archived: r.get("boards") or [],
            DEFAULT_BOARD=r.get("default_board") or "")))
    return None


def main():
    cases = mism = 0
    known = {"resolve_settings", "is_corrupt_error", "quarantine_lifted",
             "log_spawn_results", "board_slugs"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in known:
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
