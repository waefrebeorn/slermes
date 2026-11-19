#!/usr/bin/env python3
"""Generate tests/fixtures/commands_flow.jsonl — {func,in} cases for
t_port_commands_flow.c / oracle_commands_flow.py. >=5 cases per ported
function (22 functions). List-returning seams are wrapped ([[...]]) so the
scripted RESULT is the list; per-call sequences are plain arrays of results.
Inputs are consumed identically by the C harness and the oracle (which drives
LIVE quarry code). Integral values are written as ints; str()-position inputs
avoid integral floats. Unicode is limited to the five Telegram dash codepoints
and codepoint-length probes (ASCII scope otherwise).

Run:  python tests/gen_commands_flow_cases.py
"""
import json
import os
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "commands_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def cmd(name, description="A command", category="Session", **kw):
    """A full CommandDef field dict (all 15 fields, dataclass defaults)."""
    d = {
        "name": name, "description": description, "category": category,
        "aliases": [], "args_hint": "", "subcommands": [],
        "cli_only": False, "gateway_only": False, "gateway_config_gate": None,
        "busy_policy": "reject", "busy_handler": None, "execute": None,
        "argument_mode": None, "desktop": None, "desktop_subcommands": None,
    }
    d.update(kw)
    return d


def raise_spec(kind, msg):
    return {"__raise__": kind, "msg": msg}


# ── localized ──
add("localized", key="slash.x", default="Fallback", seams={"t": "Bonjour"})
add("localized", key="slash.x", default="Fallback", seams={"t": "slash.x"})
add("localized", key="slash.x", default="Fallback", seams={"t": None})
add("localized", key="slash.x", default="Fallback", seams={"t": raise_spec("RuntimeError", "boom")})
add("localized", key="slash.x", default="Fallback", seams={"t": raise_spec("SystemExit", "bye")})
add("localized", key="slash.x", default="Fallback", seams={})
add("localized", key="slash.x", default="Fallback", seams={"t": 42})

# ── category_label ──
add("category_label", category="Session", seams={"t": "Sessions"})
add("category_label", category="Session", seams={"t": "slash.category.session"})
add("category_label", category="Unknown")
add("category_label", category="Tools & Skills", seams={"t": "Outils"})
add("category_label", category=None)
add("category_label", category="Info", seams={"t": raise_spec("ValueError", "bad")})
add("category_label", category="Exit", seams={})

# ── describe ──
add("describe", cmd=cmd("new", "Start a new session"), seams={"t": "Localized desc"})
add("describe", cmd=cmd("new", "Start a new session"), seams={"t": "slash.new.description"})
add("describe", cmd=cmd("new"), seams={"t": None})
add("describe", cmd=cmd("new"), seams={"t": raise_spec("RuntimeError", "x")})
add("describe", cmd=cmd("new"), seams={})
add("describe", cmd=cmd("new", ""), seams={"t": "slash.new.description"})

# ── desktop_surface_registry ──
add("desktop_surface_registry", registry=[cmd("clear", desktop="terminal", aliases=["cls"])])
add("desktop_surface_registry", registry=[])
add("desktop_surface_registry", registry=[cmd("model", desktop="")])
add("desktop_surface_registry", registry=[cmd("model", desktop=None)])
add("desktop_surface_registry", registry=[cmd("a", desktop="terminal"), cmd("b", desktop="hidden", aliases=["a"])])
add("desktop_surface_registry", registry=[cmd("fast", desktop="advanced")])

# ── build_commands_by_category ──
add("build_commands_by_category", registry=[cmd("new")],
    seams={"_build_description": "DESC"})
add("build_commands_by_category", registry=[cmd("new", aliases=["reset"])],
    seams={"_build_description": "DESC", "t": ["Local desc", "Alias for new"]})
add("build_commands_by_category", registry=[cmd("topic", gateway_only=True)], seams={})
add("build_commands_by_category", registry=[cmd("new"), cmd("title")],
    seams={"_build_description": ["D1", "D2"]})
add("build_commands_by_category", registry=[cmd("new")],
    seams={"_build_description": raise_spec("RuntimeError", "bd")})
add("build_commands_by_category", registry=[cmd("new", aliases=["reset"])],
    seams={"_build_description": "DESC", "t": ["Local desc", raise_spec("ValueError", "al")]})
add("build_commands_by_category", registry=[cmd("new")], seams={})
add("build_commands_by_category", registry=[cmd("new", aliases=["reset"])],
    seams={"_build_description": "DESC", "t": [None, "Alias for new"]})

# ── build_commands ──
add("build_commands", registry=[cmd("new"), cmd("model", category="Configuration")],
    seams={"_build_description": ["D1", "D2"]})
add("build_commands", registry=[], seams={})
add("build_commands", registry=[cmd("topic", gateway_only=True)], seams={})
add("build_commands", registry=[cmd("new")],
    seams={"_build_description": raise_spec("RuntimeError", "bd")})
add("build_commands", registry=[cmd("new")], seams={})
add("build_commands", registry=[cmd("a", category="Session"), cmd("b", category="Info", aliases=["a"])],
    seams={"_build_description": ["D1", "D2"], "t": ["Local b", "Alias a"]})

# ── is_gateway_available ──
add("is_gateway_available", cmd=cmd("status"), seams={})
add("is_gateway_available", cmd=cmd("config", cli_only=True), seams={})
add("is_gateway_available", cmd=cmd("skills", cli_only=True, gateway_config_gate="skills.write_approval"),
    overrides=["skills"], seams={})
add("is_gateway_available", cmd=cmd("skills", cli_only=True, gateway_config_gate="skills.write_approval"),
    overrides=["other"], seams={})
add("is_gateway_available", cmd=cmd("skills", cli_only=True, gateway_config_gate="skills.write_approval"),
    overrides=None, seams={"_resolve_config_gates": [["skills"]]})
add("is_gateway_available", cmd=cmd("skills", cli_only=True, gateway_config_gate="g"),
    overrides=None, seams={"_resolve_config_gates": raise_spec("RuntimeError", "cfg")})
add("is_gateway_available", cmd=cmd("skills", cli_only=True, gateway_config_gate="g"),
    overrides=None, seams={"_resolve_config_gates": raise_spec("SystemExit", "stop")})
add("is_gateway_available", cmd=cmd("skills", cli_only=True, gateway_config_gate="g"),
    overrides=None, seams={})
add("is_gateway_available", cmd=cmd("skills", cli_only=True, gateway_config_gate="g"),
    overrides=[], seams={})

# ── module_getattr ──
add("module_getattr", name="COMMANDS", seams={"build_commands": {"/new": "D"}})
add("module_getattr", name="COMMANDS_BY_CATEGORY", seams={"build_commands_by_category": {"Session": {"/new": "D"}}})
add("module_getattr", name="BOGUS", seams={})
add("module_getattr", name="", seams={})
add("module_getattr", name="COMMANDS", seams={"build_commands": raise_spec("RuntimeError", "bc")})
add("module_getattr", name="COMMANDS", seams={})
add("module_getattr", name="COMMANDS", seams={"build_commands": None})

# ── gateway_available_commands ──
add("gateway_available_commands",
    registry=[cmd("status"), cmd("config", cli_only=True),
              cmd("skills", cli_only=True, gateway_config_gate="skills.write_approval")],
    seams={"_resolve_config_gates": [["skills"]]})
add("gateway_available_commands", registry=[], seams={"_resolve_config_gates": [[]]})
add("gateway_available_commands", registry=[cmd("status")],
    seams={"_resolve_config_gates": raise_spec("RuntimeError", "cfg")})
add("gateway_available_commands", registry=[cmd("status")], seams={})
add("gateway_available_commands", registry=[cmd("status")],
    seams={"_resolve_config_gates": raise_spec("KeyboardInterrupt", "ki")})
add("gateway_available_commands", registry=[cmd("status"), cmd("config", cli_only=True)],
    seams={"_resolve_config_gates": [[]]})

# ── normalize_telegram_desc ──
add("normalize_telegram_desc", desc="plain ascii")
add("normalize_telegram_desc", desc="a\u2013b")
add("normalize_telegram_desc", desc="\u2012\u2013\u2014\u2015\u2212")
add("normalize_telegram_desc", desc="")
add("normalize_telegram_desc", desc="x\u2014y\u2212z")
add("normalize_telegram_desc", desc="caf\u00e9 \u2615")

# ── truncate_desc ──
add("truncate_desc", desc="short", limit=10)
add("truncate_desc", desc="abcdefgh", limit=8)
add("truncate_desc", desc="abcdefghij", limit=8)
add("truncate_desc", desc="abcdef", limit=3)
add("truncate_desc", desc="abcdef", limit=2)
add("truncate_desc", desc="abcdef", limit=0)
add("truncate_desc", desc="\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9", limit=5)
add("truncate_desc", desc="abc", limit=3)

# ── sanitized_rank ──
add("sanitized_rank", raw_names=["Help", "new", "help"],
    seams={"_sanitize_telegram_name": ["help", "new", "help"]})
add("sanitized_rank", raw_names=["a", "b"], seams={"_sanitize_telegram_name": ["", "b"]})
add("sanitized_rank", raw_names=["a", "a"], seams={"_sanitize_telegram_name": ["x", "x"]})
add("sanitized_rank", raw_names=[5, 6], seams={"_sanitize_telegram_name": ["five", "six"]})
add("sanitized_rank", raw_names=["a"], seams={"_sanitize_telegram_name": raise_spec("RuntimeError", "s")})
add("sanitized_rank", raw_names=["a"], seams={})
add("sanitized_rank", raw_names=None, seams={})
add("sanitized_rank", raw_names=["a"], seams={"_sanitize_telegram_name": None})

# ── iter_gateway_skills ──
SK_INFO = {"skill_md_path": "/skills/creative/ascii-art/SKILL.md", "name": "ascii-art", "description": "d"}
BASE_SEAMS = {
    "_skills_dir": "/skills",
    "get_disabled_skill_names": [[]],
    "get_external_skills_dirs": [[]],
    "get_project_skills_dirs": [[]],
}


def ig_seams(resolve_queue, skill_cmds, **over):
    s = dict(BASE_SEAMS)
    s["path_resolve"] = resolve_queue
    s["get_skill_commands"] = skill_cmds
    s.update(over)
    return s


add("iter_gateway_skills", platform="telegram",
    seams=ig_seams(["/skills/.hub", "/skills", "/skills/creative/ascii-art/SKILL.md"],
                   {"/ascii-art": SK_INFO}))
add("iter_gateway_skills", platform="telegram",
    seams=ig_seams(["/skills/.hub", "/skills", "/skills/.hub/x/SKILL.md"],
                   {"/x": {"skill_md_path": "/skills/.hub/x/SKILL.md", "name": "x"}}))
add("iter_gateway_skills", platform="telegram",
    seams=ig_seams(["/skills/.hub", "/skills", "/skills/creative/ascii-art/SKILL.md"],
                   {"/ascii-art": SK_INFO}, get_disabled_skill_names=[["ascii-art"]]))
add("iter_gateway_skills", platform="telegram",
    seams=ig_seams(["/skills/.hub", "/skills", "/elsewhere/x/SKILL.md"],
                   {"/x": {"skill_md_path": "/elsewhere/x/SKILL.md", "name": "x"}}))
add("iter_gateway_skills", platform="telegram",
    seams=ig_seams(["/skills/.hub", "/skills"], {"/x": {"skill_md_path": "", "name": "x"}}))
add("iter_gateway_skills", platform="telegram",
    seams=ig_seams(["/skills/.hub", "/skills", "/skills/creative/ascii-art/SKILL.md"],
                   {"/ascii-art": SK_INFO},
                   get_external_skills_dirs=raise_spec("RuntimeError", "ext")))
add("iter_gateway_skills", platform="telegram",
    seams=ig_seams(["/skills/.hub", "/skills", "/ext", raise_spec("ValueError", "bad"),
                    "/skills/creative/ascii-art/SKILL.md"],
                   {"/ascii-art": SK_INFO}, get_external_skills_dirs=[["/ext", "/bad"]]))
add("iter_gateway_skills", platform="telegram",
    seams=ig_seams([], {"/ascii-art": SK_INFO},
                   get_disabled_skill_names=raise_spec("SystemExit", "stop")))
add("iter_gateway_skills", platform="telegram", import_gates={"agent.skill_utils": True}, seams={})
add("iter_gateway_skills", platform="telegram",
    seams=ig_seams(["/skills/.hub", "/skills", "/skills"], {"/x": {"skill_md_path": "/skills", "name": "x"}}))
add("iter_gateway_skills", platform="telegram", seams={})
add("iter_gateway_skills", platform="telegram",
    seams=ig_seams(["/skills/.hub", "/skills", "/skills/creative/ascii-art/SKILL.md"],
                   {"/ascii-art": SK_INFO}, get_disabled_skill_names=None))
add("iter_gateway_skills", platform="telegram",
    seams=ig_seams(["/skills/.hub", "/skills", "/skills/top/SKILL.md"],
                   {"/top": {"skill_md_path": "/skills/top/SKILL.md", "name": "top"}}))
add("iter_gateway_skills", platform="telegram",
    seams=ig_seams(["/skills/.hub", "/skills", "/skills/a/SKILL.md", "/skills/z/SKILL.md"],
                   {"/zeta": {"skill_md_path": "/skills/z/SKILL.md", "name": "zeta"},
                    "/alpha": {"skill_md_path": "/skills/a/SKILL.md", "name": "alpha"}}))

# ── gateway_bundles ──
add("gateway_bundles", seams={"get_skill_bundles": {"b": {"skills": ["s1"]}, "a": {"skills": []}}})
add("gateway_bundles", seams={"get_skill_bundles": {}})
add("gateway_bundles", seams={"get_skill_bundles": raise_spec("RuntimeError", "scan"),
                              "logger.warning": None})
add("gateway_bundles", seams={"get_skill_bundles": None, "logger.warning": None})
add("gateway_bundles", seams={"get_skill_bundles": raise_spec("SystemExit", "stop")})
add("gateway_bundles", seams={"get_skill_bundles": raise_spec("ValueError", "scan"),
                              "logger.warning": raise_spec("RuntimeError", "log")})
add("gateway_bundles", seams={})

# ── completion ──
add("completion", text="/new", word="ne", display="/new", meta="desc")
add("completion", text="/new", word="", display="/new", meta=None)
add("completion", text="/x", word="\u00e9\u00e9", display="/x", meta="m")
add("completion", text="t", word="w", display="d", meta=None)
add("completion", text="t", word="w", display="d", meta={"x": 1})
add("completion", text="", word="abc", display="", meta="")

# ── prefix_completions ──
add("prefix_completions", rows=[["new", "m1"], ["newest", "m2"]], partial="new")
add("prefix_completions", rows=[["new", "m1"], ["newest", "m2"]], partial="new", skip_exact=False)
add("prefix_completions", rows=[["new", "m1"], ["newest", "m2"]], partial="NEW")
add("prefix_completions", rows=[], partial="x")
add("prefix_completions", rows=None, partial="x")
add("prefix_completions", rows=[[1]], partial="")
add("prefix_completions", rows=[["a", "b", "c"]], partial="")
add("prefix_completions", rows=[[5, "m"]], partial="")
add("prefix_completions", rows=[["alpha", None], ["beta", "m"]], partial="")

# ── split_args ──
add("split_args", sub_text="list foo")
add("split_args", sub_text="list ")
add("split_args", sub_text="")
add("split_args", sub_text="foo")
add("split_args", sub_text="a b c")
add("split_args", sub_text="  ")
add("split_args", sub_text="a\tb")

# ── dir_completions ──
add("dir_completions", expanded="/home/user/", word="user/", limit=30, want_dir=None,
    seams={"os_listdir": [["b_dir", "a_file"]], "os_isdir": [False, True],
           "text_for": ["./a_file", "./b_dir"], "_file_size_label": ["12B"]})
add("dir_completions", expanded="/home/user/proj", word="proj", limit=30, want_dir=None,
    seams={"os_listdir": [["proj1", "proj2", "other"]], "os_isdir": [True, False],
           "text_for": ["/home/user/proj1", "/home/user/proj2"], "_file_size_label": ["3K"]})
add("dir_completions", expanded="/d/", word="d/", limit=30, want_dir=True,
    seams={"os_listdir": [["f", "g"]], "os_isdir": [False, True], "text_for": ["/d/g"]})
add("dir_completions", expanded="/d/", word="d/", limit=0, want_dir=None,
    seams={"os_listdir": [["a"]], "os_isdir": [False]})
add("dir_completions", expanded="/d/", word="d/", limit=30, want_dir=None,
    seams={"os_listdir": raise_spec("FileNotFoundError", "no dir")})
add("dir_completions", expanded="/d/", word="d/", limit=30, want_dir=None,
    seams={"os_listdir": raise_spec("RuntimeError", "boom")})
add("dir_completions", expanded="/d/", word="d/", limit=30, want_dir=None,
    seams={"os_listdir": raise_spec("SystemExit", "stop")})
add("dir_completions", expanded="/d/", word="d/", limit=30, want_dir=None,
    seams={"os_listdir": [["a"]], "os_isdir": [True], "text_for": raise_spec("ValueError", "tf")})
add("dir_completions", expanded="/d/", word="d/", limit=30, want_dir=None,
    seams={"os_listdir": [["a"]], "os_isdir": [False], "text_for": ["./a"],
           "_file_size_label": raise_spec("OSError", "fs")})
add("dir_completions", expanded="/d/", word="d/", limit=30, want_dir=None, seams={})
add("dir_completions", expanded="/d/", word="d/", limit=30, want_dir=None,
    seams={"os_listdir": None})
add("dir_completions", expanded="relative", word="rel", limit=30, want_dir=None,
    seams={"os_listdir": [["rel1"]], "os_isdir": [False], "text_for": ["rel1"],
           "_file_size_label": ["0B"]})

# ── call_provider ──
add("call_provider", provider_present=True, seams={"provider": {"a": {"description": "d"}}})
add("call_provider", provider_present=True, seams={"provider": {}})
add("call_provider", provider_present=True, seams={"provider": None})
add("call_provider", provider_present=False, seams={})
add("call_provider", provider_present=True, seams={"provider": raise_spec("ValueError", "p")})
add("call_provider", provider_present=True, seams={"provider": raise_spec("SystemExit", "stop")})
add("call_provider", provider_present=True, seams={})
add("call_provider", provider_present=True, seams={"provider": [[1]]})

# ── exec_autoprune ──
add("exec_autoprune", ctx={"args": "on"}, seams={"run_command": "Auto-prune enabled"})
add("exec_autoprune", ctx={"args": ""}, seams={"run_command": "state"})
add("exec_autoprune", ctx={"args": "x"}, seams={"run_command": raise_spec("RuntimeError", "rc")})
add("exec_autoprune", ctx={"args": "x"}, import_gates={"hermes_cli.prune_advisor": True}, seams={})
add("exec_autoprune", ctx={"args": "x"}, seams={})
add("exec_autoprune", ctx={"args": "x"}, seams={"run_command": None})

# ── skill_commands ──
add("skill_commands", seams={"get_skill_commands": {"/a": {"description": "d"}}})
add("skill_commands", seams={"get_skill_commands": None})
add("skill_commands", seams={"get_skill_commands": {}})
add("skill_commands", seams={"get_skill_commands": raise_spec("ValueError", "sc")})
add("skill_commands", seams={"get_skill_commands": raise_spec("SystemExit", "stop")})
add("skill_commands", import_gates={"agent.skill_commands": True}, seams={})
add("skill_commands", seams={})

# ── unknown_command_lines ──
add("unknown_command_lines", typed="/mdoel", known=["/model", "/new"],
    seams={"get_close_matches": [["/model"]],
           "t": ["Did you mean /model?", "Nothing was sent. Did you mean /model?", "Type /help"]})
add("unknown_command_lines", typed="/zzz", known=["/model"],
    seams={"get_close_matches": [[]], "t": ["Nothing was sent.", "Type /help"]})
add("unknown_command_lines", typed="/mdoel extra args", known=["/model"],
    seams={"get_close_matches": [["/model"]],
           "t": ["DYM", "Nothing sent. DYM", "Help"]})
add("unknown_command_lines", typed="", known=[],
    seams={"get_close_matches": [[]], "t": ["Nothing was sent.", "Type /help"]})
add("unknown_command_lines", typed="/mdoel", known=["/model"],
    seams={"get_close_matches": [["/model"]], "t": [None, "lead", "help"]})
add("unknown_command_lines", typed="/x", known=["/y"],
    seams={"get_close_matches": raise_spec("RuntimeError", "cm")})
add("unknown_command_lines", typed="/x", known=["/y"], seams={})
add("unknown_command_lines", typed="/x", known=None, seams={})
add("unknown_command_lines", typed="/x", known=["/y"],
    seams={"get_close_matches": [[]], "t": [raise_spec("KeyboardInterrupt", "ki"), "help"]})


def main():
    counts = Counter(c["func"] for c in CASES)
    short = {f: n for f, n in counts.items() if n < 5}
    if short:
        raise SystemExit("FATAL: <5 cases for %s" % short)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    for fn, n in sorted(counts.items()):
        print("  %-28s %d" % (fn, n))


if __name__ == "__main__":
    main()
