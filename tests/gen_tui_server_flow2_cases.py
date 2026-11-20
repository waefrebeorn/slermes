#!/usr/bin/env python3
"""Generate tests/fixtures/tui_server_flow2.jsonl for the second-wave
tui_gateway/server.py FLOW + leaf port (src/gateway/tui_server_flow2.c).

Each case is {"func": <live name>, "in": {...}} where "in" carries:
  - "state": the injected state mirror (only {"cancel": [tokens]} is mutated)
  - "io":    scripted universal-seam results (bare array = per-call queue,
             {"__script__": true, "ret": v} = constant list/object, else constant;
             {"__raise__": kind, "message": m} = a raised exception)
  - the call arguments for the target function.

>= 5 cases per function. Pure-Python, no quarry import needed. Run:
  python tests/gen_tui_server_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tui_server_flow2.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def st(cancel=None):
    return {"cancel": list(cancel or [])}


def raise_spec(kind, message=""):
    return {"__raise__": kind, "message": message}


# ── _ws_orphan_setting ──────────────────────────────────────────────────────
add("_ws_orphan_setting", env_var="EV", cfg_key="g", default=20.0,
    io={"environ_get": "12"})
add("_ws_orphan_setting", env_var="EV", cfg_key="g", default=20.0,
    io={"environ_get": "", "load_config": {"dashboard": {"g": "5"}}})
add("_ws_orphan_setting", env_var="EV", cfg_key="g", default=20.0,
    io={"environ_get": None, "load_config": None})
add("_ws_orphan_setting", env_var="EV", cfg_key="g", default=15.0,
    io={"environ_get": "   ", "load_config": {"dashboard": {}}})
add("_ws_orphan_setting", env_var="EV", cfg_key="g", default=20.0,
    io={"environ_get": "-5"})
add("_ws_orphan_setting", env_var="EV", cfg_key="g", default=20.0,
    io={"environ_get": "abc"})
add("_ws_orphan_setting", env_var="EV", cfg_key="g", default=20.0,
    io={"environ_get": None, "load_config": raise_spec("RuntimeError", "no cfg")})
add("_ws_orphan_setting", env_var="EV", cfg_key="g", default=20.0,
    io={"environ_get": None, "load_config": raise_spec("SystemExit", "bye")})
add("_ws_orphan_setting", env_var="EV", cfg_key="g", default=20.0,
    io={"environ_get": " 7 "})
add("_ws_orphan_setting", env_var="EV", cfg_key="g", default=9.0,
    io={"environ_get": None, "load_config": {"dashboard": "bad"}})

# ── _resolve_ws_orphan_reap_grace ───────────────────────────────────────────
add("_resolve_ws_orphan_reap_grace", io={"_ws_orphan_setting": 20.0})
add("_resolve_ws_orphan_reap_grace", io={"_ws_orphan_setting": 0})
add("_resolve_ws_orphan_reap_grace", io={"_ws_orphan_setting": 1.5})
add("_resolve_ws_orphan_reap_grace", io={"_ws_orphan_setting": raise_spec("SystemExit", "bye")})
add("_resolve_ws_orphan_reap_grace", io={"_ws_orphan_setting": raise_spec("ValueError", "bad")})
add("_resolve_ws_orphan_reap_grace", io={"_ws_orphan_setting": None})

# ── _configured_cwd_from_cfg ────────────────────────────────────────────────
add("_configured_cwd_from_cfg", cfg={"terminal": {"cwd": " /work "}},
    io={"expanduser": "/work", "abspath": "/work2", "isdir": True})
add("_configured_cwd_from_cfg", cfg={"terminal": {"cwd": "."}}, io={})
add("_configured_cwd_from_cfg", cfg={"terminal": {"cwd": ""}}, io={})
add("_configured_cwd_from_cfg", cfg={}, io={})
add("_configured_cwd_from_cfg", cfg={"terminal": {"cwd": "~/proj"}},
    io={"expanduser": "/home/u/proj", "abspath": "/h/p", "isdir": False})
add("_configured_cwd_from_cfg", cfg={"terminal": {"cwd": "/x"}},
    io={"expanduser": raise_spec("RuntimeError", "e")})
add("_configured_cwd_from_cfg", cfg={"terminal": {"cwd": "/x"}},
    io={"expanduser": "/x", "abspath": raise_spec("ValueError", "e")})
add("_configured_cwd_from_cfg", cfg={"terminal": {"cwd": "/x"}},
    io={"expanduser": "/x", "abspath": "/x", "isdir": raise_spec("SystemExit", "bye")})
add("_configured_cwd_from_cfg", cfg={"terminal": {"cwd": "auto"}}, io={})
add("_configured_cwd_from_cfg", cfg=None, io={})

# ── _default_session_cwd ────────────────────────────────────────────────────
add("_default_session_cwd", io={"launch_configured_cwd": "/a"})
add("_default_session_cwd", io={"launch_configured_cwd": None, "getenv": "/b"})
add("_default_session_cwd", io={"launch_configured_cwd": "", "getenv": "", "getcwd": "/c"})
add("_default_session_cwd", io={"launch_configured_cwd": raise_spec("SystemExit", "bye")})
add("_default_session_cwd", io={"launch_configured_cwd": None,
                                "getenv": raise_spec("ValueError", "bad")})
add("_default_session_cwd", io={"launch_configured_cwd": None, "getenv": None,
                                "getcwd": raise_spec("RuntimeError", "cwd")})
add("_default_session_cwd", io={"launch_configured_cwd": 0, "getenv": None, "getcwd": "/d"})

# ── _pet_cfg ────────────────────────────────────────────────────────────────
add("_pet_cfg", io={"load_config": {"display": {"pet": {"enabled": True}}}})
add("_pet_cfg", io={"load_config": {}})
add("_pet_cfg", io={"load_config": {"display": "bad"}})
add("_pet_cfg", io={"load_config": {"display": {"pet": "bad"}}})
add("_pet_cfg", io={"load_config": raise_spec("RuntimeError", "cfg")})
add("_pet_cfg", io={"load_config": raise_spec("SystemExit", "bye")})
add("_pet_cfg", io={"load_config": None})

# ── _pet_config_scale ───────────────────────────────────────────────────────
add("_pet_config_scale", io={"pet_cfg": {"scale": 0.5}})
add("_pet_config_scale", io={"pet_cfg": {}})
add("_pet_config_scale", io={"pet_cfg": {"scale": 0}})
add("_pet_config_scale", io={"pet_cfg": raise_spec("RuntimeError", "cfg")})
add("_pet_config_scale", io={"pet_cfg": raise_spec("SystemExit", "bye")})
add("_pet_config_scale", io={"pet_cfg": {"scale": "0.75"}})
add("_pet_config_scale", io={"pet_cfg": {"scale": "bad"}})

# ── _pet_cancel_arm ─────────────────────────────────────────────────────────
add("_pet_cancel_arm", token="a", state=st(["a", "b"]))
add("_pet_cancel_arm", token="x", state=st([]))
add("_pet_cancel_arm", token="z", state=st(["a"]))
add("_pet_cancel_arm", token="b", state=st(["a", "b", "c"]))
add("_pet_cancel_arm", token="a", state=st(["a"]))

# ── _pet_cancel_request ─────────────────────────────────────────────────────
add("_pet_cancel_request", token="a", state=st([]))
add("_pet_cancel_request", token="a", state=st(["a"]))
add("_pet_cancel_request", token="a", state=st(["b"]))
add("_pet_cancel_request", token="", state=st([]))
add("_pet_cancel_request", token="c", state=st(["a", "b"]))

# ── _pet_is_cancelled ───────────────────────────────────────────────────────
add("_pet_is_cancelled", token="a", state=st(["a", "b"]))
add("_pet_is_cancelled", token="a", state=st([]))
add("_pet_is_cancelled", token="b", state=st(["a"]))
add("_pet_is_cancelled", token="a", state=st(["a"]))
add("_pet_is_cancelled", token="", state=st([]))

# ── _response_profile_name ──────────────────────────────────────────────────
add("_response_profile_name", profile="foo",
    io={"canonical_profile_request": "foo", "profile_home": "/h/foo"})
add("_response_profile_name", profile="  foo  ",
    io={"canonical_profile_request": "foo", "profile_home": None, "current_profile_name": "launch"})
add("_response_profile_name", profile="",
    io={"canonical_profile_request": "", "current_profile_name": "launch"})
add("_response_profile_name", profile=None,
    io={"canonical_profile_request": None, "current_profile_name": "def"})
add("_response_profile_name", profile="foo",
    io={"canonical_profile_request": "foo",
        "profile_home": raise_spec("ProfileUnavailableError", "gone"),
        "current_profile_name": "launch"})
add("_response_profile_name", profile="foo",
    io={"canonical_profile_request": "foo",
        "profile_home": raise_spec("RuntimeError", "boom")})
add("_response_profile_name", profile="foo",
    io={"canonical_profile_request": "foo", "profile_home": None, "current_profile_name": "cur"})

# ── _pet_gen_root ───────────────────────────────────────────────────────────
add("_pet_gen_root", io={"get_hermes_home": "/h", "mkdir": None})
add("_pet_gen_root", io={"get_hermes_home": "/h/", "mkdir": None})
add("_pet_gen_root", io={"get_hermes_home": "/", "mkdir": None})
add("_pet_gen_root", io={"get_hermes_home": raise_spec("RuntimeError", "home")})
add("_pet_gen_root", io={"get_hermes_home": "/h", "mkdir": raise_spec("ValueError", "mkdir")})
add("_pet_gen_root", io={"get_hermes_home": "/h", "mkdir": raise_spec("SystemExit", "bye")})

# ── _spawn_trees_root ───────────────────────────────────────────────────────
add("_spawn_trees_root", io={"get_hermes_home": "/h", "mkdir": None})
add("_spawn_trees_root", io={"get_hermes_home": "/h/", "mkdir": None})
add("_spawn_trees_root", io={"get_hermes_home": "/", "mkdir": None})
add("_spawn_trees_root", io={"get_hermes_home": raise_spec("RuntimeError", "home")})
add("_spawn_trees_root", io={"get_hermes_home": "/h", "mkdir": raise_spec("OSError", "mkdir")})

# ── _spawn_tree_session_dir ─────────────────────────────────────────────────
add("_spawn_tree_session_dir", session_id="abc123",
    io={"spawn_trees_root": "/h/spawn-trees", "mkdir": None})
add("_spawn_tree_session_dir", session_id="a b/c",
    io={"spawn_trees_root": "/h/spawn-trees", "mkdir": None})
add("_spawn_tree_session_dir", session_id="",
    io={"spawn_trees_root": "/h/spawn-trees", "mkdir": None})
add("_spawn_tree_session_dir", session_id="A-1_z",
    io={"spawn_trees_root": "/h/spawn-trees", "mkdir": None})
add("_spawn_tree_session_dir", session_id="x",
    io={"spawn_trees_root": raise_spec("RuntimeError", "root")})
add("_spawn_tree_session_dir", session_id="x",
    io={"spawn_trees_root": "/h/spawn-trees", "mkdir": raise_spec("ValueError", "mkdir")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    funcs = sorted(counts)
    short = [k for k in funcs if counts[k] < 5]
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(funcs), OUT))
    for k in funcs:
        print("  %-34s %d" % (k, counts[k]))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)


if __name__ == "__main__":
    main()
