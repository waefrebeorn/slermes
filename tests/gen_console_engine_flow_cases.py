#!/usr/bin/env python3
"""Generate tests/fixtures/console_engine_flow.jsonl — {func,in} cases for
t_port_console_engine_flow.c / oracle_console_engine_flow.py.

23 targets (18 unclaimed callables of hermes_cli/console_engine.py + 5 unclaimed
module constants), >=5 cases each. The claimed siblings (_capture_output,
_expect_no_args, _format_job, _summaries_from_parser, _noop_console_command) and
every foreign collaborator (argparse ctor/methods, importlib, the persistence /
cron / session_export imports, strip_ansi) enter through ONE universal seam; a
script is a per-call RESULT SEQUENCE when it is a JSON array (last value
repeats), a single repeated result otherwise, null == None, and
{"__raise__": kind, "msg": ...} == a raised exception. A MISSING or empty script
fail-closes to RuntimeError. List-returning seams wrap one level deeper ([[...]]).

Callable doubles are scripted {"__callable__": name}; object doubles (parser /
subparsers / top-parser / module / scope / namespace) are plain dicts the oracle
materializes by SEAM NAME. Integral values are written as ints (the C-serialize
round-trip keeps isinstance(int) faithful on the live side). ASCII scope.
"""
import json
from pathlib import Path

CASES = []


def add(_f, **inp):
    CASES.append({"func": _f, "in": inp})


def CALL(name):
    return {"__callable__": name}


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def surface(kind="extracted", module="hermes_cli.subcommands.dump",
            builder="build_dump_parser", handler="cmd_dump"):
    return {"kind": kind, "module": module, "builder": builder, "handler": handler}


# ── _paths (pure) ──
add("paths", spec="list, *snapshot export")
add("paths", spec=".")
add("paths", spec="*.")
add("paths", spec="list, *enable, *disable, *post-setup")
add("paths", spec="  ,  ,")
add("paths", spec="*boards create, boards list")
add("paths", spec="a b c, *d")

# ── _sub (pure ctor) ──
add("sub", module="dump", builder="build_dump_parser", handler="cmd_dump")
add("sub", module="prompt_size", builder="build_prompt_size_parser", handler="cmd_prompt_size")
add("sub", module="cron", builder="build_cron_parser", handler="cmd_cron")
add("sub", module="import_cmd", builder="build_import_cmd_parser", handler="cmd_import")
add("sub", module="x", builder="b", handler="h")

# ── _reg (pure ctor) ──
add("reg", module="bundles", handler="bundles_command")
add("reg", module="checkpoints")
add("reg", module="curator")
add("reg", module="pets")
add("reg", module="bundles", handler=None)

# ── _drop_trailing_blank (strip_ansi seam; ret = mutated lines) ──
add("drop_trailing_blank", lines=["a", "b"], seams={"strip_ansi": ["a", "b"]})
add("drop_trailing_blank", lines=["a", "  ", ""], seams={"strip_ansi": ["  ", "a"]})
add("drop_trailing_blank", lines=["", "", ""], seams={"strip_ansi": ["", "", ""]})
add("drop_trailing_blank", lines=[], seams={"strip_ansi": []})
add("drop_trailing_blank", lines=["x"], seams={"strip_ansi": ["x"]})
add("drop_trailing_blank", lines=["a", "\t", " \n"], seams={"strip_ansi": [" \n", "\t", "a"]})
add("drop_trailing_blank", lines=["a", "b"], seams={})  # fail-closed on first strip_ansi

# ── _SHELL_TOKENS membership ──
for tok in ["|", "||", "&", "&&", ";", ">", ">>", "<", "<<", "2>", "2>>", "x", "", "2>"]:
    add("shell_tokens", token=tok)

# ── _CONFIRMED_KINDS membership ──
for k in ["extracted", "registered", "builder", "adder", "", "Extracted"]:
    add("confirmed_kinds", kind=k)

# ── _BLOCKED_TOP membership ──
for t in ["chat", "gateway", "login", "update", "whatsapp-cloud", "status", "", "moa", "oneshot"]:
    add("blocked_top", token=t)

# ── _BLOCKED_PAIRS lookup ──
add("blocked_pairs", tokens=["config", "edit"])
add("blocked_pairs", tokens=["profile", "alias"])
add("blocked_pairs", tokens=["kanban", "tail"])
add("blocked_pairs", tokens=["sessions", "delete"])
add("blocked_pairs", tokens=["sessions", "prune"])
add("blocked_pairs", tokens=["config", "show"])
add("blocked_pairs", tokens=["kanban"])
add("blocked_pairs", tokens=[])
add("blocked_pairs", tokens=["skills", "publish", "extra"])

# ── _SEND_SURFACE constant ──
for _ in range(5):
    add("send_surface")

# ── _parse (argparse flow) ──
PN = {"id": "p1"}
add("parse", prog="logs", args=["agent", "-n", "10"],
    specs=[{"flags": ["log_name"], "kwargs": {"nargs": "?", "default": "agent"}},
           {"flags": ["-n", "--lines"], "kwargs": {"type": "int", "default": 50}},
           {"bare": "--level"}],
    seams={"parser_new": PN, "add_argument": [None, None, None],
           "parse_args": {"log_name": "agent", "lines": 10, "level": None}})
add("parse", prog="sessions list", args=["--limit", "5"],
    specs=[{"flags": ["--limit"], "kwargs": {"type": "int", "default": 20}}],
    seams={"parser_new": PN, "add_argument": None, "parse_args": {"limit": 5}})
add("parse", prog="cron list", args=[], specs=[{"flags": ["--all"], "kwargs": {"action": "store_true"}}],
    seams={"parser_new": PN, "add_argument": None, "parse_args": {"all": False}})
add("parse", prog="logs", args=["-x"], specs=[],
    seams={"parser_new": PN, "parse_args": R("ConsoleCommandError", "logs: unrecognized arguments: -x")})
add("parse", prog="p", args=[], specs=[], seams={"parser_new": PN})  # fail-closed parse_args
add("parse", prog="p", args=[], specs=[{"bare": "--force"}],
    seams={"parser_new": R("RuntimeError", "ctor boom"), "add_argument": None, "parse_args": {}})
add("parse", prog="p", args=[], specs=[{"flags": ["a"], "kwargs": {}}],
    seams={"parser_new": PN, "add_argument": R("ValueError", "bad spec"), "parse_args": {}})

# ── _captured factory (ret = wrapper marker) ──
add("captured", fn=CALL("handler"))
add("captured", fn=CALL("h2"))
add("captured", fn=None)
add("captured", fn=CALL("handler3"))
add("captured", fn=CALL("x"))

# ── _captured.wrapper (full closure; _capture_output black box) ──
add("captured_wrapper", fn=CALL("handler"), engine={"id": "e1"}, args=["a"],
    seams={"_capture_output": "text out"})
add("captured_wrapper", fn=CALL("handler"), engine={"id": "e1"}, args=[],
    seams={"_capture_output": ""})
add("captured_wrapper", fn=CALL("handler"), engine=None, args=["a", "b"],
    seams={"_capture_output": R("ConsoleCommandError", "boom")})
add("captured_wrapper", fn=CALL("handler"), engine={"id": "e1"}, args=["a"], seams={})  # fail-closed
add("captured_wrapper", fn=CALL("handler"), engine={"id": "e1"}, args=["a"],
    seams={"_capture_output": R("SystemExit", "1")})

# ── _simple_command factory (ret = handler marker) ──
add("simple_command", usage="version", module="hermes_cli._startup_fast",
    name="print_fast_version_info", kwargs={"check_updates": True})
add("simple_command", usage="doctor", module="hermes_cli.doctor", name="run_doctor")
add("simple_command", usage="config show", module="hermes_cli.config_show", name="show_config")
add("simple_command", usage="cron status", module="hermes_cli.cron", name="cron_status")
add("simple_command", usage="x", module="m", name="n", make_args=CALL("mk"), kwargs={"a": 1})

# ── _simple_command.handler (full closure) ──
MOD = {"id": "m1"}
add("simple_command_handler", usage="version", module="hermes_cli._startup_fast",
    name="print_fast_version_info", args=[],
    seams={"_expect_no_args": None, "import_module": MOD, "getattr": CALL("print_fast_version_info"),
           "_capture_output": "v1.0"})
add("simple_command_handler", usage="version", module="m", name="n", args=["extra"],
    seams={"_expect_no_args": R("ConsoleCommandError", "Usage: version")})
add("simple_command_handler", usage="doctor", module="hermes_cli.doctor", name="run_doctor", args=[],
    seams={"_expect_no_args": None, "import_module": R("ImportError", "no mod"), "getattr": CALL("x"),
           "_capture_output": "ok"})
add("simple_command_handler", usage="x", module="m", name="n", args=[],
    seams={"_expect_no_args": None, "import_module": MOD, "getattr": R("AttributeError", "no attr"),
           "_capture_output": "ok"})
add("simple_command_handler", usage="x", module="m", name="n", args=[], seams={})  # fail-closed
add("simple_command_handler", usage="config show", module="hermes_cli.config_show", name="show_config",
    args=[], seams={"_expect_no_args": None, "import_module": MOD, "getattr": CALL("show_config"),
                    "_capture_output": R("RuntimeError", "cfg write guard")})

# ── _dispatch (full flow; composes surface_build REAL) ──
def dispatch_seams(**over):
    base = {"parser_new": {"id": "p1"}, "add_subparsers": {"id": "sp1"},
            "import_module": {"id": "m1"}, "getattr": CALL("entry"),
            "entry": {"id": "top1"}, "parse_args": {"func": CALL("handler"), "yes": False},
            "_capture_output": "dispatched"}
    base.update(over)
    return base


add("dispatch", surface=surface(), root="dump", fixed=[], args=["--x"],
    seams=dispatch_seams())
add("dispatch", surface=surface(), root="dump", fixed=["a"], args=["b"], namespace_update=None,
    seams=dispatch_seams(parse_args={"func": CALL("handler")}))
add("dispatch", surface=surface(), root="dump", fixed=[], args=[],
    seams=dispatch_seams(parse_args={"func": None}))  # not callable -> ConsoleCommandError
add("dispatch", surface=surface(), root="dump", fixed=[], args=[],
    seams=dispatch_seams(parse_args={"yes": True}))  # func absent -> not callable
add("dispatch", surface=surface(), root="dump", fixed=[], args=[],
    seams=dispatch_seams(parse_args={"func": "notcallable"}))
add("dispatch", surface=surface(), root="dump", fixed=[], args=[], namespace_update=CALL("nu"),
    seams=dispatch_seams(namespace_update=None, parse_args={"func": CALL("handler")}))
add("dispatch", surface=surface(), root="dump", fixed=[], args=[], seams={})  # fail-closed
add("dispatch", surface=surface(), root="dump", fixed=[], args=[],
    seams=dispatch_seams(parse_args=R("ConsoleCommandError", "dump: bad args")))
add("dispatch", surface=surface(kind="registered", module="hermes_cli.bundles",
                                builder="register_cli", handler="bundles_command"),
    root="bundles", fixed=[], args=["list"],
    seams={"parser_new": {"id": "p1"}, "add_subparsers": {"id": "sp1"},
           "import_module": {"id": "m1"}, "getattr": [CALL("entry"), CALL("bundles_command")],
           "add_parser": {"id": "top1"}, "entry": None, "set_defaults": None,
           "parse_args": {"func": CALL("handler")}, "_capture_output": "ok"})

# ── _dispatch inner lambda (func(namespace)) ──
add("dispatch_lambda", func=CALL("handler"), namespace={"a": 1}, seams={"func": "result"})
add("dispatch_lambda", func=CALL("handler"), namespace={}, seams={"func": None})
add("dispatch_lambda", func=CALL("handler"), namespace={"a": 1},
    seams={"func": R("RuntimeError", "boom")})
add("dispatch_lambda", func=CALL("handler"), namespace={"a": 1}, seams={})  # fail-closed
add("dispatch_lambda", func=CALL("handler"), namespace={"a": 1},
    seams={"func": R("SystemExit", "2")})

# ── _CliSurface.build (method; 4 kinds x live) ──
def build_seams(**over):
    base = {"parser_new": {"id": "p1"}, "add_subparsers": {"id": "sp1"},
            "import_module": {"id": "m1"}, "getattr": CALL("entry"), "entry": {"id": "top1"}}
    base.update(over)
    return base


add("surface_build", surface=surface(), root="dump", live=False, seams=build_seams())
add("surface_build", surface=surface(), root="dump", live=True,
    seams=build_seams(getattr=[CALL("entry"), CALL("cmd_dump")],
                      import_module=[{"id": "m1"}, {"id": "main1"}]))
add("surface_build", surface=surface(kind="registered", module="hermes_cli.bundles",
                                     builder="register_cli", handler="bundles_command"),
    root="bundles", live=True,
    seams=build_seams(getattr=[CALL("entry"), CALL("bundles_command")],
                      add_parser={"id": "top1"}, entry=None, set_defaults=None))
add("surface_build", surface=surface(kind="registered", module="hermes_cli.checkpoints",
                                     builder="register_cli", handler=None),
    root="checkpoints", live=False,
    seams=build_seams(add_parser={"id": "top1"}, entry=None))
add("surface_build", surface=surface(kind="builder", module="hermes_cli.projects_cmd",
                                     builder="build_parser", handler="cmd_project"),
    root="project", live=True,
    seams=build_seams(getattr=[CALL("entry"), CALL("cmd_project")],
                      import_module=[{"id": "m1"}, {"id": "main1"}],
                      entry={"id": "top1"}, set_defaults=None))
add("surface_build", surface=surface(kind="adder", module="hermes_cli.portal_cli",
                                     builder="add_parser", handler=None),
    root="portal", live=False, seams=build_seams(entry=None))
add("surface_build", surface=surface(), root="dump", live=False, seams={})  # fail-closed
add("surface_build", surface=surface(), root="dump", live=True,
    seams=build_seams(import_module=R("ImportError", "no module")))

# ── _CliSurface.build main_handler lambda ──
add("build_main_handler", handler="cmd_dump", live=True,
    seams={"import_module": {"id": "main1"}, "getattr": CALL("cmd_dump")})
add("build_main_handler", handler="cmd_dump", live=False, seams={})
add("build_main_handler", handler="cmd_cron", live=True,
    seams={"import_module": {"id": "main1"}, "getattr": CALL("cmd_cron")})
add("build_main_handler", handler="cmd_dump", live=True,
    seams={"import_module": R("ImportError", "no main")})
add("build_main_handler", handler="cmd_dump", live=True,
    seams={"import_module": {"id": "main1"}, "getattr": R("AttributeError", "no handler")})
add("build_main_handler", handler="cmd_dump", live=True, seams={})  # fail-closed

# ── _surface_summaries (composes build REAL; _summaries_from_parser seam) ──
add("surface_summaries", surface=surface(), root="dump",
    seams=dict(build_seams(), _summaries_from_parser={"dump": "Dump stuff."}))
add("surface_summaries", surface=surface(), root="dump",
    seams=dict(build_seams(), _summaries_from_parser={}))
add("surface_summaries", surface=surface(), root="dump",
    seams=dict(build_seams(), _summaries_from_parser=R("RuntimeError", "parse boom")))
add("surface_summaries", surface=surface(), root="dump",
    seams=dict(build_seams(), _summaries_from_parser=R("SystemExit", "1")))
add("surface_summaries", surface=surface(), root="dump", seams={})  # fail-closed build -> {}
add("surface_summaries", surface=surface(), root="dump",
    seams=dict(build_seams(), import_module=R("ImportError", "no mod")))

# ── _session_db (persistence_assembly imports) ──
add("session_db", read_only=True, seams={"profile_scope": {"profile_dir": "/h/p1"},
                                         "open_persistence_reader": {"id": "store1"}})
add("session_db", read_only=False, seams={"profile_scope": {"profile_dir": "/h/p1"},
                                          "open_persistence": {"id": "store2"}})
add("session_db", read_only=True, seams={"profile_scope": {"profile_dir": ""},
                                         "open_persistence_reader": None})
add("session_db", read_only=True, seams={"profile_scope": R("RuntimeError", "no scope")})
add("session_db", read_only=False, seams={"profile_scope": {"profile_dir": "/h/p1"},
                                          "open_persistence": R("OSError", "locked")})
add("session_db", read_only=True, seams={})  # fail-closed

# ── _guard_exports (ExportTooLarge identity catch) ──
add("guard_exports", store={"id": "s1"}, session_ids=["a", "b"],
    seams={"assert_exports_safe": None})
add("guard_exports", store={"id": "s1"}, session_ids=["a"],
    seams={"assert_exports_safe": R("ExportTooLarge", "session a too large")})
add("guard_exports", store={"id": "s1"}, session_ids=[],
    seams={"assert_exports_safe": R("RuntimeError", "other boom")})
add("guard_exports", store={"id": "s1"}, session_ids=["a"],
    seams={"assert_exports_safe": R("SystemExit", "1")})
add("guard_exports", store={"id": "s1"}, session_ids=["a"], seams={})  # fail-closed
add("guard_exports", store=None, session_ids=["a"], seams={"assert_exports_safe": None})

# ── _cron_job_action (run callback + _format_job seam) ──
add("cron_job_action", args=["j1"], usage="cron pause <job>", action="Paused",
    seams={"run": {"id": "j1", "name": "N"}, "_format_job": "Paused job: N (j1) [paused]"})
add("cron_job_action", args=[], usage="cron pause <job>", action="Paused", seams={})
add("cron_job_action", args=["j1", "j2"], usage="cron run <job>", action="Triggered", seams={})
add("cron_job_action", args=["j1"], usage="cron pause <job>", action="Paused",
    seams={"run": None})  # job not found
add("cron_job_action", args=["j1"], usage="cron pause <job>", action="Paused",
    seams={"run": R("AmbiguousJobReference", "ambiguous j1")})
add("cron_job_action", args=["j1"], usage="cron run <job>", action="Triggered",
    seams={"run": R("RuntimeError", "boom")})
add("cron_job_action", args=["j1"], usage="cron pause <job>", action="Paused", seams={})  # fail-closed
add("cron_job_action", args=["j1"], usage="cron pause <job>", action="Paused",
    seams={"run": {"id": "j1"}, "_format_job": R("ValueError", "fmt boom")})

# ── _doctor handler (composes _simple_command REAL + handler closure) ──
add("doctor", args=[], seams={"_expect_no_args": None, "import_module": {"id": "m1"},
                              "getattr": CALL("run_doctor"), "_capture_output": "doctor ok"})
add("doctor", args=["x"], seams={"_expect_no_args": R("ConsoleCommandError", "Usage: doctor")})
add("doctor", args=[], seams={"_expect_no_args": None, "import_module": {"id": "m1"},
                              "getattr": CALL("run_doctor"), "_capture_output": ""})
add("doctor", args=[], seams={"_expect_no_args": None, "import_module": R("ImportError", "no doctor")})
add("doctor", args=[], seams={})  # fail-closed
add("doctor", args=[], seams={"_expect_no_args": None, "import_module": {"id": "m1"},
                              "getattr": CALL("run_doctor"), "_capture_output": R("RuntimeError", "guard")})


def main():
    out = Path(__file__).resolve().parent / "fixtures" / "console_engine_flow.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func counts
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    bad = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), out))
    if bad:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % bad)
    print("per-func:", json.dumps(counts, sort_keys=True))


if __name__ == "__main__":
    main()