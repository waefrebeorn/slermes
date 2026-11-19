#!/usr/bin/env python3
"""Generate tests/fixtures/config_migrations_flow2.jsonl for the
config_migrations_flow2 FLOW oracle. Emits fixture INPUTS only (scripts +
inputs); the oracle computes expected values from the live quarry defs.
>= 5 cases per ported function, covering happy / skip / boundary / fail-closed
/ BaseException-propagation / import-failure paths. ASCII fixture domain.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "config_migrations_flow2.jsonl")

CFG = {"ret": "module"}            # cfg seam marker (module identity)
SE = {"type": "SystemExit", "message": "1", "code": 1}
KI = {"type": "KeyboardInterrupt", "message": "ki"}
GE = {"type": "GeneratorExit", "message": "ge"}
OSERR = {"type": "OSError", "message": "disk error"}
VALERR = {"type": "ValueError", "message": "bad value"}
FNFE = {"type": "FileNotFoundError", "message": "no file"}


def imp_fail(mod):
    return {"type": "ModuleNotFoundError",
            "message": f"import of {mod} halted; None in sys.modules", "name": mod}


def R(config=None, quiet=True, added=None, message=None):
    d = {"results": {"config_added": [], "warnings": []}, "quiet": quiet}
    if config is not None:
        d["state"] = {"raw_config": config}
    if added is not None:
        d["added"] = added
    if message is not None:
        d["message"] = message
    return d


CASES = []


def add(func, inp):
    CASES.append({"func": func, "in": inp})


# ── local_files_allowed ───────────────────────────────────────────────────
add("local_files_allowed", {"io": {"CONFIG_ONLY.get": {"ret": False}}})
add("local_files_allowed", {"io": {"CONFIG_ONLY.get": {"ret": True}}})
add("local_files_allowed", {"io": {}})                                   # fail closed
add("local_files_allowed", {"io": {"CONFIG_ONLY.get": {"raise": SE}}})
add("local_files_allowed", {"io": {"CONFIG_ONLY.get": {"raise": KI}}})
add("local_files_allowed", {"io": {"CONFIG_ONLY.get": {"ret": None}}})

# ── read_raw_config ───────────────────────────────────────────────────────
add("read_raw_config", {"state": {"raw_config": {"k": 1}}, "io": {"cfg": CFG}})
add("read_raw_config", {"state": {"raw_config": {"k": 1}}, "io": {"cfg": CFG, "config.read_raw_config": {"ret": {"override": True}}}})
add("read_raw_config", {"state": {"raw_config": {"k": 1}}, "io": {"cfg": CFG, "config.read_raw_config": {"ret": None}}})
add("read_raw_config", {"state": {"raw_config": {}}, "io": {}})          # cfg fail closed
add("read_raw_config", {"state": {"raw_config": {}}, "io": {"cfg": {"raise": OSERR}}})
add("read_raw_config", {"state": {"raw_config": {}}, "io": {"cfg": CFG, "config.read_raw_config": {"raise": SE}}})

# ── persist_migration ─────────────────────────────────────────────────────
add("persist_migration", {"config": {"a": 1}, "io": {"cfg": CFG}})
add("persist_migration", {"config": {}, "io": {"cfg": CFG}})
add("persist_migration", {"config": {"a": {"b": 2}}, "io": {"cfg": CFG, "config._persist_migration": {"raise": OSERR}}})
add("persist_migration", {"config": {"a": 1}, "io": {"cfg": {"raise": SE}}})
add("persist_migration", {"config": {"a": 1}, "io": {}})                 # cfg fail closed
add("persist_migration", {"config": {"nested": {"x": [1, 2]}}, "io": {"cfg": CFG}})

# ── dict_at ───────────────────────────────────────────────────────────────
add("dict_at", {"config": {"a": {"x": 1}}, "key": "a"})
add("dict_at", {"config": {"a": 5}, "key": "a"})
add("dict_at", {"config": {}, "key": "a"})
add("dict_at", {"config": {"a": None}, "key": "a"})
add("dict_at", {"config": {"a": [1, 2]}, "key": "a"})
add("dict_at", {"config": {"a": {"deep": {"y": 2}}}, "key": "a"})

# ── commit ────────────────────────────────────────────────────────────────
add("commit", {"config": {"a": 1}, "results": {"config_added": []}, "quiet": False, "added": "x", "message": "hello", "io": {"cfg": CFG}})
add("commit", {"config": {"a": 1}, "results": {"config_added": []}, "quiet": True, "added": "x", "message": "hello", "io": {"cfg": CFG}})
add("commit", {"config": {"a": 1}, "results": {"config_added": []}, "quiet": False, "added": None, "message": "hello", "io": {"cfg": CFG}})
add("commit", {"config": {"a": 1}, "results": {"config_added": []}, "quiet": False, "added": "x", "message": None, "io": {"cfg": CFG}})
add("commit", {"config": {"a": 1}, "results": {"config_added": []}, "quiet": False, "added": "", "message": "", "io": {"cfg": CFG}})
add("commit", {"config": {"a": 1}, "results": {}, "quiet": True, "added": "x", "message": "m", "io": {"cfg": CFG}})  # KeyError
add("commit", {"config": {"a": 1}, "results": {"config_added": 5}, "quiet": True, "added": "x", "message": "m", "io": {"cfg": CFG}})  # AttributeError
add("commit", {"config": {"a": 1}, "results": {"config_added": []}, "quiet": False, "added": "x", "message": "m", "io": {"cfg": CFG, "config._persist_migration": {"raise": OSERR}}})
add("commit", {"config": {"a": 1}, "results": {"config_added": []}, "quiet": False, "added": "x", "message": "m", "io": {"cfg": CFG, "print": {"raise": SE}}})

# ── rewrite_key ───────────────────────────────────────────────────────────
def rk(**kw):
    base = {"results": {"config_added": []}, "quiet": True, "section": "agent",
            "key": "k", "match": {"kind": "eq", "value": 1}, "new": 2,
            "added": "a", "message": "m", "io": {"cfg": CFG}}
    base.update(kw)
    return base

add("rewrite_key", rk(state={"raw_config": {"agent": {"k": 1}}}))
add("rewrite_key", rk(state={"raw_config": {"agent": {"k": 9}}}))        # no match
add("rewrite_key", rk(state={"raw_config": {"agent": {"k": 1}}}, new=None))  # delete
add("rewrite_key", rk(state={"raw_config": {"agent": {}}}, new=None, match={"kind": "always_true"}))  # KeyError del absent
add("rewrite_key", rk(state={"raw_config": {"agent": {"k": " AUTO "}}}, match={"kind": "lower_is", "word": "auto"}, new=False))
add("rewrite_key", rk(state={"raw_config": {"agent": {}}}, match={"kind": "none_or_lower_is", "word": "auto"}, new=False))
add("rewrite_key", rk(state={"raw_config": {"agent": {"k": 0}}}, match={"kind": "not_none"}, new=None))
add("rewrite_key", rk(state={"raw_config": {"agent": {"k": 1}}}, match={"kind": "always_true"}, extra_guard={"kind": "has_key", "key": "k"}, new=None))
add("rewrite_key", rk(state={"raw_config": {"agent": {"k": 1}}}, match={"kind": "always_true"}, extra_guard={"kind": "lacks_key", "key": "k"}, new=None))  # guard skip
add("rewrite_key", rk(state={"raw_config": {}}, create_section=True, new=False))
add("rewrite_key", rk(state={"raw_config": {}}))                         # missing section, no create
add("rewrite_key", rk(state={"raw_config": {"agent": {"k": 1}}}, match={"kind": "always_true"}, extra_guard={"kind": "is_true", "key": "k"}, new=None))  # 1 is not True
add("rewrite_key", rk(state={"raw_config": {"agent": {"k": True}}}, match={"kind": "always_true"}, extra_guard={"kind": "is_true", "key": "k"}, new=None))
add("rewrite_key", rk(state={"raw_config": {}}, io={}))                  # cfg fail closed
add("rewrite_key", rk(state={"raw_config": {"agent": {"k": 1}}}, quiet=False, io={"cfg": CFG, "print": {"raise": GE}}))

# ── rewrite_stale_default ─────────────────────────────────────────────────
add("rewrite_stale_default", {"old": 24, "kw": {"section": "model_catalog", "key": "ttl_hours", "new": 1, "added": "a", "message": "m"}})
add("rewrite_stale_default", {"old": None, "kw": {"section": "s", "key": "k", "new": None, "added": "a", "message": "m"}})
add("rewrite_stale_default", {"old": True, "kw": {"section": "agent", "key": "verify_on_stop", "new": False, "added": "a", "message": "m"}})
add("rewrite_stale_default", {"old": "node24", "kw": {"section": "terminal", "key": "vercel_runtime", "new": None, "added": "a", "message": "m", "create_section": True}})
add("rewrite_stale_default", {"old": 3600, "kw": {"section": "agent", "key": "clarify_timeout", "new": 90, "added": "a", "message": "m"}})

# ── rewrite_stale_defaults ────────────────────────────────────────────────
def rsd(rows, config, **kw):
    base = {"results": {"config_added": []}, "quiet": True, "section": "curator",
            "rows": rows, "state": {"raw_config": config}, "io": {"cfg": CFG}}
    base.update(kw)
    return base

row0 = ["stale_after_days", 30, 14, "a0", "m0"]
row1 = ["archive_after_days", 90, 30, "a1", "m1"]
add("rewrite_stale_defaults", rsd([row0], {"curator": {"stale_after_days": 30}}))
add("rewrite_stale_defaults", rsd([row0, row1], {"curator": {"stale_after_days": 30, "archive_after_days": 90}}))
add("rewrite_stale_defaults", rsd([row0, row1], {"curator": {"stale_after_days": 7}}))  # no due
add("rewrite_stale_defaults", rsd([["k", 1, None, "a", "m"]], {"curator": {"k": 1}}))  # delete
add("rewrite_stale_defaults", rsd([["k", None, 5, "a", "m"]], {}))       # detached fresh raw
add("rewrite_stale_defaults", rsd([["k", None, None, "a", "m"]], {}))    # KeyError del absent
add("rewrite_stale_defaults", rsd([row0], {"curator": {"stale_after_days": 30}}, results={}))  # KeyError after persist
add("rewrite_stale_defaults", rsd([row0], {"curator": {"stale_after_days": 30}}, io={"cfg": CFG, "config._persist_migration": {"raise": OSERR}}))
add("rewrite_stale_defaults", rsd([row0, row1], {"curator": {"stale_after_days": 30, "archive_after_days": 90}}, quiet=False))

# ── lower_is ──────────────────────────────────────────────────────────────
add("lower_is", {"word": "auto", "cur": " AUTO "})
add("lower_is", {"word": "auto", "cur": 5})
add("lower_is", {"word": "auto", "cur": None})
add("lower_is", {"word": "auto", "cur": "\x1cauto\x1f"})
add("lower_is", {"word": "all", "cur": "ALL"})
add("lower_is", {"word": "auto", "cur": "automatic"})

# ── installed_user_plugins ────────────────────────────────────────────────
def iup(io, disabled=None):
    d = {"io": io}
    if disabled is not None:
        d["disabled"] = disabled
    return d

base_io = {"cfg": CFG, "config.get_hermes_home": {"ret": "/h"},
           "path.is_dir": [{"ret": True}, {"ret": True}],
           "path.iterdir": {"ret": ["/h/plugins/p1"]},
           "path.exists": {"ret": True}, "open": {"ret": "f"},
           "config.fast_safe_load": {"ret": {"name": "plug"}}}
add("installed_user_plugins", iup(dict(base_io)))
io2 = dict(base_io); io2["config.fast_safe_load"] = {"ret": {"name": ""}}
add("installed_user_plugins", iup(io2))                                # name fallback
add("installed_user_plugins", iup(dict(base_io), disabled=["plug"]))   # disabled
io3 = dict(base_io); io3["path.exists"] = [{"ret": False}, {"ret": True}]
add("installed_user_plugins", iup(io3))                                # plugin.yml fallback
io4 = dict(base_io); io4["path.exists"] = {"ret": False}
add("installed_user_plugins", iup(io4))                                # no manifest
io5 = dict(base_io); io5["path.is_dir"] = [{"ret": False}]
add("installed_user_plugins", iup(io5))                                # plugins dir absent
io6 = dict(base_io); io6["path.is_dir"] = [{"ret": True}, {"ret": False}]
add("installed_user_plugins", iup(io6))                                # child not dir
io7 = dict(base_io); io7["open"] = {"raise": OSERR}
add("installed_user_plugins", iup(io7))                                # open OSError -> child.name
io8 = dict(base_io); io8["config.fast_safe_load"] = {"raise": VALERR}
add("installed_user_plugins", iup(io8))                                # outer catch -> []
io9 = dict(base_io); io9["config.fast_safe_load"] = {"raise": SE}
add("installed_user_plugins", iup(io9))                                # BaseException propagate
io10 = dict(base_io); io10["path.iterdir"] = {"ret": None}
add("installed_user_plugins", iup(io10))                               # TypeError -> []
io11 = dict(base_io); io11["config.get_hermes_home"] = {"ret": None}
add("installed_user_plugins", iup(io11))                               # TypeError / -> []
io12 = dict(base_io); io12["config.fast_safe_load"] = {"ret": [1]}
add("installed_user_plugins", iup(io12))                               # manifest non-dict -> []
io13 = dict(base_io); io13["config.fast_safe_load"] = {"ret": {"name": ["x"]}}
add("installed_user_plugins", iup(io13))                               # unhashable name -> []
io14 = dict(base_io); del io14["path.is_dir"]
add("installed_user_plugins", iup(io14))                               # fail closed

# ── migrate_to_41 ─────────────────────────────────────────────────────────
HEAD = "## Messaging other agents"
def m41(roster, io_extra=None, quiet=True, config_only=False, results=None):
    io = {"CONFIG_ONLY.get": {"ret": config_only}, "get_hermes_home": {"ret": "/h"},
          "_hermes_root": {"ret": "/h"}, "_roster": {"ret": roster}}
    if io_extra:
        io.update(io_extra)
    d = {"results": results if results is not None else {"config_added": []}, "quiet": quiet, "io": io}
    return d

soul_text = "intro\n" + HEAD + "\nbody\n## Next\nkeep\n"
add("migrate_to_41", m41([["p1", "/h/p1"]], {"path.is_file": {"ret": True}, "path.read_text": {"ret": soul_text}, "strip_legacy_protocol": {"ret": "intro\nkeep\n"}, "path.write_text": {"ret": 5}}, quiet=False))
add("migrate_to_41", m41([["p1", "/h/p1"]], {"path.is_file": {"ret": True}, "path.read_text": {"ret": "no heading\n"}}))
add("migrate_to_41", m41([["p1", "/h/p1"], ["p2", "/h/p2"]], {"path.is_file": {"ret": True}, "path.read_text": {"ret": soul_text}, "strip_legacy_protocol": {"ret": "clean\n"}, "path.write_text": {"ret": 5}}, quiet=False))
add("migrate_to_41", m41([["p1", "/h/p1"]], {"path.is_file": {"ret": True}, "path.read_text": {"raise": FNFE}}))  # OSError continue
add("migrate_to_41", m41([["p1", "/h/p1"]], {"path.is_file": {"ret": True}, "path.read_text": {"raise": VALERR}}))  # propagate
add("migrate_to_41", m41([["p1", "/h/p1"]], {"path.is_file": {"raise": SE}}))  # BaseException
add("migrate_to_41", m41([["p1", "/h/p1", "extra"]]))                   # unpack ValueError
add("migrate_to_41", m41([["p1", "/h/p1"]], {"path.is_file": {"ret": False}}))  # not file -> text ""
add("migrate_to_41", m41([["p1", "/h/p1"]], config_only=True))          # skip
add("migrate_to_41", m41([["p1", "/h/p1"]], {"_roster": {"import_raise": imp_fail("tools.bot_mode_probe")}}))  # import fail
add("migrate_to_41", m41([], {}))                                       # empty roster

# ── migrate_to_44 ─────────────────────────────────────────────────────────
add("migrate_to_44", R({"curator": {"stale_after_days": 30, "archive_after_days": 90}}))
add("migrate_to_44", R({"curator": {"stale_after_days": 30}}))
add("migrate_to_44", R({"curator": {"stale_after_days": 7, "archive_after_days": 90}}))
add("migrate_to_44", R({}))                                            # absent section, no due
add("migrate_to_44", R({"curator": {"stale_after_days": 30, "archive_after_days": 90}}, quiet=False))
add("migrate_to_44", {"results": {"config_added": []}, "quiet": True, "state": {"raw_config": {"curator": {"stale_after_days": 30}}}, "io": {"cfg": CFG, "config._persist_migration": {"raise": OSERR}}})

# ── migrate_to_45 ─────────────────────────────────────────────────────────
def m45(config, io_extra=None, quiet=True):
    io = {"cfg": CFG, "parse_config_string_list": {"ret": []},
          "_configurable_keys": {"ret": ["hermes-cli"]},
          "_get_plugin_toolset_keys": {"ret": []},
          "toolset_allowed_for_platform": {"ret": True}}
    if io_extra:
        io.update(io_extra)
    return {"results": {"config_added": []}, "quiet": quiet, "state": {"raw_config": config}, "io": io}

add("migrate_to_45", m45({"platform_toolsets": {"cli": ["hermes-cli"]}}, quiet=False))
add("migrate_to_45", m45({"platform_toolsets": {"cli": ["hermes-cli", "connections"]}}))  # already
add("migrate_to_45", m45({"platform_toolsets": {"cli": ["hermes-cli"]}, "agent": {"disabled_toolsets": "connections"}}, {"parse_config_string_list": {"ret": ["connections"]}}))
add("migrate_to_45", m45({"platform_toolsets": {"cli": ["hermes-cli"]}}, {"toolset_allowed_for_platform": {"ret": False}}))
add("migrate_to_45", m45({"platform_toolsets": {"cli": ["unknown"]}}))  # no explicit key
add("migrate_to_45", m45({"platform_toolsets": {"cli": ["hermes-cli"]}, "known_builtin_toolsets": {"cli": ["connections"]}}))  # offered decline
add("migrate_to_45", m45({"platform_toolsets": {"cli": ["hermes-cli"]}, "known_builtin_toolsets": {"cli": ["hermes-cli"]}}, quiet=False))  # offered updated
add("migrate_to_45", m45({"platform_toolsets": {"cli": ["hermes-cli"], "tui": ["hermes-cli"]}}, quiet=False))  # multi sorted
add("migrate_to_45", m45({"platform_toolsets": {"cli": ["hermes-cli"]}}, {"parse_config_string_list": {"ret": None}}))  # in None -> TypeError
# true fail-closed: parse_config_string_list script omitted entirely
_fc = m45({"platform_toolsets": {"cli": ["hermes-cli"]}})
del _fc["io"]["parse_config_string_list"]
add("migrate_to_45", _fc)
add("migrate_to_45", m45({"platform_toolsets": "x"}))                   # saved non-dict
add("migrate_to_45", m45({"platform_toolsets": {"cli": ["hermes-cli"]}}, {"_configurable_keys": {"raise": SE}}))

# ── migrate_to_46 ─────────────────────────────────────────────────────────
def m46(config, pb, quiet=True):
    return {"results": {"config_added": []}, "quiet": quiet, "state": {"raw_config": config},
            "io": {"cfg": CFG, "_parse_boolish": pb}}

add("migrate_to_46", m46({"mcp_servers": {"s1": {"disabled": True}}}, {"ret": True}, quiet=False))
add("migrate_to_46", m46({"mcp_servers": {"s1": {"disabled": False}}}, {"ret": False}))
add("migrate_to_46", m46({"mcp_servers": {"b": {"disabled": True}, "a": {"disabled": True}}}, {"ret": True}, quiet=False))  # sorted
add("migrate_to_46", m46({"mcp_servers": {"s1": {"disabled": True, "enabled": True}}}, {"ret": True}))  # enabled overwrite
add("migrate_to_46", m46({"mcp_servers": {"s1": "scalar", "s2": {"disabled": True}}}, {"ret": True}))  # non-dict skip
add("migrate_to_46", m46({"mcp_servers": "x"}, {"ret": True}))          # servers non-dict
add("migrate_to_46", m46({"mcp_servers": {"s1": {"disabled": True}}}, {"raise": SE}))
add("migrate_to_46", m46({"mcp_servers": {"s1": {"disabled": True}}}, {}))  # fail closed

# ── migrate_to_48 ─────────────────────────────────────────────────────────
L1 = "nikolaik/python-nodejs:python3.11-nodejs20"
L2 = "nikolaik/python-nodejs:python3.14-nodejs22"
add("migrate_to_48", R({"terminal": {"docker_image": L1}}))
add("migrate_to_48", R({"terminal": {"singularity_image": "docker://" + L2}}))
add("migrate_to_48", R({"terminal": {"docker_image": "user/pinned:1"}}))  # preserved
add("migrate_to_48", R({"terminal": {"docker_image": L1, "modal_image": L2, "daytona_image": L1}}, quiet=False))
add("migrate_to_48", R({}))                                            # absent terminal
add("migrate_to_48", R({"terminal": {"modal_image": L2, "singularity_image": "docker://" + L1}}))

# ── migrate_to_49 ─────────────────────────────────────────────────────────
def m49(config, env, quiet=True, config_only=False, io_extra=None):
    io = {"cfg": CFG, "CONFIG_ONLY.get": {"ret": config_only},
          "config.get_env_value_prefer_dotenv": env}
    if io_extra:
        io.update(io_extra)
    return {"results": {"config_added": []}, "quiet": quiet, "state": {"raw_config": config}, "io": io}

add("migrate_to_49", m49({"terminal": {"vercel_runtime": "node24"}}, {"ret": "node24"}, quiet=False))
add("migrate_to_49", m49({"terminal": {"vercel_runtime": "node24"}}, {"ret": "node24"}, config_only=True))
add("migrate_to_49", m49({"terminal": {"vercel_runtime": "node24"}}, {"ret": "node22"}))  # env mismatch
add("migrate_to_49", m49({"terminal": {"vercel_runtime": "node24"}}, {"ret": None}))
add("migrate_to_49", m49({"terminal": {"vercel_runtime": "python3.13"}}, {"ret": "node24"}))  # user runtime
add("migrate_to_49", m49({"terminal": {"vercel_runtime": "node24"}}, {"ret": 5}))  # AttributeError strip
add("migrate_to_49", m49({"terminal": {"vercel_runtime": "node24"}}, {"ret": " node24 "}, quiet=False))  # strip match
add("migrate_to_49", m49({"terminal": {"vercel_runtime": "node24"}}, {"ret": "node24"}, io_extra={"config.remove_env_value": {"raise": OSERR}}))

# ── migrate_to_50 ─────────────────────────────────────────────────────────
add("migrate_to_50", R({"security": {"tirith_enabled": True, "tirith_path": "/x", "other": 1}}))
add("migrate_to_50", R({"security": {"other": 1}}))                     # none present
add("migrate_to_50", R({"security": "x"}))                              # non-dict
add("migrate_to_50", R({"security": {"tirith_timeout": 5}}))
add("migrate_to_50", R({"security": {"tirith_fail_open": True}}, quiet=False))
add("migrate_to_50", {"results": {"config_added": []}, "quiet": True, "state": {"raw_config": {"security": {"tirith_enabled": True}}}, "io": {"cfg": CFG, "config._persist_migration": {"raise": SE}}})

# ── prompt_caching_child_lines ────────────────────────────────────────────
add("prompt_caching_child_lines", {"text": "prompt_caching:\n  cache_ttl: 5m\n  enabled: true\n"})
add("prompt_caching_child_lines", {"text": '"prompt_caching":\n  cache_ttl: 5m\n'})
add("prompt_caching_child_lines", {"text": "prompt_caching:\n  # comment\n  cache_ttl: 5m\n\n  enabled: true\n"})
add("prompt_caching_child_lines", {"text": "prompt_caching:\n  cache_ttl: 5m\nother:\n  x: 1\n"})
add("prompt_caching_child_lines", {"text": "prompt_caching:\n  cache_ttl: 5m\n    deeper: 1\n  enabled: true\n"})
add("prompt_caching_child_lines", {"text": "prompt_caching:\n  a: 1\nprompt_caching:\n  b: 2\n"})
add("prompt_caching_child_lines", {"text": "other:\n  cache_ttl: 5m\n"})
add("prompt_caching_child_lines", {"text": "prompt_caching:\r\n  cache_ttl: 5m\r\n  enabled: true\r\n"})
add("prompt_caching_child_lines", {"text": "prompt_caching:\v  cache_ttl: 5m\f  enabled: true\x1c"})
add("prompt_caching_child_lines", {"text": 5})                          # AttributeError
add("prompt_caching_child_lines", {"text": "prompt_caching:\n"})        # empty block

# ── migrate_to_54 ─────────────────────────────────────────────────────────
TTL_LINE = '  cache_ttl: "5m"  # use "1h" for long sessions with pauses between turns'
SEEDED = "prompt_caching:\n" + TTL_LINE + "\n"
PINNED = "prompt_caching:\n  cache_ttl: 5m\n"
def m54(config, io_extra=None, quiet=True):
    io = {"cfg": CFG, "config.get_config_path": {"ret": "/h/config.yaml"},
          "supports_file_tooling": {"ret": True}, "path.read_text": {"ret": SEEDED}}
    if io_extra:
        io.update(io_extra)
    return {"results": {"config_added": []}, "quiet": quiet, "state": {"raw_config": config}, "io": io}

add("migrate_to_54", m54({"prompt_caching": {"cache_ttl": "5m"}}, quiet=False))
add("migrate_to_54", m54({"prompt_caching": {"cache_ttl": "5m"}}, {"path.read_text": {"ret": PINNED}}, quiet=False))  # pinned info
add("migrate_to_54", m54({"prompt_caching": {"cache_ttl": "5m"}}, {"supports_file_tooling": {"ret": False}}, quiet=False))  # no read_text
add("migrate_to_54", m54({"prompt_caching": {"cache_ttl": "auto"}}))    # not 5m
add("migrate_to_54", m54({"prompt_caching": "x"}))                      # non-dict
add("migrate_to_54", m54({"prompt_caching": {"cache_ttl": "5m"}}, {"path.read_text": {"ret": PINNED}}))  # quiet pinned
add("migrate_to_54", m54({"prompt_caching": {"cache_ttl": "5m"}}, {"path.read_text": {"raise": OSERR}}))
add("migrate_to_54", m54({"prompt_caching": {"cache_ttl": "5m"}}, {"config.get_config_path": {"ret": None}}))  # AttributeError read_text
add("migrate_to_54", m54({"prompt_caching": {"cache_ttl": "5m"}}, {"path.read_text": {"ret": "\ufeff" + SEEDED}}))  # BOM

# ── disable_suspicious_mcp_servers ────────────────────────────────────────
def dms(config, val, quiet=True, io_extra=None, results=None):
    io = {"cfg": CFG, "validate_mcp_server_entry": val}
    if io_extra:
        io.update(io_extra)
    return {"results": results if results is not None else {"config_added": [], "warnings": []},
            "quiet": quiet, "state": {"raw_config": config}, "io": io}

add("disable_suspicious_mcp_servers", dms({"mcp_servers": {"s1": {"command": "x"}}}, {"ret": ["exfil risk"]}, quiet=False))
add("disable_suspicious_mcp_servers", dms({"mcp_servers": {"s1": {"command": "x"}}}, {"ret": []}))
add("disable_suspicious_mcp_servers", dms({"mcp_servers": {"s1": "scalar"}}, {"ret": ["x"]}))  # non-dict skip
add("disable_suspicious_mcp_servers", dms({"mcp_servers": {"s1": {}}}, {"import_raise": imp_fail("hermes_cli.mcp_security")}))  # import fail
add("disable_suspicious_mcp_servers", dms({"mcp_servers": {"s1": {}}}, {"import_raise": SE}))  # BaseException import
add("disable_suspicious_mcp_servers", dms({"mcp_servers": {"s1": {}}}, {"raise": SE}))  # call raise
add("disable_suspicious_mcp_servers", dms({"mcp_servers": {"s1": {}}}, {"ret": ["x"]}, results={"config_added": []}))  # KeyError warnings
add("disable_suspicious_mcp_servers", dms({"mcp_servers": {"s1": {}, "s2": {}}}, {"ret": ["a", "b"]}, quiet=False))  # multi issues
add("disable_suspicious_mcp_servers", dms({"mcp_servers": "x"}, {"ret": ["x"]}))  # non-dict servers

# ── write ─────────────────────────────────────────────────────────────────
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=True) + "\n")

from collections import Counter
cnt = Counter(c["func"] for c in CASES)
print(f"wrote {len(CASES)} cases to {OUT}")
for k in sorted(cnt):
    print(f"  {k}: {cnt[k]}")
low = [k for k, v in cnt.items() if v < 5]
print("FUNCS <5:", low if low else "none")