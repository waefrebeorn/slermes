#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_projects_flow.jsonl for the
tui_gateway/methods_projects.py FLOW port (src/gateway/tui_methods_projects_flow.c).

Each case: {"func": <live def name / _h_* bare handler>, "in": {"args": {...},
"io": {...}, ["state": {...}]}, ["fail_closed": true]}.

io scripts mirror the C universal seam: a plain value is returned on every
call; a bare LIST is a per-call queue (exhaustion is fail-closed); a
{"__script__": true, "ret": v} wrapper returns v (use it for list-valued
results); {"__script__": true, "raise": {"type", "message"}} raises.
ok/err scripts return the JSON-RPC envelope the server global would build.
connect_closing / persistence_for scripts return the YIELDED handle (CM silent).
os.walk scripts return a TREE node {"dirs": {name: subtree}, "files": [...]}.

Every func gets >= 5 cases: happy paths, branch variants, error paths, a
fail-closed missing-script case, and (where a catch site exists) a
BaseException propagation case. ASCII corpus; absolute POSIX paths; integral
numbers. Scripted KeyError raises are outside the domain (KeyError str() is
repr(key)); the C-emitted KeyError specs are exercised via live subscripts.
"""
import json
import posixpath
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "tui_methods_projects_flow.jsonl"

CASES = []


def case(func, args=None, io=None, state=None, fail_closed=False):
    inp = {"args": args or {}, "io": io or {}}
    if state is not None:
        inp["state"] = state
    c = {"func": func, "in": inp}
    if fail_closed:
        c["fail_closed"] = True
    CASES.append(c)


def ok_env(rid, result):
    return {"jsonrpc": "2.0", "id": rid, "result": result}


def err_env(rid, code, message):
    return {"jsonrpc": "2.0", "id": rid, "error": {"code": code, "message": message}}


def script(ret):
    return {"__script__": True, "ret": ret}


def dio(base, updates):
    d = dict(base)
    d.update(updates)
    return d


def raise_script(kind, message):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


RID = "r1"
HOME = "/home/u"
HH = "/home/u/.hermes"

DEFAULTS = {"desktop": {"repo_scan_enabled": True,
                        "repo_scan_roots": ["/home/u/code"],
                        "repo_scan_exclude_paths": ["/home/u/code/.cache"]}}
EMPTY_DEFAULTS = {"desktop": {"repo_scan_enabled": True,
                              "repo_scan_roots": [],
                              "repo_scan_exclude_paths": []}}


def eu_map(v, home=HOME):
    if v == "~":
        return home
    if v.startswith("~/"):
        return home + v[1:]
    return v


def eu_queue(lists, home=HOME):
    """expanduser call queue for _repo_discovery_policy_key._paths over the
    given value lists: one home call ("~") per list, then one per value."""
    q = []
    for values in lists:
        q.append(home)
        q.extend(eu_map(v, home) for v in values)
    return q


def nwd_realpath_queue(home=HOME):
    dn = posixpath.dirname(home)
    return [home, "/", home, dn, "/home", "/Users"]


def nwd_io(home=HOME):
    return {"os.path.expanduser": home, "os.path.realpath": nwd_realpath_queue(home)}


def junk_io(root_real, hh=HH, home=HOME):
    io = {"get_hermes_home": hh, "os.path.expanduser": home}
    io["os.path.realpath"] = [root_real, hh] + nwd_realpath_queue(home)
    return io


def junk_io_many(root_reals, hh=HH, home=HOME):
    io = {"get_hermes_home": hh, "os.path.expanduser": home}
    q = []
    for r in root_reals:
        q.extend([r, hh] + nwd_realpath_queue(home))
    io["os.path.realpath"] = q
    return io


def leaf(dirs=None, files=None):
    return {"dirs": dirs or {}, "files": files or []}


def gitdir():
    return leaf({".git": leaf()})


SESSION = {
    "session_id": "s1", "parent_session_id": None, "reset_from": None,
    "branched_from": None, "title": "t", "started_at": 0, "ended_at": None,
    "source": "cli", "archived": 0, "message_count": 0, "tool_call_count": 0,
    "input_tokens": 0, "output_tokens": 0, "actual_cost_usd": None,
    "estimated_cost_usd": None, "model": None, "cwd": None,
    "git_branch": None, "git_repo_root": None,
}


def entry(session=None, lineage_root_id="s1", lineage_ids=None, preview="p",
          last_active=0):
    return {"session": dict(SESSION if session is None else session),
            "lineage_root_id": lineage_root_id,
            "lineage_ids": [] if lineage_ids is None else lineage_ids,
            "preview": preview, "last_active": last_active}


# ── _pick ──
case("_pick", {"params": {"a": 1, "b": 2}, "keys": ["a", "c"]})
case("_pick", {"params": {"a": 1}, "keys": []})
case("_pick", {"params": {"a": 1, "b": None}, "keys": ["a", "b"]})
case("_pick", {"params": None, "keys": ["a"]})          # AttributeError 'get'
case("_pick", {"params": None, "keys": []})              # no iteration -> {}
case("_pick", {"params": {"a": 1, "b": 2}, "keys": ["a", "a", "b"]})  # dup keys

# ── _project_tree_row ──
case("_project_tree_row", {"entry": entry()})
case("_project_tree_row", {"entry": entry(lineage_ids=["s1", "s0"], last_active=7.0)})
s = dict(SESSION, started_at=5, archived=1, message_count=3, tool_call_count=2,
         input_tokens=10, output_tokens=20, actual_cost_usd=0.5,
         estimated_cost_usd=0.25, model="m", cwd="/w", git_branch="main",
         git_repo_root="/w")
case("_project_tree_row", {"entry": entry(session=s, last_active=0)})  # fallback started_at
case("_project_tree_row", {"entry": entry(session=dict(SESSION, started_at=None))})
case("_project_tree_row", {"entry": {"lineage_root_id": "s1"}})        # missing session
case("_project_tree_row", {"entry": entry(lineage_ids=None)})          # list(None)
e = entry()
del e["session"]["title"]
case("_project_tree_row", {"entry": e})                                # missing s.title
case("_project_tree_row", {"entry": entry(session=dict(SESSION, archived="yes"))})

# ── _non_workspace_dirs ──
case("_non_workspace_dirs", {}, io=nwd_io())
case("_non_workspace_dirs", {}, io=nwd_io(home="/"))
case("_non_workspace_dirs", {}, io={"os.path.expanduser": "",
                                    "os.path.realpath": ["", "/", "/home", "/Users"]})
case("_non_workspace_dirs", {}, io={"os.path.expanduser": HOME,
                                    "os.path.realpath": [HOME, raise_script("OSError", "denied")]})
case("_non_workspace_dirs", {}, fail_closed=True)

# ── _is_repo_junk ──
case("_is_repo_junk", {"root": ""})
case("_is_repo_junk", {"root": None})
case("_is_repo_junk", {"root": "/home/u/code/proj"}, io=junk_io("/home/u/code/proj"))
case("_is_repo_junk", {"root": "/home/u"}, io=junk_io("/home/u"))          # in nwd set
case("_is_repo_junk", {"root": "/home/u/.hermes"}, io=junk_io("/home/u/.hermes"))  # == hh
case("_is_repo_junk", {"root": "/home/u/.hermes/ws"}, io=junk_io("/home/u/.hermes/ws"))  # under hh
case("_is_repo_junk", {"root": "/home/u/code/proj"},
     io={"get_hermes_home": HH, "os.path.realpath": ["/home/u/code/proj", HH],
         "os.path.expanduser": HOME}, fail_closed=True)  # nwd expanduser scripted, realpath exhausted
case("_is_repo_junk", {"root": "/x"}, io={"get_hermes_home": None,
                                          "os.path.realpath": ["/x", "None"] + nwd_realpath_queue(),
                                          "os.path.expanduser": HOME})  # str(None) == "None"

# ── _is_session_cwd_junk ──
case("_is_session_cwd_junk", {"cwd": ""})
case("_is_session_cwd_junk", {"cwd": "/home/u/code/proj"}, io=junk_io("/home/u/code/proj"))
case("_is_session_cwd_junk", {"cwd": "/home/u/.hermes"}, io=junk_io("/home/u/.hermes"))
case("_is_session_cwd_junk", {"cwd": "/home/u/.hermes/ws"}, io=junk_io("/home/u/.hermes/ws"))  # NOT junk
case("_is_session_cwd_junk", {"cwd": "/home/u"}, io=junk_io("/home/u"))
case("_is_session_cwd_junk", {"cwd": "/x"}, fail_closed=True)

# ── _repo_discovery_policy ──
case("_repo_discovery_policy", {"raw": {"enabled": False, "roots": [" /a ", "", 5],
                                        "exclude_paths": ["/x"]}},
     io={"default_config": DEFAULTS})
case("_repo_discovery_policy", {"raw": {"repo_scan_enabled": False,
                                        "repo_scan_roots": ["/long"],
                                        "repo_scan_exclude_paths": []}},
     io={"default_config": DEFAULTS})                       # long-key fallback
case("_repo_discovery_policy", {"raw": None},
     io={"default_config": DEFAULTS, "load_cfg": {"desktop": {"enabled": True, "roots": ["/cfg"]}}})
case("_repo_discovery_policy", {"raw": "x"},
     io={"default_config": DEFAULTS, "load_cfg": {}})       # desktop absent -> {}
case("_repo_discovery_policy", {"raw": None},
     io={"default_config": DEFAULTS, "load_cfg": {"desktop": None}})  # or {}
case("_repo_discovery_policy", {"raw": None},
     io={"default_config": DEFAULTS, "load_cfg": {"desktop": "notdict"}})  # isinstance gate
case("_repo_discovery_policy", {"raw": {"enabled": "yes", "roots": "notlist"}},
     io={"default_config": DEFAULTS})                       # enabled default + list(defaults)
case("_repo_discovery_policy", {"raw": {"enabled": True}},
     io={"default_config": {"desktop": {}}})                # eager defaults[long] KeyError
case("_repo_discovery_policy", {"raw": {}}, io={"default_config": {}})  # KeyError 'desktop'
case("_repo_discovery_policy", {"raw": None},
     io={"default_config": DEFAULTS, "load_cfg": None})     # None.get AttributeError
case("_repo_discovery_policy", {"raw": {}}, fail_closed=True)
case("_repo_discovery_policy", {"raw": None},
     io={"default_config": DEFAULTS, "load_cfg": raise_script("RuntimeError", "cfg down")})

# ── _repo_discovery_policy_key ──
case("_repo_discovery_policy_key",
     {"policy": {"enabled": True, "roots": ["/a", "/a"], "exclude_paths": []}},
     io={"os.path.expanduser": eu_queue([["/a", "/a"], []])})
case("_repo_discovery_policy_key",
     {"policy": {"enabled": 1, "roots": ["~/code", "~/code", "~/other"],
                 "exclude_paths": ["~/x"]}},
     io={"os.path.expanduser": eu_queue([["~/code", "~/code", "~/other"], ["~/x"]])})
case("_repo_discovery_policy_key",
     {"policy": {"enabled": False, "roots": ["code", "/a/../b"], "exclude_paths": []}},
     io={"os.path.expanduser": eu_queue([["code", "/a/../b"], []])})
case("_repo_discovery_policy_key", {"policy": {}})          # KeyError 'enabled'
case("_repo_discovery_policy_key",
     {"policy": {"enabled": True, "roots": [], "exclude_paths": []}},
     io={"os.path.expanduser": eu_queue([[], []])})
case("_repo_discovery_policy_key",
     {"policy": {"enabled": True, "roots": ["/a"], "exclude_paths": []}},
     fail_closed=True)
case("_repo_discovery_policy_key",
     {"policy": {"enabled": True, "roots": ["/a"], "exclude_paths": []}},
     io={"os.path.expanduser": [HOME, raise_script("OSError", "no home")]})

# ── _repo_discovery_policy_is_default ──
POL_DEFAULT = {"enabled": True, "roots": ["/home/u/code"],
               "exclude_paths": ["/home/u/code/.cache"]}
case("_repo_discovery_policy_is_default", {"policy": POL_DEFAULT},
     io={"default_config": DEFAULTS,
         "os.path.expanduser": eu_queue([POL_DEFAULT["roots"], POL_DEFAULT["exclude_paths"]])
         + eu_queue([DEFAULTS["desktop"]["repo_scan_roots"],
                     DEFAULTS["desktop"]["repo_scan_exclude_paths"]])})
case("_repo_discovery_policy_is_default",
     {"policy": {"enabled": True, "roots": ["/other"], "exclude_paths": []}},
     io={"default_config": EMPTY_DEFAULTS,
         "os.path.expanduser": eu_queue([["/other"], []]) + eu_queue([[], []])})
case("_repo_discovery_policy_is_default",
     {"policy": {"enabled": False, "roots": [], "exclude_paths": []}},
     io={"default_config": EMPTY_DEFAULTS,
         "os.path.expanduser": eu_queue([[], []]) + eu_queue([[], []])})
case("_repo_discovery_policy_is_default", {"policy": {}},
     io={"default_config": DEFAULTS})                       # KeyError 'enabled'
case("_repo_discovery_policy_is_default", {"policy": POL_DEFAULT}, fail_closed=True)
case("_repo_discovery_policy_is_default",
     {"policy": {"enabled": True, "roots": [], "exclude_paths": []}},
     io={"default_config": EMPTY_DEFAULTS,
         "os.path.expanduser": eu_queue([[], []]) + eu_queue([[], []])})  # True

# ── _dir_exists_cached ──
case("_dir_exists_cached", {"path": "/w/a"}, io={"os.path.isdir": True})
case("_dir_exists_cached", {"path": "/w/a"}, io={"os.path.isdir": False})
case("_dir_exists_cached", {"path": "/w/a"}, state={"_DIR_EXISTS_CACHE": {"/w/a": True}})
case("_dir_exists_cached", {"path": "/w/a"}, state={"_DIR_EXISTS_CACHE": {"/w/a": False}})
case("_dir_exists_cached", {"path": "/w/b"},
     state={"_DIR_EXISTS_CACHE": {"/w/a": True}}, io={"os.path.isdir": True})
case("_dir_exists_cached", {"path": "/w/a"}, io={"os.path.isdir": raise_script("OSError", "denied")})
case("_dir_exists_cached", {"path": "/w/a"}, fail_closed=True)

# ── _require_project ──
case("_require_project", {"conn": "c1", "params": {"id": "p1"}},
     io={"pdb.get_project": {"id": "p1", "name": "P"}})
case("_require_project", {"conn": "c1", "params": {"id": "p1"}},
     io={"pdb.get_project": None})                          # _NoProject
case("_require_project", {"conn": "c1", "params": {"id": None}},
     io={"pdb.get_project": {"id": ""}})                    # str(None or "") == ""
case("_require_project", {"conn": "c1", "params": {"id": 5}},
     io={"pdb.get_project": {"id": "5"}})                   # str(5)
case("_require_project", {"conn": "c1", "params": None})    # None.get AttributeError
case("_require_project", {"conn": "c1", "params": {"id": "p1"}},
     io={"pdb.get_project": raise_script("RuntimeError", "db down")})
case("_require_project", {"conn": "c1", "params": {"id": "p1"}}, fail_closed=True)

# ── _projects_payload ──
case("_projects_payload", {"conn": "c1"},
     io={"pdb.list_projects": script([{"id": "p1"}, {"id": "p2"}]),
         "to_dict": [{"id": "p1", "name": "A"}, {"id": "p2", "name": "B"}],
         "pdb.get_active_id": "p1"})
case("_projects_payload", {"conn": "c1"},
     io={"pdb.list_projects": script([]), "pdb.get_active_id": None})
case("_projects_payload", {"conn": "c1"},
     io={"pdb.list_projects": script([{"id": "p1"}]), "to_dict": {"id": "p1"},
         "pdb.get_active_id": None})
case("_projects_payload", {"conn": "c1"},
     io={"pdb.list_projects": raise_script("RuntimeError", "db down")})
case("_projects_payload", {"conn": "c1"},
     io={"pdb.list_projects": script([{"id": "p1"}]),
         "to_dict": raise_script("ValueError", "bad project")})
case("_projects_payload", {"conn": "c1"}, io={"pdb.list_projects": None})  # not iterable
case("_projects_payload", {"conn": "c1"}, fail_closed=True)

# ── _projects_method ──
case("_projects_method", {"name": "projects.x", "rid": RID, "params": {"a": 1}},
     io={"connect_closing": "c1", "projects_fn": {"ok": 1}})
case("_projects_method", {"name": "projects.x", "rid": RID, "params": {}},
     io={"connect_closing": "c1",
         "projects_fn": raise_script("_NoProject", ""),
         "err": err_env(RID, 5062, "no such project")})
case("_projects_method", {"name": "projects.x", "rid": RID, "params": {}},
     io={"connect_closing": "c1",
         "projects_fn": raise_script("ValueError", "bad arg"),
         "err": err_env(RID, 5063, "bad arg")})
case("_projects_method", {"name": "projects.x", "rid": RID, "params": {}},
     io={"connect_closing": "c1",
         "projects_fn": raise_script("RuntimeError", "boom"),
         "err": err_env(RID, 5061, "boom")})
case("_projects_method", {"name": "projects.x", "rid": RID, "params": {}},
     io={"connect_closing": "c1", "projects_fn": raise_script("SystemExit", "bye")})
case("_projects_method", {"name": "projects.x", "rid": RID, "params": {}},
     io={"connect_closing": "c1", "projects_fn": raise_script("KeyboardInterrupt", "")})
case("_projects_method", {"name": "projects.x", "rid": RID, "params": {}},
     io={"connect_closing": raise_script("RuntimeError", "conn fail"),
         "err": err_env(RID, 5061, "conn fail")})
case("_projects_method", {"name": "projects.x", "rid": RID, "params": {}},
     io={"connect_closing": "c1",
         "projects_fn": raise_script("UnicodeError", "uni"),
         "err": err_env(RID, 5063, "uni")})                 # ValueError subclass
case("_projects_method", {"name": "projects.x", "rid": RID, "params": {}},
     io={"connect_closing": "c1",
         "projects_fn": raise_script("CustomErr", "custom"),
         "err": err_env(RID, 5061, "custom")})              # unknown -> Exception
case("_projects_method", {"name": "projects.y", "rid": RID, "params": {}},
     io={"connect_closing": "c1", "projects_fn": None},
     state={"_registry_pending": [["projects.old", "handler"]]})
case("_projects_method", {"name": "projects.x", "rid": RID, "params": {}}, fail_closed=True)

# ── _register_project_mutator ──
PROJ = {"id": "p1", "name": "P"}
PROJ_DICT = {"id": "p1", "name": "P2"}
case("_register_project_mutator",
     {"suffix": "update", "fn_name": "update_project", "takes_path": False,
      "rid": RID, "params": {"id": "p1", "name": "n"}},
     io={"connect_closing": "c1",
         "pdb.get_project": [PROJ, PROJ],
         "kwargs_of": {"name": "n", "description": None, "icon": None,
                       "color": None, "board_slug": None},
         "pdb.mutator": None, "to_dict": PROJ_DICT,
         "ok": ok_env(RID, {"project": PROJ_DICT})})
case("_register_project_mutator",
     {"suffix": "add_folder", "fn_name": "add_folder", "takes_path": True,
      "rid": RID, "params": {"id": "p1", "path": "/f", "label": "L", "is_primary": 1}},
     io={"connect_closing": "c1",
         "pdb.get_project": [PROJ, PROJ],
         "kwargs_of": {"label": "L", "is_primary": True},
         "pdb.mutator": "f1", "to_dict": PROJ_DICT,
         "ok": ok_env(RID, {"project": PROJ_DICT})})
case("_register_project_mutator",
     {"suffix": "remove_folder", "fn_name": "remove_folder", "takes_path": True,
      "rid": RID, "params": {"id": "p1", "path": "/f"}},
     io={"connect_closing": "c1",
         "pdb.get_project": [PROJ, PROJ], "kwargs_of": {},
         "pdb.mutator": True, "to_dict": PROJ_DICT,
         "ok": ok_env(RID, {"project": PROJ_DICT})})
case("_register_project_mutator",
     {"suffix": "set_primary", "fn_name": "set_primary", "takes_path": True,
      "rid": RID, "params": {"id": "p1", "path": None}},
     io={"connect_closing": "c1",
         "pdb.get_project": [PROJ, PROJ], "kwargs_of": {},
         "pdb.mutator": True, "to_dict": PROJ_DICT,
         "ok": ok_env(RID, {"project": PROJ_DICT})})        # path None -> ""
case("_register_project_mutator",
     {"suffix": "update", "fn_name": "update_project", "takes_path": False,
      "rid": RID, "params": {"id": "p1"}},
     io={"connect_closing": "c1", "pdb.get_project": None,
         "err": err_env(RID, 5062, "no such project")})
case("_register_project_mutator",
     {"suffix": "update", "fn_name": "update_project", "takes_path": False,
      "rid": RID, "params": {"id": "p1"}},
     io={"connect_closing": "c1", "pdb.get_project": PROJ,
         "kwargs_of": {}, "pdb.mutator": raise_script("ValueError", "bad folder"),
         "err": err_env(RID, 5063, "bad folder")})
case("_register_project_mutator",
     {"suffix": "update", "fn_name": "update_project", "takes_path": False,
      "rid": RID, "params": {"id": "p1"}},
     io={"connect_closing": "c1", "pdb.get_project": [PROJ, None],
         "kwargs_of": {}, "pdb.mutator": None,
         "err": err_env(RID, 5061, "'NoneType' object has no attribute 'to_dict'")})
case("_register_project_mutator",
     {"suffix": "update", "fn_name": "update_project", "takes_path": False,
      "rid": RID, "params": {"id": "p1"}},
     io={"connect_closing": "c1", "pdb.get_project": PROJ,
         "kwargs_of": {}, "pdb.mutator": raise_script("SystemExit", "bye")})
case("_register_project_mutator",
     {"suffix": "update", "fn_name": "update_project", "takes_path": False,
      "rid": RID, "params": {"id": "p1"}},
     io={"connect_closing": "c1", "pdb.get_project": {"name": "no id"},
         "err": err_env(RID, 5061, "'types.SimpleNamespace' object has no attribute 'id'")})
case("_register_project_mutator",
     {"suffix": "update", "fn_name": "update_project", "takes_path": False,
      "rid": RID, "params": {"id": "p1"}},
     io={"connect_closing": "c1", "pdb.get_project": PROJ,
         "kwargs_of": raise_script("RuntimeError", "kwargs fail"),
         "err": err_env(RID, 5061, "kwargs fail")})
case("_register_project_mutator",
     {"suffix": "update", "fn_name": "update_project", "takes_path": False,
      "rid": RID, "params": {"id": "p1"}}, fail_closed=True)

# ── _h_projects_list ──
case("_h_projects_list", {"rid": RID, "params": {}, "conn": "c1"},
     io={"pdb.list_projects": script([{"id": "p1"}]), "to_dict": {"id": "p1"},
         "pdb.get_active_id": "p1", "ok": ok_env(RID, {"projects": [{"id": "p1"}], "active_id": "p1"})})
case("_h_projects_list", {"rid": RID, "params": {}, "conn": "c1"},
     io={"pdb.list_projects": script([]), "pdb.get_active_id": None,
         "ok": ok_env(RID, {"projects": [], "active_id": None})})
case("_h_projects_list", {"rid": RID, "params": {}, "conn": "c1"},
     io={"pdb.list_projects": raise_script("RuntimeError", "db down")})
case("_h_projects_list", {"rid": RID, "params": {}, "conn": "c1"},
     io={"pdb.list_projects": script([{"id": "p1"}]),
         "to_dict": raise_script("ValueError", "bad")})
case("_h_projects_list", {"rid": RID, "params": {}, "conn": "c1"}, fail_closed=True)
case("_h_projects_list", {"rid": RID, "params": {}, "conn": None},
     io={"pdb.list_projects": script([]), "pdb.get_active_id": None,
         "ok": ok_env(RID, {"projects": [], "active_id": None})})

# ── _h_projects_get ──
case("_h_projects_get", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"},
     io={"pdb.get_project": PROJ, "to_dict": PROJ_DICT,
         "ok": ok_env(RID, {"project": PROJ_DICT})})
case("_h_projects_get", {"rid": RID, "params": {"id": "nope"}, "conn": "c1"},
     io={"pdb.get_project": None})                          # bare _NoProject raise
case("_h_projects_get", {"rid": RID, "params": {}, "conn": "c1"},
     io={"pdb.get_project": PROJ, "to_dict": PROJ_DICT,
         "ok": ok_env(RID, {"project": PROJ_DICT})})        # id absent -> ""
case("_h_projects_get", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"},
     io={"pdb.get_project": raise_script("RuntimeError", "db down")})
case("_h_projects_get", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"},
     io={"pdb.get_project": PROJ, "to_dict": raise_script("ValueError", "bad")})
case("_h_projects_get", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"}, fail_closed=True)

# ── _h_projects_create ──
CREATE_IO = {
    "pdb.create_project": "p9",
    "record_feature_used": None,
    "pdb.get_project": {"id": "p9"},
    "to_dict": {"id": "p9", "name": "N"},
    "ok": ok_env(RID, {"project": {"id": "p9", "name": "N"}}),
}
case("_h_projects_create", {"rid": RID, "params": {"name": "N", "use": True}, "conn": "c1"},
     io=dio(CREATE_IO, {"pdb.set_active": None}))
case("_h_projects_create", {"rid": RID, "params": {"name": "N"}, "conn": "c1"}, io=CREATE_IO)
case("_h_projects_create", {"rid": RID, "params": {"name": 5, "folders": None,
                                                   "slug": "s", "color": None}, "conn": "c1"},
     io=dio(CREATE_IO, {"pdb.create_project": "p9"}))
case("_h_projects_create", {"rid": RID, "params": {"name": "N"}, "conn": "c1"},
     io=dio(CREATE_IO, {"pdb.get_project": None,
                        "ok": ok_env(RID, {"project": None})}))  # proj null -> None
case("_h_projects_create", {"rid": RID, "params": {"name": "N"}, "conn": "c1"},
     io=dio(CREATE_IO, {"pdb.get_project": {}, "to_dict": {"id": ""},
                        "ok": ok_env(RID, {"project": {"id": ""}})}))  # {} double TRUTHY
case("_h_projects_create", {"rid": RID, "params": {"name": "N"}, "conn": "c1"},
     io={"pdb.create_project": raise_script("ValueError", "dup slug")})
case("_h_projects_create", {"rid": RID, "params": {"name": "N", "use": True}, "conn": "c1"},
     io=dio(CREATE_IO, {"pdb.set_active": raise_script("RuntimeError", "active fail")}))
case("_h_projects_create", {"rid": RID, "params": {"name": "N"}, "conn": "c1"},
     io=dict(CREATE_IO, record_feature_used=raise_script("SystemExit", "bye")))
case("_h_projects_create", {"rid": RID, "params": {"name": "N"}, "conn": "c1"}, fail_closed=True)

# ── _h_projects_archive ──
ARCH_IO = {
    "pdb.get_project": PROJ,
    "pdb.archive_project": True,
    "pdb.list_projects": script([PROJ]),
    "to_dict": PROJ_DICT,
    "pdb.get_active_id": None,
    "ok": ok_env(RID, {"projects": [PROJ_DICT], "active_id": None}),
}
case("_h_projects_archive", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"}, io=ARCH_IO)
case("_h_projects_archive", {"rid": RID, "params": {"id": "p1", "restore": True}, "conn": "c1"},
     io=dio(ARCH_IO, {"pdb.restore_project": True}))
case("_h_projects_archive", {"rid": RID, "params": {"id": "p1", "restore": 1}, "conn": "c1"},
     io=dio(ARCH_IO, {"pdb.restore_project": True}))
case("_h_projects_archive", {"rid": RID, "params": {"id": "nope"}, "conn": "c1"},
     io={"pdb.get_project": None})
case("_h_projects_archive", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"},
     io=dio(ARCH_IO, {"pdb.archive_project": raise_script("RuntimeError", "locked")}))
case("_h_projects_archive", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"}, fail_closed=True)

# ── _h_projects_delete ──
DEL_IO = {
    "pdb.get_project": PROJ,
    "pdb.delete_project": True,
    "pdb.list_projects": script([]),
    "pdb.get_active_id": None,
    "ok": ok_env(RID, {"projects": [], "active_id": None}),
}
case("_h_projects_delete", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"}, io=DEL_IO)
case("_h_projects_delete", {"rid": RID, "params": {"id": "nope"}, "conn": "c1"},
     io={"pdb.get_project": None})
case("_h_projects_delete", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"},
     io=dio(DEL_IO, {"pdb.delete_project": raise_script("RuntimeError", "locked")}))
case("_h_projects_delete", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"},
     io=dio(DEL_IO, {"pdb.list_projects": raise_script("RuntimeError", "db down")}))
case("_h_projects_delete", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"}, fail_closed=True)
case("_h_projects_delete", {"rid": RID, "params": {"id": 7}, "conn": "c1"},
     io=dio(DEL_IO, {"pdb.get_project": {"id": "7"}}))

# ── _h_projects_set_active ──
SA_IO = {
    "pdb.get_project": PROJ,
    "pdb.set_active": None,
    "pdb.get_active_id": "p1",
    "ok": ok_env(RID, {"active_id": "p1"}),
}
case("_h_projects_set_active", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"}, io=SA_IO)
case("_h_projects_set_active", {"rid": RID, "params": {}, "conn": "c1"},
     io={"pdb.set_active": None, "pdb.get_active_id": None,
         "ok": ok_env(RID, {"active_id": None})})           # no id -> None, no lookup
case("_h_projects_set_active", {"rid": RID, "params": {"id": ""}, "conn": "c1"},
     io={"pdb.set_active": None, "pdb.get_active_id": None,
         "ok": ok_env(RID, {"active_id": None})})           # falsy id
case("_h_projects_set_active", {"rid": RID, "params": {"id": "nope"}, "conn": "c1"},
     io={"pdb.get_project": None})
case("_h_projects_set_active", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"},
     io=dio(SA_IO, {"pdb.set_active": raise_script("RuntimeError", "fail")}))
case("_h_projects_set_active", {"rid": RID, "params": {"id": "p1"}, "conn": "c1"}, fail_closed=True)

# ── _h_projects_for_cwd ──
FC_IO = {
    "completion_cwd": "/w/a",
    "pdb.project_for_path": {"id": "p1"},
    "to_dict": PROJ_DICT,
    "git_probe.branch": "main",
    "ok": ok_env(RID, {"project": PROJ_DICT, "cwd": "/w/a", "branch": "main"}),
}
case("_h_projects_for_cwd", {"rid": RID, "params": {"cwd": " /w/a "}, "conn": "c1"}, io=FC_IO)
case("_h_projects_for_cwd", {"rid": RID, "params": {}, "conn": "c1"},
     io=dict(FC_IO, completion_cwd="/default"))             # {} arg
case("_h_projects_for_cwd", {"rid": RID, "params": {"cwd": None}, "conn": "c1"},
     io=dict(FC_IO, completion_cwd="/default"))
case("_h_projects_for_cwd", {"rid": RID, "params": {"cwd": "/w/a"}, "conn": "c1"},
     io=dio(FC_IO, {"pdb.project_for_path": None,
                    "ok": ok_env(RID, {"project": None, "cwd": "/w/a", "branch": "main"})}))
case("_h_projects_for_cwd", {"rid": RID, "params": {"cwd": "/w/a"}, "conn": "c1"},
     io=dio(FC_IO, {"pdb.project_for_path": {}, "to_dict": {"id": ""},
                    "ok": ok_env(RID, {"project": {"id": ""}, "cwd": "/w/a", "branch": "main"})}))
case("_h_projects_for_cwd", {"rid": RID, "params": {"cwd": "/w/a"}, "conn": "c1"},
     io=dio(FC_IO, {"git_probe.branch": raise_script("RuntimeError", "no git")}))
case("_h_projects_for_cwd", {"rid": RID, "params": {"cwd": "/w/a"}, "conn": "c1"},
     io={"completion_cwd": raise_script("RuntimeError", "cwd fail")})
case("_h_projects_for_cwd", {"rid": RID, "params": {"cwd": "/w/a"}, "conn": "c1"}, fail_closed=True)

# ── _scan_discovered_repos_remote ──
SCAN_POL = {"enabled": True, "roots": ["/r"], "exclude_paths": []}
SCAN_TREE = leaf({
    "repoA": leaf({".git": leaf(), "nested": gitdir()}, ["f.txt"]),
    "plain": leaf({"z": leaf()}),
    ".hidden": leaf({"x": leaf()}),
    "node_modules": leaf({"y": leaf()}),
})
case("_scan_discovered_repos_remote", {"conn": "c1", "policy": SCAN_POL},
     io={"os.path.isdir": [True], "os.walk": script(SCAN_TREE),
         "os.path.expanduser": eu_queue([["/r"], []]),
         "pdb.record_discovered_repos": None})
case("_scan_discovered_repos_remote", {"conn": "c1", "policy": SCAN_POL},
     io={"os.path.isdir": [False],
         "logger.debug": None})                             # missing root
case("_scan_discovered_repos_remote", {"conn": "c1", "policy": SCAN_POL},
     io={"os.path.isdir": [True],
         "os.walk": raise_script("OSError", "walk fail"),
         "logger.debug": None})
case("_scan_discovered_repos_remote",
     {"conn": "c1", "policy": {"enabled": True, "roots": ["/r"], "exclude_paths": ["/r/skip"]}},
     io={"os.path.isdir": [True],
         "os.walk": script(leaf({"skip": gitdir(), "ok": gitdir()})),
         "os.path.expanduser": eu_queue([["/r"], ["/r/skip"]]),
         "pdb.record_discovered_repos": None})
case("_scan_discovered_repos_remote", {"conn": "c1", "policy": SCAN_POL},
     io={"os.path.isdir": [True], "os.walk": script(SCAN_TREE),
         "os.path.expanduser": eu_queue([["/r"], []]),
         "pdb.record_discovered_repos": raise_script("RuntimeError", "cache fail"),
         "logger.debug": None})
case("_scan_discovered_repos_remote", {"conn": "c1", "policy": SCAN_POL},
     io={"os.path.isdir": [True], "os.walk": raise_script("SystemExit", "bye")})
case("_scan_discovered_repos_remote", {"conn": "c1", "policy": SCAN_POL},
     io={"os.path.isdir": [True], "os.walk": script(SCAN_TREE),
         "os.path.expanduser": eu_queue([["/r"], []]),
         "pdb.record_discovered_repos": raise_script("KeyboardInterrupt", "")})
case("_scan_discovered_repos_remote",
     {"conn": "c1", "policy": {"enabled": True, "roots": ["/r", "/r2"], "exclude_paths": []}},
     io={"os.path.isdir": [False, True],
         "logger.debug": None,
         "os.walk": script(leaf({"repoB": gitdir()})),
         "os.path.expanduser": eu_queue([["/r", "/r2"], []]),
         "pdb.record_discovered_repos": None})
case("_scan_discovered_repos_remote", {"conn": "c1", "policy": SCAN_POL}, fail_closed=True)
case("_scan_discovered_repos_remote", {"conn": "c1", "policy": SCAN_POL},
     io={"os.path.isdir": [False], "logger.debug": raise_script("RuntimeError", "log fail")})
case("_scan_discovered_repos_remote", {"conn": "c1", "policy": {"enabled": True, "roots": [], "exclude_paths": []}},
     io={})                                                 # no roots -> True, no seams
# cap-500 case: 500 git dirs under /r, second root never probed
CAP_TREE = leaf({f"r{i:03d}": gitdir() for i in range(500)})
case("_scan_discovered_repos_remote",
     {"conn": "c1", "policy": {"enabled": True, "roots": ["/r", "/r2"], "exclude_paths": []}},
     io={"os.path.isdir": [True], "os.walk": script(CAP_TREE),
         "os.path.expanduser": eu_queue([["/r", "/r2"], []]),
         "pdb.record_discovered_repos": None})
# exclude with trailing slashes/backslashes (rstrip("/\\") + sep)
case("_scan_discovered_repos_remote",
     {"conn": "c1", "policy": {"enabled": True, "roots": ["/r"], "exclude_paths": ["/r/skip/\\"]}},
     io={"os.path.isdir": [True],
         "os.walk": script(leaf({"skip": gitdir(), "ok": gitdir()})),
         "os.path.expanduser": eu_queue([["/r"], ["/r/skip/\\"]]),
         "pdb.record_discovered_repos": None})

# ── _discover_repos_payload ──
ROWS2 = [{"cwd": "/w/a", "sessions": 2, "last_active": 10.0},
         {"cwd": "/w/b", "sessions": 1, "last_active": 5.0}]
DISC_IO = {
    "persistence.distinct_session_cwds": script(ROWS2),
    "git_probe.warm_roots": None,
    "git_probe.common_repo_root": ["/w/a", "/w/b"],
    "persistence.backfill_repo_roots": None,
    "pdb.list_discovered_repos": script([{"root": "/w/c", "label": "C"}]),
}
DISC_IO.update(junk_io_many(["/w/a", "/w/b", "/w/c"]))
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "backfill": True, "include_cached": True,
      "persistence": {"read_only": False, "sessions": {}}}, io=DISC_IO)
case("_discover_repos_payload",
     {"db": "db1", "conn": "c1", "persistence": None},
     io=dict(DISC_IO, persistence_for={"read_only": True, "sessions": {}}))  # yielded read-only handle
case("_discover_repos_payload",
     {"db": "db1", "conn": "c1", "persistence": None},
     io={"persistence_for": None, "git_probe.warm_roots": None,
         "pdb.list_discovered_repos": script([])})           # no handle -> [] rows
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "persistence": {"read_only": False, "sessions": {}}},
     io=dio(DISC_IO, {"persistence.backfill_repo_roots": raise_script("RuntimeError", "bf fail"),
                      "logger.debug": None}))
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "persistence": {"read_only": False, "sessions": {}}},
     io=dict(DISC_IO, **{"persistence.backfill_repo_roots": raise_script("SystemExit", "bye")}))
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "persistence": {"read_only": True, "sessions": {}}},
     io=dict(DISC_IO, **{"pdb.list_discovered_repos": raise_script("RuntimeError", "cache fail"),
                         "logger.debug": None}))
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "backfill": True, "include_cached": False,
      "persistence": {"read_only": False, "sessions": {}}},
     io={k: v for k, v in DISC_IO.items() if k != "pdb.list_discovered_repos"})
# junk root: cwd_to_root keeps it, agg drops it
J_IO = {
    "persistence.distinct_session_cwds": script([{"cwd": "/home/u", "sessions": 3, "last_active": 1.0}]),
    "git_probe.warm_roots": None,
    "git_probe.common_repo_root": ["/home/u"],
    "persistence.backfill_repo_roots": None,
    "pdb.list_discovered_repos": script([]),
}
J_IO.update(junk_io_many(["/home/u"]))
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "persistence": {"read_only": False, "sessions": {}}}, io=J_IO)
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "persistence": {"read_only": False, "sessions": {}}},
     io={"persistence.distinct_session_cwds": script([{"sessions": 1, "last_active": 0}]),
         "git_probe.warm_roots": None})                      # row missing cwd
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "persistence": {"read_only": False, "sessions": {}}},
     io=dio({"persistence.distinct_session_cwds": script([{"cwd": "/w/a", "sessions": "abc", "last_active": 0}]),
             "git_probe.warm_roots": None, "git_probe.common_repo_root": ["/w/a"]},
            junk_io_many(["/w/a"])))
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "persistence": {"read_only": False, "sessions": {}}}, fail_closed=True)
case("_discover_repos_payload",
     {"db": None, "conn": None, "persistence": {"read_only": True, "sessions": {}}},
     io=dict(DISC_IO, connect_closing="c2"))                 # nullcontext -> connect_closing
case("_discover_repos_payload",
     {"db": None, "conn": None, "persistence": {"read_only": True, "sessions": {}}},
     io=dict(DISC_IO, connect_closing=raise_script("RuntimeError", "conn fail"),
             **{"logger.debug": None}))
# cached entry: falsy label keeps session label; empty root skipped; junk cached root
C_IO = {
    "persistence.distinct_session_cwds": script([{"cwd": "/w/a", "sessions": 1, "last_active": 2.0}]),
    "git_probe.warm_roots": None,
    "git_probe.common_repo_root": ["/w/a"],
    "persistence.backfill_repo_roots": None,
    "pdb.list_discovered_repos": script([{"root": "/w/a", "label": ""},
                                         {"root": "", "label": "X"},
                                         {"root": "/home/u", "label": "J"}]),
}
C_IO.update(junk_io_many(["/w/a", "/w/a", "/home/u"]))
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "persistence": {"read_only": False, "sessions": {}}}, io=C_IO)
# last_active tie -> stable insertion order; string coercion
T_IO = {
    "persistence.distinct_session_cwds": script([{"cwd": "/w/a", "sessions": "2", "last_active": "5"},
                                                 {"cwd": "/w/b", "sessions": 1, "last_active": 5.0}]),
    "git_probe.warm_roots": None,
    "git_probe.common_repo_root": ["/w/a", "/w/b"],
    "persistence.backfill_repo_roots": None,
    "pdb.list_discovered_repos": script([]),
}
T_IO.update(junk_io_many(["/w/a", "/w/b"]))
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "persistence": {"read_only": False, "sessions": {}}}, io=T_IO)
# persistence missing read_only -> AttributeError OUTSIDE the try
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "backfill": True, "persistence": {"sessions": {}}},
     io={"persistence.distinct_session_cwds": script([]), "git_probe.warm_roots": None})
# no scripts at all: the sessions recorder fires fail-closed
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "persistence": {"read_only": False, "sessions": {}}},
     io={}, fail_closed=True)
# cached entries not iterable -> caught inside try
case("_discover_repos_payload",
     {"db": None, "conn": "c1", "persistence": {"read_only": True, "sessions": {}}},
     io={"persistence.distinct_session_cwds": script([]), "git_probe.warm_roots": None,
         "pdb.list_discovered_repos": None, "logger.debug": None})

# ── _project_tree_inputs ──
TI_IO = {
    "SessionQuery": None,
    "SessionWindow": None,
    "persistence.list_sessions": script([entry(session=dict(SESSION, cwd="/w/a"))]),
    "git_probe.warm_roots": None,
    "default_config": EMPTY_DEFAULTS,
    "load_cfg": {"desktop": {"repo_scan_enabled": True, "repo_scan_roots": [],
                             "repo_scan_exclude_paths": []}},
    "os.path.expanduser": HOME,
    "connect_closing": "c1",
    "pdb.list_projects": script([{"id": "p1"}]),
    "to_dict": {"id": "p1", "folders": []},
    "pdb.get_active_id": "p1",
}
case("_project_tree_inputs",
     {"persistence": {"read_only": True, "sessions": {}}, "session_limit": 50, "include_discovered": False},
     io=TI_IO)
TI_DISC_IO = dict(TI_IO)
TI_DISC_IO.update({
    "persistence.distinct_session_cwds": script([]),
    "pdb.list_discovered_repos": script([]),
    "pdb.reconcile_discovered_repos_policy": None,
    "os.path.expanduser": HOME,   # all expanduser calls are "~" here
})
case("_project_tree_inputs",
     {"persistence": {"read_only": True, "sessions": {}}, "session_limit": 50, "include_discovered": True},
     io=TI_DISC_IO)
case("_project_tree_inputs",
     {"persistence": {"read_only": True, "sessions": {}}, "include_discovered": True,
      "session_limit": 10},
     io=dict(TI_DISC_IO, load_cfg={"desktop": {"repo_scan_enabled": False,
                                               "repo_scan_roots": [],
                                               "repo_scan_exclude_paths": []}}))
case("_project_tree_inputs",
     {"persistence": {"read_only": True, "sessions": {}}, "include_discovered": False},
     io=dict(TI_IO, **{"persistence.list_sessions": script([{"lineage_root_id": "s1"}])}))
case("_project_tree_inputs",
     {"persistence": {"read_only": True, "sessions": {}}, "include_discovered": False},
     io=dict(TI_IO, SessionQuery=raise_script("RuntimeError", "query fail")))
case("_project_tree_inputs",
     {"persistence": {"read_only": True, "sessions": {}}, "include_discovered": False}, fail_closed=True)
case("_project_tree_inputs",
     {"persistence": {"read_only": True, "sessions": {}}, "include_discovered": False},
     io=dict(TI_IO, connect_closing=raise_script("SystemExit", "bye")))
case("_project_tree_inputs",
     {"persistence": {"read_only": True, "sessions": {}}, "include_discovered": False},
     io=dict(TI_IO, **{"persistence.list_sessions": script([]),
                       "pdb.list_projects": script([]), "pdb.get_active_id": None}))

# ── _build_project_tree ──
BT_IO = dict(TI_IO)
BT_IO.update({
    "to_dict": {"id": "p1", "folders": [{"path": "/f1"}, {"path": None}]},
    "project_tree.build_tree": {"projects": [], "scoped_session_ids": []},
})
case("_build_project_tree",
     {"db": None, "preview_limit": 3, "hydrate": False, "session_limit": 50,
      "include_discovered": False, "persistence": {"read_only": True, "sessions": {}}},
     io=BT_IO, state={"_DIR_EXISTS_CACHE": {"/stale": True}})
case("_build_project_tree",
     {"db": "db1", "include_discovered": False, "persistence": None},
     io=dict(BT_IO, persistence_for={"read_only": True, "sessions": {}}))
case("_build_project_tree",
     {"db": None, "include_discovered": False, "persistence": {"read_only": True, "sessions": {}}},
     io=dict(BT_IO, **{"project_tree.build_tree": raise_script("RuntimeError", "tree fail")}))
case("_build_project_tree",
     {"db": None, "include_discovered": False, "persistence": {"read_only": True, "sessions": {}}},
     fail_closed=True)
BT_DISC_IO = dict(TI_DISC_IO)
BT_DISC_IO.update({
    "to_dict": {"id": "p1", "folders": []},
    "project_tree.build_tree": {"projects": [], "scoped_session_ids": []},
})
case("_build_project_tree",
     {"db": None, "preview_limit": 5, "hydrate": True, "session_limit": 20,
      "include_discovered": True, "persistence": {"read_only": True, "sessions": {}}},
     io=BT_DISC_IO)
case("_build_project_tree",
     {"db": None, "include_discovered": False, "persistence": {"read_only": True, "sessions": {}}},
     io=dict(BT_IO, **{"persistence.list_sessions": script([]),
                       "pdb.list_projects": script([]), "pdb.get_active_id": None,
                       "project_tree.build_tree": {"projects": [], "scoped_session_ids": []}}))


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = [f for f, n in counts.items() if n < 5]
    print(f"GEN tui_methods_projects_flow: {len(CASES)} cases, {len(counts)} funcs")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if thin:
        print(f"THIN (<5): {thin}")
        raise SystemExit(1)


if __name__ == "__main__":
    main()