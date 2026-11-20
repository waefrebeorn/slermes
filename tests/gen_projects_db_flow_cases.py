#!/usr/bin/env python3
"""gen_projects_db_flow_cases.py — companion generator for the projects_db
worst-tier FLOW port (src/cli/projects_db_flow.c).

Emits tests/fixtures/projects_db_flow.jsonl — the scripted-seam case file the
runner auto-feeds to tests/t_port_projects_db_flow.c (>=5 cases per ported
func; 19 funcs). Every script shape mirrors the oracle's recording stubs:
{"ret": v} / {"raise": {"type","message"}} / a per-call queue LIST. A seam
whose live result is itself a LIST (list_projects, cursor fetchall) is
scripted via {"ret": [...]} — a bare array would be read as a per-call queue.

Domain guards (documented in the port header):
  - slug/name scalars: str | null | int | bool | NON-integral float.
  - ASCII slug scope; full White_Space strip (NBSP included), ASCII lower.
  - row values: str/int/float/null (bool excluded — sqlite coerces to int);
    row keys are SQL identifiers; rows non-empty; fetchall is a list.
  - params is a list or null; cursor specs are objects.
  - normalize_path / path.resolve / get_hermes_home scripts return str.
  - only table-known exception kinds are scripted (no KeyError /
    ExceptionGroup — their str() rewrites the message).
  - write_txn BEGIN/COMMIT/ROLLBACK ride the conn.execute queue in order.

--check-const verifies the C SCHEMA_SQL literal / optional-column tuples /
meta keys against the LIVE projects_db.py AST. --check-pop verifies every PoP
annotation names a real live def (top-level, class method, or nested closure)
and that the (cfunc, pyfunc) pairs are unique.

Usage: python tests/gen_projects_db_flow_cases.py [--check-const] [--check-pop]
"""
import ast
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
QUARRY = REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "projects_db.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "projects_db.py"
PORT = REPO / "src" / "cli" / "projects_db_flow.c"
OUT = HERE / "fixtures" / "projects_db_flow.jsonl"

CASES = []


def case(func, inp=None, io=None, state=None):
    d = dict(inp or {})
    d["io"] = io or {}
    if state is not None:
        d["state"] = state
    CASES.append({"func": func, "in": d})


def ret(v):
    return {"ret": v}


def raise_(t, m=""):
    return {"raise": {"type": t, "message": m}}


def cur(fetchone=None, fetchall=None, rowcount=None):
    """Cursor spec: only the keys the case reads are present (a missing key
    is the 'CursorDouble'-named AttributeError branch)."""
    d = {}
    if fetchone is not None:
        d["fetchone"] = fetchone
    if fetchall is not None:
        d["fetchall"] = fetchall
    if rowcount is not None:
        d["rowcount"] = rowcount
    return d


ALL_COLS = [{"name": c} for c in
            ["id", "slug", "name", "description", "icon", "color",
             "board_slug", "primary_path", "created_at", "archived"]]


def init_io(resolved="/r/x", pragma=None, extra=None):
    io = {"conn.executescript": ret(None),
          "conn.execute": ret(cur(fetchall=ALL_COLS if pragma is None else pragma))}
    if extra:
        io.update(extra)
    return io


# ── _slugify (pure leaf) ─────────────────────────────────────────────────────
for v in [None, "", "  ", "Hello World", "--__--", "a" * 70, "A-B_C", 5, True,
          0, False, "  MiXeD  Case  ", "-lead", "trail-", "a.b/c", "___",
          "2fast", "a" * 63 + "--y", "a" * 64 + "-", "x_y-z0", "\t\r\n",
          "a\nb", 5.5, "it's", "\u00a0x\u00a0"]:
    case("_slugify", {"name": v})

# ── _now ─────────────────────────────────────────────────────────────────────
for t in [ret(123.7), ret(0), ret(-0.5), ret(1000000000.9), ret(True),
          ret(False), ret(None), ret(5), raise_("OSError", "clock"),
          raise_("KeyboardInterrupt", "ki")]:
    case("_now", {}, {"time.time": t})

# ── projects_db_path ─────────────────────────────────────────────────────────
for h in [ret("/home/u"), ret(""), ret("/"), ret("//x"), ret("rel"),
          ret("/a//b/"), ret("."), ret("/home/u/"), raise_("OSError", "home"),
          raise_("SystemExit", "sx"), ret(5)]:
    case("projects_db_path", {}, {"get_hermes_home": h})

# ── _primary_path_key ────────────────────────────────────────────────────────
case("_primary_path_key", {"path": "/a/b"}, {"normalize_path": ret("/norm/x")})
case("_primary_path_key", {"path": ""}, {"normalize_path": ret("")})
case("_primary_path_key", {"path": "x"}, {"normalize_path": ret(5)})
case("_primary_path_key", {"path": "x"}, {"normalize_path": ret(None)})
case("_primary_path_key", {"path": "x"}, {"normalize_path": raise_("ValueError", "np")})
case("_primary_path_key", {"path": "x"}, {"normalize_path": raise_("KeyboardInterrupt", "ki")})
case("_primary_path_key", {"path": None}, {"normalize_path": ret("/n")})

# ── owns (nested closure; os double sep="/") ─────────────────────────────────
for folder, target in [
    ("/a/b", "/a/b"), ("/a/b", "/a/b/c"), ("/a/b/", "/a/b/c"),
    ("/a/b\\", "/a/b/c"), ("/a/b", "/a/bc"), ("/a/b", "/a/b/"),
    ("", ""), ("", "x"), ("/", "/x"), ("//x", "//x/y"),
    ("/a/b", ""), ("a", "a/b"),
]:
    case("owns", {"folder": folder, "target": target})

# ── _initialize (nested closure; stateful) ───────────────────────────────────
case("_initialize", {"conn": "c1", "resolved": "/r/x"}, init_io(), state={})
case("_initialize", {"conn": "c1", "resolved": "/r/x"},
     init_io(pragma=[{"name": c} for c in
                     ["id", "slug", "name", "description", "color",
                      "primary_path", "created_at", "archived"]],
             extra={"add_column_if_missing": [ret(True), ret(True)]}), state={})
case("_initialize", {"conn": "c1", "resolved": "/r/x"},
     init_io(pragma=[], extra={"add_column_if_missing":
                               [ret(True), ret(True), ret(True), ret(True)]}), state={})
case("_initialize", {"conn": "c1", "resolved": "/r/x"}, {},
     state={"initialized_paths": ["/r/x"]})
case("_initialize", {"conn": "c1", "resolved": "/r/x"},
     {"conn.executescript": raise_("OSError", "schema")}, state={})
case("_initialize", {"conn": "c1", "resolved": "/r/x"},
     {"conn.executescript": ret(None), "conn.execute": raise_("OperationalError", "pragma")},
     state={})
case("_initialize", {"conn": "c1", "resolved": "/r/x"},
     init_io(pragma=[{"name": "id"}, {"other": "x"}]), state={})
case("_initialize", {"conn": "c1", "resolved": "/r/x"},
     init_io(pragma=[{"name": "id"}],
             extra={"add_column_if_missing": raise_("ValueError", "dup")}), state={})
case("_initialize", {"conn": "c1", "resolved": "/r/x"},
     init_io(pragma=[{"name": "id"}],
             extra={"add_column_if_missing": raise_("KeyboardInterrupt", "ki")}), state={})
case("_initialize", {"conn": "c1", "resolved": "/r/x"},
     {"conn.executescript": ret(None), "conn.execute": ret(cur(fetchall=5))}, state={})
case("_initialize", {"conn": "c1", "resolved": "/r/x"},
     {"conn.executescript": ret(None), "conn.execute": ret(5)}, state={})
case("_initialize", {"conn": "c1", "resolved": "/r/x"},
     init_io(pragma=[{"name": "id"}, {"name": "id"}],
             extra={"add_column_if_missing":
                    [ret(True), ret(True), ret(True), ret(True)]}), state={})
case("_initialize", {"conn": "c1", "resolved": "/r/x"}, init_io(),
     state={"initialized_paths": ["/other"]})
case("_initialize", {"conn": "c1", "resolved": "/r/x"},
     {"conn.executescript": ret(None), "conn.execute": ret({})}, state={})

# ── connect ──────────────────────────────────────────────────────────────────
CONN_IO = {"path.resolve": ret("/home/u/projects.db"), "open_db": ret("conn-1"),
           "conn.executescript": ret(None), "conn.execute": ret(cur(fetchall=ALL_COLS))}
case("connect", {"db_path": None},
     dict(CONN_IO, **{"get_hermes_home": ret("/home/u")}))
case("connect", {"db_path": "/x//y"},
     {"path.resolve": ret("/resolved"), "open_db": ret("c2"),
      "conn.executescript": ret(None), "conn.execute": ret(cur(fetchall=ALL_COLS))})
case("connect", {"db_path": "/x"}, {"path.resolve": raise_("OSError", "resolve")})
case("connect", {"db_path": "/x"},
     {"path.resolve": ret("/r"), "open_db": raise_("OSError", "open")})
case("connect", {"db_path": "/x"},
     {"path.resolve": ret("/r"), "open_db": ret("c1"),
      "conn.executescript": raise_("ValueError", "schema"), "conn.close": ret(None)})
case("connect", {"db_path": "/x"},
     {"path.resolve": ret("/r"), "open_db": ret("c1"),
      "conn.executescript": raise_("ValueError", "schema"),
      "conn.close": raise_("OSError", "close")})
case("connect", {"db_path": "/x"},
     {"path.resolve": ret("/r"), "open_db": ret("c1"),
      "conn.executescript": raise_("ValueError", "schema"),
      "conn.close": raise_("KeyboardInterrupt", "ki")})
case("connect", {"db_path": 5}, {})
case("connect", {"db_path": None}, {"get_hermes_home": raise_("RuntimeError", "home")})
case("connect", {"db_path": "/x"}, {"path.resolve": ret(5)})
case("connect", {"db_path": ""},
     {"path.resolve": ret("/cwd"), "open_db": ret("c3"),
      "conn.executescript": ret(None), "conn.execute": ret(cur(fetchall=ALL_COLS))})
case("connect", {"db_path": "/x"},
     {"path.resolve": ret("/r"), "open_db": ret("c4"),
      "conn.executescript": ret(None), "conn.execute": ret(cur(fetchall=[])),
      "add_column_if_missing": [ret(True), ret(True), ret(True), ret(True)]})

# ── connect_closing ──────────────────────────────────────────────────────────
CC_IO = {"path.resolve": ret("/r"), "open_db": ret("c1"),
         "conn.executescript": ret(None), "conn.execute": ret(cur(fetchall=ALL_COLS)),
         "conn.close": ret(None)}
case("connect_closing", {"db_path": "/x", "body": None}, dict(CC_IO))
case("connect_closing", {"db_path": "/x", "body": {"raise": {"type": "ValueError", "message": "body"}}},
     dict(CC_IO))
case("connect_closing", {"db_path": "/x", "body": {"raise": {"type": "SystemExit", "message": "sx"}}},
     dict(CC_IO))
case("connect_closing", {"db_path": "/x", "body": {"raise": {"type": "ValueError", "message": "body"}}},
     dict(CC_IO, **{"conn.close": raise_("OSError", "close")}))
case("connect_closing", {"db_path": "/x", "body": {"raise": {"type": "ValueError", "message": "body"}}},
     dict(CC_IO, **{"conn.close": raise_("KeyboardInterrupt", "ki")}))
case("connect_closing", {"db_path": "/x", "body": None},
     dict(CC_IO, **{"conn.close": raise_("SystemExit", "sx")}))
case("connect_closing", {"db_path": "/x", "body": None},
     {"path.resolve": ret("/r"), "open_db": raise_("OSError", "open")})
case("connect_closing", {"db_path": None, "body": None},
     dict(CC_IO, **{"get_hermes_home": ret("/home/u")}))
case("connect_closing", {"db_path": "/x", "body": {"raise": {"type": "GeneratorExit", "message": ""}}},
     dict(CC_IO))

# ── ProjectFolder.to_dict ────────────────────────────────────────────────────
for f in [
    {"path": "/a", "label": "L", "is_primary": True, "added_at": 5},
    {"path": "/a"},
    {},
    {"path": "/a", "is_primary": "yes"},
    {"path": "/a", "is_primary": 0},
    {"path": "/a", "is_primary": None},
    {"path": "/a", "is_primary": []},
    {"path": "/a", "is_primary": {}},
    {"path": "/a", "is_primary": [1]},
    "x", 5, None,
    {"path": "/a", "label": "L", "added_at": 9},
]:
    case("ProjectFolder.to_dict", {"folder": f})

# ── Project.to_dict ──────────────────────────────────────────────────────────
for p in [
    {"id": "p1", "slug": "s", "name": "n", "created_at": 5, "archived": False, "folders": []},
    {"id": "p1", "slug": "s", "name": "n", "created_at": 5},
    {"id": "p1", "slug": "s", "name": "n", "created_at": 5, "description": "d",
     "icon": "i", "color": "c", "board_slug": "b", "primary_path": "/p",
     "archived": True, "folders": [{"path": "/a", "label": "L", "is_primary": 1, "added_at": 7}]},
    {"id": "p1", "name": "n", "created_at": 5, "folders": []},
    {"id": "p1", "slug": "s", "name": "n", "created_at": 5, "folders": [5]},
    {"id": "p1", "slug": "s", "name": "n", "created_at": 5, "folders": None},
    {"id": "p1", "slug": "s", "name": "n", "created_at": 5, "folders": 5},
    {"id": "p1", "slug": "s", "name": "n", "created_at": 5, "folders": [{"path": "/a"}]},
    {"id": "p1", "slug": "s", "name": "n", "created_at": 5, "folders": [{"label": "x"}]},
    {"id": "p1", "slug": "s", "name": "n", "created_at": 5, "archived": "yes", "folders": []},
    "x", None,
    {"id": "p1", "slug": "s", "name": "n", "created_at": 5,
     "folders": [{"path": "/a", "label": None, "is_primary": 1, "added_at": 7}]},
]:
    case("Project.to_dict", {"project": p})

# ── _load_project ────────────────────────────────────────────────────────────
FULL_ROW = {"id": "p1", "slug": "s", "name": "n", "created_at": 5, "archived": 0,
            "description": "d", "icon": "i", "color": "c", "board_slug": "b",
            "primary_path": "/p"}
MIN_ROW = {"id": "p1", "slug": "s", "name": "n", "created_at": 5}
case("_load_project", {"row": FULL_ROW}, {"conn.execute": ret(cur(fetchall=[]))})
case("_load_project", {"row": MIN_ROW}, {"conn.execute": ret(cur(fetchall=[]))})
case("_load_project", {"row": {"slug": "s", "name": "n", "created_at": 5}}, {})
case("_load_project", {"row": {"id": "p1", "name": "n", "created_at": 5}},
     {"conn.execute": ret(cur(fetchall=[]))})
case("_load_project", {"row": MIN_ROW},
     {"conn.execute": ret(cur(fetchall=[
         {"path": "/a", "label": "L", "is_primary": 1, "added_at": 7},
         {"path": "/b", "label": None, "is_primary": 0, "added_at": 8}]))})
case("_load_project", {"row": MIN_ROW},
     {"conn.execute": ret(cur(fetchall=[{"path": "/a", "is_primary": 1, "added_at": 7}]))})
case("_load_project", {"row": MIN_ROW}, {"conn.execute": raise_("OperationalError", "db")})
case("_load_project", {"row": MIN_ROW}, {"conn.execute": ret(cur(fetchall=5))})
case("_load_project", {"row": None}, {})
case("_load_project", {"row": {}}, {})
case("_load_project", {"row": dict(MIN_ROW, archived="x")},
     {"conn.execute": ret(cur(fetchall=[]))})
case("_load_project", {"row": MIN_ROW},
     {"conn.execute": ret(cur(fetchall=[{"path": "/a", "label": None, "is_primary": 0, "added_at": 0}]))})
case("_load_project", {"row": MIN_ROW}, {"conn.execute": ret({})})
case("_load_project", {"row": dict(MIN_ROW, description=None, icon=None)},
     {"conn.execute": ret(cur(fetchall=[]))})

# ── find_by_primary_path ─────────────────────────────────────────────────────
def fbp(projects, norm_queue, path="/target", include_archived=False, norm_raise=None):
    io = {}
    if norm_raise is not None:
        io["normalize_path"] = norm_raise
    else:
        io["normalize_path"] = norm_queue
    if projects is not None:
        io["list_projects"] = projects
    case("find_by_primary_path", {"path": path, "include_archived": include_archived}, io)


fbp(None, ret(""))                                              # empty key: no list seam
fbp(ret([]), [ret("/key")])                                     # no projects
fbp(ret([{"primary_path": "/key", "folders": []}]), [ret("/key"), ret("/key")])
fbp(ret([{"primary_path": None, "folders": [{"path": "/key", "is_primary": True}]}]),
    [ret("/key"), ret("/key")])
fbp(ret([{"primary_path": None, "folders": [{"path": "/first", "is_primary": False},
                                            {"path": "/key", "is_primary": False}]}]),
    [ret("/key"), ret("/first")])
fbp(ret([{"primary_path": None, "folders": []}]), [ret("/key")])
fbp(ret([{"folders": []}]), [ret("/key")])                      # missing primary_path
fbp(ret([{"primary_path": None}]), [ret("/key")])               # missing folders
fbp(ret([{"primary_path": None, "folders": [{}]}]), [ret("/key")])   # eager default path
fbp(ret([{"primary_path": None, "folders": [{"is_primary": True}]}]), [ret("/key")])
fbp(ret([{"primary_path": None, "folders": [{"path": "/first", "is_primary": False},
                                            {"path": "/key", "is_primary": True}]}]),
    [ret("/key"), ret("/key")])
fbp(raise_("OSError", "db"), [ret("/key")])
fbp(None, None, norm_raise=raise_("ValueError", "np"))
fbp(ret([{"primary_path": 5, "folders": []}]), [ret("/key"), ret("/key")])
fbp(ret([{"primary_path": "/key", "folders": []}]), [ret("/key"), ret("/key")],
    include_archived=True)
fbp(ret([{"primary_path": None, "folders": None}]), [ret("/key")])
fbp(ret([{"primary_path": "", "folders": [{"path": "/key", "is_primary": True}]}]),
    [ret("/key"), ret("/key")])
fbp(ret(None), [ret("/key")])                                   # list_projects null

# ── _execute_rowcount ────────────────────────────────────────────────────────
def er(queue, params=None, sql="UPDATE projects SET name = ? WHERE id = ?"):
    inp = {"sql": sql}
    if params is not None:
        inp["params"] = params
    case("_execute_rowcount", inp, {"conn.execute": queue})


er([ret({}), ret(cur(rowcount=1)), ret({})], params=["p1"])
er([ret({}), ret(cur(rowcount=0)), ret({})], params=["p1"])
er([ret({}), raise_("ValueError", "upd"), ret({})], params=["p1"])
er([ret({}), raise_("KeyboardInterrupt", "ki")], params=["p1"])
er([ret({}), raise_("ValueError", "upd"), raise_("OperationalError", "no txn")], params=["p1"])
er([ret({}), raise_("ValueError", "upd"), raise_("RuntimeError", "rb")], params=["p1"])
er([raise_("OSError", "begin")], params=["p1"])
er([ret({}), ret(cur(rowcount=1)), raise_("OSError", "commit")], params=["p1"])
er([ret({}), ret({}), ret({})], params=["p1"])                  # missing rowcount
er([ret({}), ret({})], params=None)                             # null params -> TypeError + ROLLBACK
er([ret({}), ret(5), ret({})], params=["p1"])                   # non-object cursor
er([ret({}), raise_("OperationalError", "upd"), ret({})], params=["p1"])

# ── _upsert_meta_locked ──────────────────────────────────────────────────────
case("_upsert_meta_locked", {"key": "active_id", "value": "p1"}, {"conn.execute": ret({})})
case("_upsert_meta_locked", {"key": "active_id", "value": None}, {"conn.execute": ret({})})
case("_upsert_meta_locked", {"key": "active_id", "value": "p1"},
     {"conn.execute": raise_("IntegrityError", "dup")})
case("_upsert_meta_locked", {"key": None, "value": "p1"}, {"conn.execute": ret({})})
case("_upsert_meta_locked", {"key": "active_id", "value": "p1"},
     {"conn.execute": raise_("KeyboardInterrupt", "ki")})

# ── _get_meta ────────────────────────────────────────────────────────────────
case("_get_meta", {"key": "active_id"}, {"conn.execute": ret(cur(fetchone={"value": "p1"}))})
case("_get_meta", {"key": "active_id"}, {"conn.execute": ret(cur(fetchone={"value": None}))})
case("_get_meta", {"key": "active_id"}, {"conn.execute": ret(cur(fetchone=None))})
case("_get_meta", {"key": "active_id"}, {"conn.execute": ret(cur(fetchone={"other": "x"}))})
case("_get_meta", {"key": "active_id"}, {"conn.execute": raise_("OperationalError", "db")})
case("_get_meta", {"key": "active_id"}, {"conn.execute": ret({})})
case("_get_meta", {"key": "active_id"}, {"conn.execute": ret(cur(fetchone={}))})
case("_get_meta", {"key": "active_id"}, {"conn.execute": ret(cur(fetchone=5))})

# ── _clear_repos_locked ──────────────────────────────────────────────────────
case("_clear_repos_locked", {"clear": True, "policy_key": None}, {"conn.execute": ret({})})
case("_clear_repos_locked", {"clear": False, "policy_key": "k"}, {"conn.execute": ret({})})
case("_clear_repos_locked", {"clear": True, "policy_key": "k"},
     {"conn.execute": [ret({}), ret({})]})
case("_clear_repos_locked", {"clear": False, "policy_key": None}, {})
case("_clear_repos_locked", {"clear": True, "policy_key": None},
     {"conn.execute": raise_("OSError", "del")})
case("_clear_repos_locked", {"clear": False, "policy_key": "k"},
     {"conn.execute": raise_("IntegrityError", "dup")})
case("_clear_repos_locked", {"clear": True, "policy_key": ""},
     {"conn.execute": [ret({}), ret({})]})

# ── get_discovery_policy_key ─────────────────────────────────────────────────
case("get_discovery_policy_key", {}, {"conn.execute": ret(cur(fetchone={"value": "pol"}))})
case("get_discovery_policy_key", {}, {"conn.execute": ret(cur(fetchone=None))})
case("get_discovery_policy_key", {}, {"conn.execute": ret(cur(fetchone={"value": None}))})
case("get_discovery_policy_key", {}, {"conn.execute": raise_("OSError", "db")})
case("get_discovery_policy_key", {}, {"conn.execute": ret(cur(fetchone={"other": 1}))})

# ── reconcile_discovered_repos_policy ────────────────────────────────────────
case("reconcile_discovered_repos_policy", {"policy_key": "new"},
     {"conn.execute": ret(cur(fetchone={"value": "new"}))})
case("reconcile_discovered_repos_policy", {"policy_key": "new", "preserve_unversioned": False},
     {"conn.execute": [ret(cur(fetchone=None)), ret({}), ret({}), ret({}), ret({})]})
case("reconcile_discovered_repos_policy", {"policy_key": "new", "preserve_unversioned": True},
     {"conn.execute": [ret(cur(fetchone=None)), ret({}), ret({}), ret({})]})
case("reconcile_discovered_repos_policy", {"policy_key": "new", "preserve_unversioned": True},
     {"conn.execute": [ret(cur(fetchone={"value": "old"})), ret({}), ret({}), ret({}), ret({})]})
case("reconcile_discovered_repos_policy", {"policy_key": "new"},
     {"conn.execute": [ret(cur(fetchone=None)), raise_("OSError", "begin")]})
case("reconcile_discovered_repos_policy", {"policy_key": "new"},
     {"conn.execute": [ret(cur(fetchone=None)), ret({}), raise_("ValueError", "del"), ret({})]})
case("reconcile_discovered_repos_policy", {"policy_key": None},
     {"conn.execute": ret(cur(fetchone=None))})
case("reconcile_discovered_repos_policy", {"policy_key": "new", "preserve_unversioned": False},
     {"conn.execute": [ret(cur(fetchone={"value": "old"})), ret({}), ret({}), ret({}), ret({})]})

# ── clear_discovered_repos ───────────────────────────────────────────────────
case("clear_discovered_repos", {"policy_key": None},
     {"conn.execute": [ret({}), ret({}), ret({})]})
case("clear_discovered_repos", {"policy_key": "k"},
     {"conn.execute": [ret({}), ret({}), ret({}), ret({})]})
case("clear_discovered_repos", {"policy_key": None},
     {"conn.execute": [ret({}), raise_("OSError", "del"), ret({})]})
case("clear_discovered_repos", {"policy_key": "k"},
     {"conn.execute": [ret({}), ret({}), raise_("IntegrityError", "dup"), ret({})]})
case("clear_discovered_repos", {"policy_key": None},
     {"conn.execute": [ret({}), ret({}), raise_("OSError", "commit")]})
case("clear_discovered_repos", {"policy_key": ""},
     {"conn.execute": [ret({}), ret({}), ret({}), ret({})]})


# ── checks ───────────────────────────────────────────────────────────────────

def _decode_c_strings(region):
    """Concatenate + decode the C string literals in a declaration region."""
    lits = re.findall(r'"((?:[^"\\]|\\.)*)"', region, re.S)
    out = []
    esc = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\", "0": "\0"}
    for lit in lits:
        i = 0
        while i < len(lit):
            c = lit[i]
            if c == "\\" and i + 1 < len(lit):
                out.append(esc.get(lit[i + 1], lit[i + 1]))
                i += 2
            else:
                out.append(c)
                i += 1
    return "".join(out)


def check_const():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    live = {}
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for tgt in node.targets:
                if isinstance(tgt, ast.Name) and tgt.id in (
                    "SCHEMA_SQL", "_OPTIONAL_PROJECT_COLUMNS", "_OPTIONAL_ROW_FIELDS",
                    "_ACTIVE_META_KEY", "_DISCOVERY_POLICY_META_KEY",
                ):
                    live[tgt.id] = ast.literal_eval(node.value)
    csrc = PORT.read_text(encoding="utf-8")
    m = re.search(r'static const char PDB_SCHEMA_SQL\[\]\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);',
                  csrc, re.S)
    c_schema = _decode_c_strings(m.group(1)) if m else None
    ok = True
    if c_schema != live.get("SCHEMA_SQL"):
        print("CHECK-CONST SCHEMA_SQL drift")
        ok = False
    for name, macro in [("_OPTIONAL_PROJECT_COLUMNS", "PDB_OPTIONAL_PROJECT_COLUMNS"),
                        ("_OPTIONAL_ROW_FIELDS", "PDB_OPTIONAL_ROW_FIELDS")]:
        m = re.search(r"static const char \*" + macro + r"\[\]\s*=\s*\{(.*?)\};", csrc, re.S)
        vals = tuple(re.findall(r'"([^"]*)"', m.group(1))) if m else None
        if vals != live.get(name):
            print(f"CHECK-CONST {name} drift: live={live.get(name)} C={vals}")
            ok = False
    for name, macro in [("_ACTIVE_META_KEY", "PDB_ACTIVE_META_KEY"),
                        ("_DISCOVERY_POLICY_META_KEY", "PDB_DISCOVERY_POLICY_META_KEY")]:
        m = re.search(r"#define\s+" + macro + r'\s+"([^"]*)"', csrc)
        val = m.group(1) if m else None
        if val != live.get(name):
            print(f"CHECK-CONST {name} drift: live={live.get(name)} C={val}")
            ok = False
    print(f"CHECK-CONST projects_db_flow: schema_len={len(c_schema) if c_schema else None} "
          f"{'OK' if ok else 'DRIFT'}")
    return 0 if ok else 1


def check_pop():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    live = set()
    for n in tree.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)):
            live.add(n.name)
            for s in ast.walk(n):
                if isinstance(s, ast.FunctionDef) and s is not n:
                    live.add(s.name)
        elif isinstance(n, ast.ClassDef):
            for s in n.body:
                if isinstance(s, ast.FunctionDef):
                    live.add(s.name)
    csrc = PORT.read_text(encoding="utf-8")
    pops = re.findall(r"PoP:\s*(\w+)\s*@\s*hermes_cli/projects_db\.py:(\w+)", csrc)
    bad = [p for p in pops if p[1] not in live]
    dup = len(pops) != len(set(pops))
    print(f"CHECK-POP projects_db_flow: {len(pops)} annotations, {len(bad)} unknown, dup={dup}")
    for p in bad:
        print("  unknown:", p)
    return 0 if not bad and not dup and len(pops) >= 19 else 1


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
    print(f"GEN projects_db_flow: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")
    for k, v in sorted(counts.items()):
        print(f"  {k}: {v}")
    if thin:
        print("GEN FAIL: funcs with <5 cases:", thin)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())