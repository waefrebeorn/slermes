#!/usr/bin/env python3
"""gen_kanban_db_flow2_cases.py — companion generator for the kanban_db
UNCLAIMED-remainder FLOW port #2 (src/cli/kanban_db_flow2.c).

Emits tests/fixtures/kanban_db_flow2.jsonl — the scripted-seam case file the
runner auto-feeds to tests/t_port_kanban_db_flow2.c (>=5 cases per ported
func; 7 funcs). Every script shape mirrors the oracle's recording stubs:
{"ret": v} / {"raise": {"type","message"}} / a per-call queue LIST;
"import:hermes_cli.projects_db" gates carry {"import_raise": {...}}
(None-in-sys.modules halt, message byte-pinned to CPython).

Domain guards (documented in the port header):
  - slug/project_id scalars: str | null | int | bool | NON-integral float
    (integral floats lose identity in the libjson parse; repr(5.0)!="5").
  - ASCII slug error-message scope (repr passes non-ASCII > U+00FF raw on
    both sides; U+2028/2029 are repr-escaped live and OUT of scope).
  - bytes chunks: canonical padded base64 doubles; the read queue scripts
    the terminating empty chunk explicitly (exhaustion fails closed).
  - cap: per-case injection (default byte-pinned 26214400) so the
    growth-abort branch runs without 25 MB payloads.
  - only table-known exception kinds are scripted (the C hierarchy table
    fails closed on unknowns).

--check-const verifies KD2_ATTACHMENT_MAX_BYTES / KD2_COPY_CHUNK against the
LIVE kanban_db.py AST. --check-pop verifies every PoP annotation names a real
live top-level def.

Usage: python tests/gen_kanban_db_flow2_cases.py [--check-const] [--check-pop]
"""
import ast
import base64
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
QUARRY = REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "kanban_db.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "kanban_db.py"
PORT = REPO / "src" / "cli" / "kanban_db_flow2.c"
OUT = HERE / "fixtures" / "kanban_db_flow2.jsonl"

CASES = []


def case(func, inp=None, io=None):
    CASES.append({"func": func, "in": {**(inp or {}), "io": io or {}}})


def ret(v):
    return {"ret": v}


def raise_(t, m=""):
    return {"raise": {"type": t, "message": m}}


def b64(b):
    return {"__bytes__": base64.b64encode(b).decode("ascii")}


def halt(mod):
    return {"import_raise": {"type": "ModuleNotFoundError",
                             "message": f"import of {mod} halted; None in sys.modules"}}


PIN_IO = {"pin_first.set": ret("tok-1"), "pin_first.reset": ret(None)}

# ── pin_first_board_resolution ───────────────────────────────────────────────
case("pin_first_board_resolution", {"body": None}, dict(PIN_IO))
case("pin_first_board_resolution", {"body": {"raise": {"type": "ValueError", "message": "body"}}}, dict(PIN_IO))
case("pin_first_board_resolution", {"body": {"raise": {"type": "SystemExit", "message": "sx"}}}, dict(PIN_IO))
case("pin_first_board_resolution", {"body": {"raise": {"type": "KeyboardInterrupt", "message": "ki"}}}, dict(PIN_IO))
case("pin_first_board_resolution", {"body": {"raise": {"type": "GeneratorExit", "message": ""}}}, dict(PIN_IO))
case("pin_first_board_resolution", {"body": None},
     {"pin_first.set": raise_("OSError", "set")})
case("pin_first_board_resolution", {"body": None},
     {"pin_first.set": ret("tok-1"), "pin_first.reset": raise_("RuntimeError", "reset")})
case("pin_first_board_resolution", {"body": {"raise": {"type": "ValueError", "message": "body"}}},
     {"pin_first.set": ret("tok-1"), "pin_first.reset": raise_("KeyboardInterrupt", "ki")})
case("pin_first_board_resolution", {"body": None},
     {"pin_first.set": ret({"n": 1}), "pin_first.reset": ret(None)})

# ── _normalize_board_slug ────────────────────────────────────────────────────
for s in [None, "", "  ", "ABC", " My-Board_2 ", "-x", "_x", "a" * 64, "a" * 65,
          "a b", "a/b", "a.b", "0", "9x", "a\n", "héllo", "a" * 63 + "Z",
          "\u00a0x\u00a0", "\t\r\n", "x_y-z0"]:
    case("_normalize_board_slug", {"slug": s})
case("_normalize_board_slug", {"slug": 5})
case("_normalize_board_slug", {"slug": True})
case("_normalize_board_slug", {"slug": False})
case("_normalize_board_slug", {"slug": 5.5})
case("_normalize_board_slug", {"slug": "it's"})

# ── _resolve_project_link ────────────────────────────────────────────────────
P = {"id": "proj-1", "primary_path": "/repo"}
T = {"id": "t_src", "project_id": "proj-1", "workspace_kind": "worktree",
     "workspace_path": "/repo/.worktrees/t_src", "branch_name": "proj-1/t_src"}
BASE = {"connect_closing.enter": ret("pconn"), "connect_closing.exit": ret(None)}
FALL = {"get_task": ret(T), "normalize_slug": ret("proj-1"), "Project": ret(P)}


def rpl(inp, io):
    case("_resolve_project_link", inp, io)


rpl({"project_id": None}, {})
rpl({"project_id": "   "}, {})
rpl({"project_id": "proj-1"}, dict(BASE, get_project=ret(P)))
rpl({"project_id": "proj-1", "workspace_kind": "worktree", "workspace_path": "/custom"},
    dict(BASE, get_project=ret(P)))
rpl({"project_id": "proj-1", "workspace_kind": "worktree"}, dict(BASE, get_project=ret(P)))
rpl({"project_id": "proj-1"}, dict(BASE, get_project=ret({"id": "proj-1", "primary_path": None})))
rpl({"project_id": "proj-1"}, dict(BASE, get_project=ret({"id": "proj-1", "primary_path": ""})))
rpl({"project_id": "proj-1"}, dict(BASE, get_project=ret(None)))
rpl({"project_id": "proj-1"}, dict(BASE, get_project=raise_("OSError", "db")))
rpl({"project_id": "proj-1"}, dict(BASE, get_project=raise_("KeyboardInterrupt", "ki")))
rpl({"project_id": "proj-1"}, dict(BASE, get_project=raise_("SystemExit", "sx")))
rpl({"project_id": "proj-1"},
    dict(BASE, **{"connect_closing.enter": raise_("OSError", "open")}))
rpl({"project_id": "proj-1"},
    dict(BASE, get_project=ret(P), **{"connect_closing.exit": raise_("OSError", "close")}))
rpl({"project_id": "proj-1"},
    dict(BASE, get_project=raise_("ValueError", "gp"),
         **{"connect_closing.exit": raise_("OSError", "close")}))
rpl({"project_id": "proj-1"}, {"import:hermes_cli.projects_db": halt("hermes_cli.projects_db")})
rpl({"project_id": "proj-1"},
    {"import:hermes_cli.projects_db": {"import_raise": {"type": "OSError", "message": "imp"}}})
rpl({"project_id": "proj-1", "project_source_task_id": "t_src", "workspace_kind": "worktree"},
    dict(BASE, get_project=ret(None), **FALL))
rpl({"project_id": "proj-1", "project_source_task_id": "t_src", "workspace_kind": "scratch"},
    dict(BASE, get_project=ret(None), **FALL))
rpl({"project_id": "proj-1", "project_source_task_id": ""}, dict(BASE, get_project=ret(None)))
rpl({"project_id": "proj-1", "project_source_task_id": "t_src"},
    dict(BASE, get_project=ret(None), get_task=ret(None)))
rpl({"project_id": 5}, dict(BASE, get_project=ret(P)))
rpl({"project_id": "  proj-1  "}, dict(BASE, get_project=ret(P)))
rpl({"project_id": "proj-1"}, dict(BASE, get_project=ret({"id": "proj-1"})))
rpl({"project_id": "proj-1"}, dict(BASE, get_project=ret({"primary_path": "/repo"})))
rpl({"project_id": "proj-1", "project_source_task_id": 7},
    dict(BASE, get_project=ret(None), **FALL))
rpl({"project_id": "proj-1", "project_source_task_id": "t_src"},
    dict(BASE, get_project=ret(None), get_task=ret(T), normalize_slug=ret("proj-1"),
         Project=raise_("RuntimeError", "ctor")))
rpl({"project_id": "proj-1", "project_source_task_id": "t_src", "workspace_kind": "worktree",
     "workspace_path": "/custom"},
    dict(BASE, get_project=ret(None), **FALL))

# ── _project_from_source_task ────────────────────────────────────────────────
PF = {"normalize_slug": ret("proj-1"), "Project": ret(P)}


def pfst(task, io, pid="proj-1", stid="t_src"):
    case("_project_from_source_task", {"project_id": pid, "source_task_id": stid},
         {"get_task": ret(task), **io})


pfst(T, dict(PF))
pfst(None, {})
pfst(dict(T, project_id="other"), {})
pfst(dict(T, workspace_kind="scratch"), {})
pfst(dict(T, workspace_path=""), {})
pfst(dict(T, workspace_path=None), {})
pfst(dict(T, workspace_path="repo/.worktrees/t_src"), {})
pfst(dict(T, workspace_path="/repo/wt/t_src"), {})
pfst(dict(T, workspace_path="/repo/.worktrees/other"), {})
pfst(dict(T, branch_name="proj-1/t_src-extra"), dict(PF))
pfst(dict(T, branch_name="noslash"), dict(PF))
pfst(dict(T, branch_name="proj-1/other"), dict(PF))
pfst(dict(T, branch_name=""), dict(PF))
pfst(T, {"normalize_slug": [raise_("ValueError", "bad"), ret("proj-1")], "Project": ret(P)})
pfst(T, {"normalize_slug": [raise_("ValueError", "bad"), raise_("ValueError", "bad2")]})
pfst(T, {"normalize_slug": [ret("")]})
pfst(T, {"normalize_slug": [ret(None), ret("proj-1")], "Project": ret(P)})
pfst(T, {"normalize_slug": [raise_("TypeError", "t")]})
pfst(T, {"normalize_slug": [raise_("KeyboardInterrupt", "ki")]})
pfst({"id": "t_src"}, {})
pfst({k: v for k, v in T.items() if k != "branch_name"}, {})
pfst(dict(T, branch_name=5), {})
pfst(T, {"normalize_slug": ret("proj-1"), "Project": raise_("OSError", "ctor")})
pfst({}, {})
pfst(dict(T, workspace_path=5), {})
pfst(dict(T, workspace_path="/.worktrees/t_src"), dict(PF))
pfst(dict(T, branch_name="proj-1/t_src-"), dict(PF))
pfst(T, {"normalize_slug": ret("proj-1"), "Project": ret(P)}, pid="proj-2")

# ── _stage_completion_artifacts ──────────────────────────────────────────────
ST = {"_persist_scratch_completion_artifacts": ret(None)}


def stage(inp, io):
    case("_stage_completion_artifacts", {"task_id": "t_1", "now": 100, **inp}, io)


stage({"metadata": {}}, dict(ST))
stage({"metadata": {"_staged_artifacts": ["/att/a.txt", "/att/b.txt"]}},
      dict(ST, stat=[ret({"st_size": 10}), ret({"st_size": 20})],
           _insert_completion_attachment=[ret(None), ret(None)]))
stage({"metadata": {"_staged_artifacts": ["/att/a.txt"]}},
      {"_persist_scratch_completion_artifacts": raise_("ValueError", "p")})
stage({"metadata": {"_staged_artifacts": ["/att/a.txt"]}},
      dict(ST, stat=[raise_("OSError", "st")]))
stage({"metadata": {"_staged_artifacts": ["/att/a.txt"]}},
      dict(ST, stat=[ret({"st_size": 5})], _insert_completion_attachment=[raise_("RuntimeError", "ins")]))
stage({"metadata": {"_staged_artifacts": ["/att/a.txt"]}}, dict(ST, stat=[ret({})]))
stage({"metadata": {"_staged_artifacts": ["/att/a.txt"]}}, dict(ST, stat=[ret(None)]))
stage({"metadata": {"artifacts": ["x"], "_staged_artifacts": []}}, dict(ST))
stage({"metadata": {"_staged_artifacts": ["/att/a.txt"]}, "uploaded_by": "worker_x", "now": 7},
      dict(ST, stat=[ret({"st_size": 1})], _insert_completion_attachment=[ret(None)]))
stage({"metadata": {"_staged_artifacts": ["/att//sub/../a.txt"]}},
      dict(ST, stat=[ret({"st_size": 3})], _insert_completion_attachment=[ret(None)]))
stage({"metadata": {"_staged_artifacts": None}}, dict(ST))
stage({"metadata": None}, dict(ST))
stage({"metadata": {"_staged_artifacts": ["/att/a.txt", "/att/b.txt"]}},
      dict(ST, stat=[ret({"st_size": 1}), raise_("OSError", "st2")],
           _insert_completion_attachment=[ret(None)]))
stage({"metadata": {"_staged_artifacts": [5]}}, dict(ST))
stage({"metadata": {"_staged_artifacts": ["rel/a.txt", "/abs/b.txt"]}},
      dict(ST, stat=[ret({"st_size": 2}), ret({"st_size": 4})],
           _insert_completion_attachment=[ret(None), ret(None)]))

# ── _discard_staged_copies ───────────────────────────────────────────────────
def discard(copies, unlink, rmdir_, ad="/att"):
    io = {"rmdir": rmdir_}
    if unlink is not None:
        io["unlink"] = unlink
    case("_discard_staged_copies", {"copies": copies, "attachment_dir": ad}, io)


discard(["/att/a", "/att/b"], [ret(None), ret(None)], ret(None))
discard(["/att/a"], [raise_("FileNotFoundError", "x")], ret(None))
discard(["/att/a"], [raise_("PermissionError", "x")], ret(None))
discard(["/att/a"], [raise_("ValueError", "x")], ret(None))
discard(["/att/a"], [raise_("KeyboardInterrupt", "x")], ret(None))
discard([], None, raise_("OSError", "not empty"))
discard([], None, raise_("ValueError", "bad"))
discard([], None, ret(None))
discard(["/att/a", "/att/b"], [ret(None), raise_("OSError", "gone")], ret(None))
discard(None, None, ret(None))
discard([], None, raise_("FileNotFoundError", "no dir"))
discard(["/att//a.txt"], [ret(None)], ret(None), ad="/att//sub/..")

# ── _copy_capped ─────────────────────────────────────────────────────────────
def copy(chunks, cap=None, open_=None, read=None, write=None, close=None,
         src="/w/a.txt", dest="/att/a.txt", artifact="a.txt"):
    inp = {"src": src, "dest": dest, "artifact": artifact}
    if cap is not None:
        inp["cap"] = cap
    io = {}
    io["open"] = open_ if open_ is not None else [ret("src-h"), ret("dest-h")]
    io["read"] = read if read is not None else [ret(b64(c)) for c in chunks]
    if write is not None:
        io["write"] = write
    else:
        nw = sum(1 for c in chunks[:-1]) if chunks and chunks[-1] == b"" else len(chunks)
        io["write"] = [ret(None)] * nw
    io["close"] = close if close is not None else [ret(None), ret(None)]
    case("_copy_capped", inp, io)


copy([b"abc", b""])
copy([b""])
copy([b"12345", b"678901", b""], cap=10, write=[ret(None)])
copy([b"1234567890", b""], cap=10)
copy([b"123", b"456", b""], cap=10)
copy([b"a", b""], read=[raise_("OSError", "rd")], write=[])
copy([b"a", b""], write=[raise_("OSError", "wr")])
copy([b"a", b""], open_=[ret("src-h"), raise_("FileExistsError", "exists")],
     write=[], close=[ret(None)])
copy([b"a", b""], open_=[raise_("FileNotFoundError", "nope")], write=[], close=[])
copy([b""], close=[raise_("OSError", "cl"), ret(None)])
copy([b""], close=[ret(None), raise_("OSError", "srccl")])
copy([b""], close=[raise_("OSError", "destcl"), raise_("OSError", "srccl")])
copy([b"12345", b"678901", b""], cap=10, write=[ret(None)],
     close=[raise_("OSError", "cl"), ret(None)])
copy([b"abc", b""])  # default byte-pinned cap
copy([b"\xff\xfe", b""])
copy([b"a"], read=[ret(b64(b"a"))], write=[ret(None)])  # exhausted: fail closed
copy([b"x" * 1024, b"y" * 1024, b""], cap=2048)
copy([b"x" * 1024, b"y" * 1024, b""], cap=2047, write=[ret(None)])


def check_const():
    """Verify the byte-pinned C macros against the LIVE AST."""
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    live_cap = None
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for tgt in node.targets:
                if isinstance(tgt, ast.Name) and tgt.id == "KANBAN_ATTACHMENT_MAX_BYTES":
                    live_cap = eval(compile(ast.Expression(node.value), str(SRC), "eval"))
    csrc = PORT.read_text(encoding="utf-8")
    m = re.search(r"#define\s+KD2_ATTACHMENT_MAX_BYTES\s+(\d+)LL", csrc)
    c_cap = int(m.group(1)) if m else None
    m = re.search(r"#define\s+KD2_COPY_CHUNK\s+(\d+)LL", csrc)
    c_chunk = int(m.group(1)) if m else None
    # live chunk: the read(1024 * 1024) call inside _copy_capped
    live_chunk = None
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_copy_capped":
            for sub in ast.walk(node):
                if isinstance(sub, ast.Call) and isinstance(sub.func, ast.Attribute) \
                        and sub.func.attr == "read" and sub.args:
                    live_chunk = eval(compile(ast.Expression(sub.args[0]), str(SRC), "eval"))
    ok = True
    if live_cap != c_cap:
        print(f"CHECK-CONST cap drift: live={live_cap} C={c_cap}")
        ok = False
    if live_chunk != c_chunk:
        print(f"CHECK-CONST chunk drift: live={live_chunk} C={c_chunk}")
        ok = False
    print(f"CHECK-CONST kanban_db_flow2: cap={c_cap} chunk={c_chunk} {'OK' if ok else 'DRIFT'}")
    return 0 if ok else 1


def check_pop():
    """Every PoP annotation must name a real live top-level def."""
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    live = {n.name for n in tree.body if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))}
    csrc = PORT.read_text(encoding="utf-8")
    pops = re.findall(r"PoP:\s*\w+\s*@\s*hermes_cli/kanban_db\.py:(\w+)", csrc)
    bad = [p for p in pops if p not in live]
    dup = len(pops) != len(set(pops))
    print(f"CHECK-POP kanban_db_flow2: {len(pops)} annotations, {len(bad)} unknown, dup={dup}")
    for p in bad:
        print("  unknown:", p)
    return 0 if not bad and not dup and len(pops) >= 7 else 1


def main():
    rc = 0
    if "--check-const" in sys.argv:
        rc |= check_const()
    if "--check-pop" in sys.argv:
        rc |= check_pop()
    if "--check-const" in sys.argv or "--check-pop" in sys.argv:
        return rc
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    print(f"GEN kanban_db_flow2: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")
    for k, v in sorted(counts.items()):
        print(f"  {k}: {v}")
    if thin:
        print("GEN FAIL: funcs with <5 cases:", thin)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())