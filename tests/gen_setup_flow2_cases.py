#!/usr/bin/env python3
"""Generate tests/fixtures/setup_flow2.jsonl — differential cases for the
unclaimed hermes_cli/setup.py FLOW ports (src/cli/setup_flow2.c).

Every case is {"func", "in"}; "in.seams" scripts the universal seam (single
value reused, array consumed per call with the last element repeating,
{"__raise__": kind, "msg": ...} scripts an exception, MISSING script is
fail-closed RuntimeError on both sides). Output seams (print / print_info /
print_success / print_warning / print_error / color) default to null.

Domain (see the port header): ASCII inputs; paths are POSIX strings;
backup_config returns null or {"path": str}; load_config returns objects;
prompt results are strings; int/float parse inputs stay int64/finite; no
integral floats in str()-computed positions; scope-exit scripts are null or a
raise (never a suppressing truthy value).

Run:  python tests/gen_setup_flow2_cases.py
"""
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "setup_flow2.jsonl")

OUTPUT_SEAMS = {
    "print": None, "print_info": None, "print_success": None,
    "print_warning": None, "print_error": None, "color": None,
}
ENV = {
    "DEFAULT_CONFIG": {"model": {"provider": "nous"}, "agent": {"max_turns": 90}},
    "PROJECT_ROOT": "/opt/hermes",
}
CP = "/home/u/.hermes/config.yaml"
EP = "/home/u/.hermes/.env"
HOME = "/home/u/.hermes"
BP = {"path": CP + ".pre-setup"}


def case(func, seams=None, env=None, **inp):
    s = dict(OUTPUT_SEAMS)
    s.update(seams or {})
    d = dict(inp)
    d["seams"] = s
    d["env"] = env if env is not None else dict(ENV)
    return {"func": func, "in": d}


def bare(func, seams=None, **inp):
    """Case WITHOUT the default output scripts (fail-closed probes)."""
    d = dict(inp)
    d["seams"] = dict(seams or {})
    d["env"] = dict(ENV)
    return {"func": func, "in": d}


def impl_seams(**over):
    s = {
        "is_managed": False,
        "ensure_hermes_home": None,
        "get_config_path": CP,
        "backup_config": BP,
        "load_config": [{}, {"model": {"provider": "nous"}}],
        "get_hermes_home": HOME,
        "is_interactive_stdin": True,
        "get_env_value": [None, None],
        "get_active_provider": None,
        "offer_agent_import": "",
        "prompt_choice": 1,
        "print_header": None,
        "get_env_path": EP,
        "_apply_default_agent_settings": None,
        "run_setup_steps": None,
        "save_config": None,
        "backup_path.exists": True,
        "_print_setup_summary": None,
        "offer_consent_if_undecided": None,
        "record_setup_completed": None,
        "print_noninteractive_setup_guidance": None,
        "run_portal_one_shot": None,
        "managed_error": None,
    }
    s.update(over)
    return s


def wiz_seams(**over):
    s = impl_seams()
    s["setup_navigation_scope.enter"] = None
    s["setup_navigation_scope.exit"] = None
    s.update(over)
    return s


CASES = []

# ── _info ──
CASES += [
    case("info", lines=["a", "b"]),
    case("info", lines=[None]),
    case("info", lines=["a", None, "b"]),
    case("info", lines=[]),
    case("info", lines=[""]),
    bare("info", lines=["x"]),                                   # print_info fail-closed
    case("info", {"print_info": {"__raise__": "KeyboardInterrupt", "msg": ""}}, lines=["x"]),
    case("info", {"print": {"__raise__": "ValueError", "msg": "boom"}}, lines=[None]),
]

# ── _sub_dict ──
CASES += [
    case("sub_dict", parent={}, key="a"),
    case("sub_dict", parent={"a": {"x": 1}}, key="a"),
    case("sub_dict", parent={"a": None}, key="a"),
    case("sub_dict", parent={"a": [1, 2]}, key="a"),
    case("sub_dict", parent={"a": "s"}, key="a"),
    case("sub_dict", parent={"a": 5, "b": 2}, key="a"),
    case("sub_dict", parent={"a": False}, key="a"),
    case("sub_dict", parent={"a": {}}, key="a"),
]

# ── _section_rule ──
CASES += [
    case("section_rule", title="API Keys"),
    case("section_rule", title=""),
    case("section_rule", title="Keys & Tokens"),
    case("section_rule", {"color": {"__raise__": "ValueError", "msg": "c"}}, title="T"),
    bare("section_rule", title="T"),                             # print fail-closed
    case("section_rule", {"color": "COLORED"}, title="T"),
]

# ── _print_banner ──
CASES += [
    case("print_banner", lines=["a", "b"]),
    case("print_banner", lines=[]),
    case("print_banner", lines=[""]),
    case("print_banner", lines=["x", "y", "z"]),
    case("print_banner", {"color": {"__raise__": "SystemExit", "msg": "1"}}, lines=["a"]),
    bare("print_banner", lines=["a"]),                           # print fail-closed
]

# ── _module_installed ──
CASES += [
    case("module_installed", {"find_spec": {"origin": "x"}}, name="os"),
    case("module_installed", {"find_spec": None}, name="nope"),
    case("module_installed", {"find_spec": ""}, name="edge"),    # "" is not None -> True
    case("module_installed", {"find_spec": 0}, name="edge2"),
    case("module_installed", {"find_spec": {"__raise__": "ValueError", "msg": "bad"}}, name="x"),
    case("module_installed", {"find_spec": {"__raise__": "RuntimeError", "msg": "r"}}, name="x"),
    case("module_installed", {"find_spec": {"__raise__": "KeyboardInterrupt", "msg": ""}}, name="x"),
    case("module_installed", {"find_spec": {"__raise__": "SystemExit", "msg": "1"}}, name="x"),
    bare("module_installed", name="x"),                          # fail-closed -> False
]

# ── _prompt_number ──
PN = [
    ("int", 90, "150"), ("int", 90, "abc"), ("int", 90, "  42  "),
    ("int", 90, "+7"), ("int", 90, "1_0"), ("int", 90, "_1"),
    ("int", 90, "1_"), ("int", 90, "0x10"), ("int", 90, "007"),
    ("int", 90, "-12"), ("int", None, "5"),
    ("float", 0.5, "0.75"), ("float", 0.5, "1e3"), ("float", 0.5, ".5"),
    ("float", 0.5, "1._5"), ("float", 0.95, "  -0.25 "),
]
for cast, cur, answer in PN:
    CASES.append(case("prompt_number", {"prompt": answer}, label="L", current=cur, cast=cast))
CASES += [
    case("prompt_number", {"prompt": {"__raise__": "ValueError", "msg": "v"}},
         label="L", current=90, cast="int"),
    case("prompt_number", {"prompt": {"__raise__": "KeyboardInterrupt", "msg": ""}},
         label="L", current=90, cast="int"),
    bare("prompt_number", label="L", current=90, cast="int"),    # prompt fail-closed
]
# seeded parse fuzz (in-domain only: int64 / finite results)
rng = random.Random(20261118)
alpha = "0123456789._+-eE \t"
fuzz = 0
while fuzz < 12:
    s = "".join(rng.choice(alpha) for _ in range(rng.randint(0, 8)))
    cast = "int" if fuzz % 2 == 0 else "float"
    try:
        if cast == "int":
            v = int(s)
            if abs(v) > 2 ** 53:
                continue
        else:
            v = float(s)
            if not math.isfinite(v):
                continue
    except ValueError:
        pass
    CASES.append(case("prompt_number", {"prompt": s}, label="L", current=90, cast=cast))
    fuzz += 1

# ── _prompt_int_setting ──
CASES += [
    case("prompt_int_setting", {"prompt": "150", "accept": True},
         section={}, key="max_turns", label="Max iterations", current=90),
    case("prompt_int_setting", {"prompt": "150", "accept": False},
         section={}, key="max_turns", label="L", current=90),
    case("prompt_int_setting", {"prompt": "abc"},
         section={}, key="max_turns", label="L", current=90),
    case("prompt_int_setting", {"prompt": "0", "accept": True},
         section={}, key="k", label="L", current=90),
    case("prompt_int_setting", {"prompt": "150", "accept": {"__raise__": "RuntimeError", "msg": "a"}},
         section={}, key="k", label="L", current=90),
    case("prompt_int_setting", {"prompt": {"__raise__": "SystemExit", "msg": "1"}},
         section={}, key="k", label="L", current=90),
    case("prompt_int_setting", {"prompt": "150", "accept": True},
         section={"max_turns": 5}, key="max_turns", label="L", current=5),
    case("prompt_int_setting", {"prompt": "150", "accept": None},
         section={}, key="k", label="L", current=90),
]

# ── _prompt_and_save_env_var ──
VAR = {"name": "OPENAI_API_KEY", "prompt": "Enter key", "password": True}
CASES += [
    case("prompt_and_save_env_var", {"prompt": "sk-123", "save_env_value": None},
         var=VAR, saved_msg="  Saved", skipped_msg="  Skipped"),
    case("prompt_and_save_env_var", {"prompt": ""},
         var=VAR, saved_msg="S", skipped_msg="K"),
    case("prompt_and_save_env_var", {"prompt": None},
         var=VAR, saved_msg="S", skipped_msg="K"),
    case("prompt_and_save_env_var", {"prompt": "v", "save_env_value": None},
         var={"name": "X"}, saved_msg="S", skipped_msg="K"),
    case("prompt_and_save_env_var", {},
         var={"prompt": "p"}, saved_msg="S", skipped_msg="K"),   # KeyError 'name'
    case("prompt_and_save_env_var", {"prompt": "v", "save_env_value": None},
         var={"name": "X", "password": False}, saved_msg="S", skipped_msg="K"),
    case("prompt_and_save_env_var", {"prompt": {"__raise__": "SystemExit", "msg": "1"}},
         var=VAR, saved_msg="S", skipped_msg="K"),
    case("prompt_and_save_env_var",
         {"prompt": "v", "save_env_value": {"__raise__": "ValueError", "msg": "disk"}},
         var=VAR, saved_msg="S", skipped_msg="K"),
    case("prompt_and_save_env_var", {"prompt": "v", "save_env_value": None},
         var={"name": "X", "prompt": None}, saved_msg="S", skipped_msg="K"),
]

# ── _record_setup_completed ──
CASES += [
    case("record_setup_completed",
         {"offer_consent_if_undecided": None, "record_setup_completed": None},
         config={"model": {"provider": "nous"}}),
    case("record_setup_completed",
         {"offer_consent_if_undecided": None, "record_setup_completed": None},
         config={"model": None}),
    case("record_setup_completed",
         {"offer_consent_if_undecided": None, "record_setup_completed": None},
         config={"model": "str"}),
    case("record_setup_completed",
         {"offer_consent_if_undecided": None, "record_setup_completed": None},
         config={"model": {}}),
    case("record_setup_completed",
         {"offer_consent_if_undecided": None, "record_setup_completed": None},
         config={"model": {"provider": None}}),
    case("record_setup_completed",
         {"offer_consent_if_undecided": None, "record_setup_completed": None},
         config={}),
    case("record_setup_completed",
         {"offer_consent_if_undecided": {"__raise__": "RuntimeError", "msg": "o"}},
         config={"model": {"provider": "x"}}),
    case("record_setup_completed", {"offer_consent_if_undecided": None},
         config={"model": {"provider": "x"}}),                   # record fail-closed
    case("record_setup_completed",
         {"offer_consent_if_undecided": None, "record_setup_completed": None},
         config={"model": [1]}),
]

# ── run_setup_wizard ──
CASES += [
    case("run_setup_wizard", wiz_seams(), args={}),
    case("run_setup_wizard",
         wiz_seams(**{"run_setup_steps": {"__raise__": "_SetupCancelled", "msg": ""}}),
         args={}),
    case("run_setup_wizard",
         wiz_seams(**{"run_setup_steps": {"__raise__": "_SetupGoBack", "msg": "2"}}),
         args={}),
    case("run_setup_wizard",
         wiz_seams(**{"setup_navigation_scope.enter": {"__raise__": "RuntimeError", "msg": "e"}}),
         args={}),
    case("run_setup_wizard",
         wiz_seams(**{"setup_navigation_scope.exit": {"__raise__": "ValueError", "msg": "x"}}),
         args={}),
    case("run_setup_wizard", wiz_seams(), args={"non_interactive": True}),
    case("run_setup_wizard",
         wiz_seams(is_managed={"__raise__": "SystemExit", "msg": "1"}), args={}),
]

# ── _run_setup_section ──
SEC_SEAMS = {"run_setup_steps": None, "save_config": None}
for sec in ("model", "agent", "tools", "telemetry", "gateway", "bogus", ""):
    CASES.append(case("run_setup_section", SEC_SEAMS, config={"model": {}}, section=sec))
CASES += [
    case("run_setup_section",
         {"run_setup_steps": {"__raise__": "_SetupGoBack", "msg": "0"},
          "save_config": None},
         config={}, section="model"),
    case("run_setup_section", {"run_setup_steps": None}, config={}, section="model"),
]

# ── _run_full_setup ──
FS_SEAMS = {"print_header": None, "get_config_path": CP, "get_env_path": EP,
            "_apply_default_agent_settings": None, "run_setup_steps": None}
CASES += [
    case("run_full_setup", FS_SEAMS, config={"x": 1}, hermes_home=HOME,
         is_existing=True, imported_from=""),
    case("run_full_setup", FS_SEAMS, config={"x": 1}, hermes_home=HOME,
         is_existing=False, imported_from=""),
    case("run_full_setup", FS_SEAMS, config={}, hermes_home=HOME,
         is_existing=False, imported_from="openclaw"),
    case("run_full_setup", FS_SEAMS, config={}, hermes_home=HOME,
         is_existing=True, imported_from="claw"),
    case("run_full_setup",
         dict(FS_SEAMS, get_config_path={"__raise__": "ValueError", "msg": "p"}),
         config={}, hermes_home=HOME, is_existing=True, imported_from=""),
    case("run_full_setup",
         dict(FS_SEAMS, _apply_default_agent_settings={"__raise__": "SystemExit", "msg": "1"}),
         config={}, hermes_home=HOME, is_existing=False, imported_from=""),
    case("run_full_setup",
         dict(FS_SEAMS, run_setup_steps={"__raise__": "_SetupGoBack", "msg": "1"}),
         config={}, hermes_home=HOME, is_existing=True, imported_from=""),
    bare("run_full_setup", config={}, hermes_home=HOME, is_existing=True, imported_from=""),
]

# ── _run_setup_wizard_impl ──
CASES += [
    case("run_setup_wizard_impl", impl_seams(is_managed=True), args={}),
    case("run_setup_wizard_impl",
         impl_seams(is_managed=True, managed_error={"__raise__": "RuntimeError", "msg": "m"}),
         args={}),
    case("run_setup_wizard_impl", impl_seams(), args={"non_interactive": True}),
    case("run_setup_wizard_impl", impl_seams(is_interactive_stdin=False), args={}),
    case("run_setup_wizard_impl", impl_seams(), args={"portal": True}),
    case("run_setup_wizard_impl", impl_seams(), args={"section": "agent"}),
    case("run_setup_wizard_impl", impl_seams(get_env_value=["k"]), args={"quick": True}),
    case("run_setup_wizard_impl", impl_seams(get_env_value=["k"]), args={}),
    case("run_setup_wizard_impl", impl_seams(), args={}),
    case("run_setup_wizard_impl", impl_seams(prompt_choice=0), args={}),
    case("run_setup_wizard_impl", impl_seams(prompt_choice=2), args={}),
    case("run_setup_wizard_impl", impl_seams(prompt_choice=3), args={}),
    case("run_setup_wizard_impl", impl_seams(prompt_choice=-1), args={}),
    case("run_setup_wizard_impl", impl_seams(), args={"reset": True}),
    case("run_setup_wizard_impl", impl_seams(backup_config=None), args={}),
    case("run_setup_wizard_impl", impl_seams(**{"backup_path.exists": False}), args={}),
    bare("run_setup_wizard_impl", args={}),                      # is_managed fail-closed
    case("run_setup_wizard_impl",
         {k: v for k, v in impl_seams(get_env_value=["k"]).items()
          if k != "get_active_provider"},
         args={}),                                               # short-circuit probe
    case("run_setup_wizard_impl",
         impl_seams(get_active_provider={"id": "nous"}), args={}),
    case("run_setup_wizard_impl", impl_seams(), args={"reconfigure": True}),
    case("run_setup_wizard_impl",
         impl_seams(load_config=[{}, {"__raise__": "ValueError", "msg": "disk"}]), args={}),
    case("run_setup_wizard_impl", impl_seams(offer_agent_import="openclaw"), args={}),
]

# ── _SetupNavigationState.__init__ ──
CASES += [
    case("nav_state_init"),
    case("nav_state_init", section_index=2, prompt_index=5),
    case("nav_state_init", section_index=0, prompt_index=3),
    case("nav_state_init", section_index=-1, prompt_index=0),
    case("nav_state_init", section_index=7, prompt_index=1),
]

# ── _SetupNavigationState.reset ──
CASES += [
    case("nav_state_reset", section_index=1, replay=None),
    case("nav_state_reset", section_index=0, replay=[1, 2]),
    case("nav_state_reset", section_index=2, replay=[]),
    case("nav_state_reset", section_index=0, replay="ab"),
    case("nav_state_reset", section_index=0, replay=0),
    case("nav_state_reset", section_index=3, replay=[{"x": [1]}]),
    case("nav_state_reset",
         nav={"section_index": 5, "prompt_index": 9, "active_prompt_index": 2,
              "resolved_choices": ["a"], "replay_choices": ["b"]},
         section_index=-1),
    case("nav_state_reset", section_index=0, replay=False),
]

# ── _SetupGoBack.__init__ ──
for pi in (0, 1, 5, -1, 42):
    CASES.append(case("go_back_init", prompt_index=pi))

# ── _record_answers ──
CASES += [
    case("record_answers", nav=None, section_index=0, answers={}),
    case("record_answers", nav={"resolved_choices": [1, 2]}, section_index=0, answers={}),
    case("record_answers", nav={"resolved_choices": ["a"]}, section_index=2,
         answers={"2": ["old"], "0": []}),
    case("record_answers", nav={"resolved_choices": []}, section_index=1, answers={}),
    case("record_answers", nav={"resolved_choices": [{"x": 1}]}, section_index=0, answers={}),
    case("record_answers", nav=None, section_index=3, answers={"3": [9]}),
]

# ── _skip ──
CASES += [
    case("skip", {}, config={}, imported_from="", key="model", label="Model & Provider"),
    case("skip", {"_skip_configured_section": {"summary": "s"}},
         config={"model": {}}, imported_from="x", key="model", label="Model & Provider"),
    case("skip", {"_skip_configured_section": None},
         config={}, imported_from="x", key="model", label="L"),
    case("skip", {"_skip_configured_section": {}},
         config={}, imported_from="x", key="model", label="L"),
    case("skip", {"_skip_configured_section": {"__raise__": "ValueError", "msg": "s"}},
         config={}, imported_from="x", key="model", label="L"),
    bare("skip", config={}, imported_from="x", key="model", label="L"),
    case("skip", {"_skip_configured_section": ""},
         config={}, imported_from="x", key="model", label="L"),
]

# ── _gateway_step ──
CASES += [
    case("gateway_step", {"setup_gateway": None}, config={"gateway": {}}, imported_from=""),
    case("gateway_step", {"_skip_configured_section": None, "setup_gateway": None},
         config={}, imported_from="x"),
    case("gateway_step",
         {"_skip_configured_section": {"s": 1}, "ensure_gateway_service": None},
         config={}, imported_from="x"),
    case("gateway_step", {"setup_gateway": {"__raise__": "RuntimeError", "msg": "g"}},
         config={}, imported_from=""),
    case("gateway_step",
         {"_skip_configured_section": {"s": 1},
          "ensure_gateway_service": {"__raise__": "SystemExit", "msg": "1"}},
         config={}, imported_from="x"),
    bare("gateway_step", config={}, imported_from="x"),
]


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {f: n for f, n in counts.items() if n < 5}
    if short:
        print("GEN setup_flow2: FAIL funcs with <5 cases: %s" % short, file=sys.stderr)
        sys.exit(1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("GEN setup_flow2: %d cases, %d funcs -> %s" % (len(CASES), len(counts), OUT))
    for fn in sorted(counts):
        print("  %-26s %d" % (fn, counts[fn]))


if __name__ == "__main__":
    main()