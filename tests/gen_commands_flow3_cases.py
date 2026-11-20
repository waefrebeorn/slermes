#!/usr/bin/env python3
"""Generate tests/fixtures/commands_flow3.jsonl (deterministic).

Every case is {"func": <harness name>, "in": {...}}; the harness wires the
"seams" object to its universal seam and emits the output envelope.
Each of the 10 ported functions gets >= 5 cases, covering the branch structure
and the full builtin-exception-hierarchy catch sites.
"""
import json
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "fixtures", "commands_flow3.jsonl")


def cmd(name, **kw):
    d = {
        "name": name, "description": "desc-" + name, "category": "Session",
        "aliases": [], "args_hint": "", "subcommands": [],
        "cli_only": False, "gateway_only": False, "gateway_config_gate": None,
        "busy_policy": "reject", "busy_handler": None, "execute": None,
        "argument_mode": None, "desktop": None, "desktop_subcommands": None,
    }
    d.update(kw)
    return d


REG = [
    cmd("new", aliases=["reset"], args_hint="[name]",
        busy_policy="interrupt_then_dispatch", desktop="terminal"),
    cmd("model", aliases=["m"], args_hint="[name]", busy_policy="reject"),
    cmd("stop", busy_policy="interrupt_then_dispatch"),
    cmd("start", gateway_only=True, busy_policy="dispatch"),
    cmd("clear", cli_only=True, desktop="terminal"),
    cmd("adv", cli_only=True, gateway_config_gate="features.adv"),
    cmd("beta", cli_only=True, gateway_config_gate="features.beta"),
    cmd("plain"),
]

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


# ── infer_argument_mode ──
add("infer_argument_mode", cmd=cmd("a", argument_mode="options"))
add("infer_argument_mode", cmd=cmd("a", argument_mode="text", args_hint="[x]"))
add("infer_argument_mode", cmd=cmd("a", argument_mode="mixed", subcommands=["x"]))
add("infer_argument_mode", cmd=cmd("a"))
add("infer_argument_mode", cmd=cmd("a", args_hint="   <prompt>  "))
add("infer_argument_mode", cmd=cmd("a", subcommands=["on", "off"]))
add("infer_argument_mode", cmd=cmd("a", subcommands=["x"], args_hint="INSTRUCTIONS here"))
add("infer_argument_mode", cmd=cmd("a", subcommands=["x"], args_hint="[text custom]"))
add("infer_argument_mode", cmd=cmd("a", argument_mode="weird", args_hint="[N]"))
add("infer_argument_mode", cmd=cmd("a", args_hint="   "))

# ── command_desktop_meta ──
add("command_desktop_meta", cmd=cmd("a", argument_mode="text", desktop="terminal",
                                    desktop_subcommands=["install", "list"]))
add("command_desktop_meta", cmd=cmd("a", args_hint="[N]", desktop="hidden"))
add("command_desktop_meta", cmd=cmd("a", desktop_subcommands=[]))
add("command_desktop_meta", cmd=cmd("a", subcommands=["x"], args_hint="<prompt>"))
add("command_desktop_meta", cmd=cmd("a", argument_mode="mixed"))
add("command_desktop_meta", cmd=cmd("a", desktop="offline"))
add("command_desktop_meta", cmd=cmd("a", argument_mode="options", desktop_subcommands=["go"]))

# ── resolve_command ──
add("resolve_command", name="new", registry=REG)
add("resolve_command", name="/new", registry=REG)
add("resolve_command", name="RESET", registry=REG)
add("resolve_command", name="/reset", registry=REG)
add("resolve_command", name="unknown", registry=REG)
add("resolve_command", name="", registry=REG)
add("resolve_command", name="///new", registry=REG)
add("resolve_command", name="m", registry=REG)

# ── build_description (_build_description) ──
add("build_description", cmd=cmd("a"), seams={"describe": "Desc A"})
add("build_description", cmd=cmd("a", args_hint="<prompt>"),
    seams={"describe": "Desc A", "t": "Desc A (usage: <prompt>)"})
add("build_description", cmd=cmd("a"),
    seams={"describe": {"__raise__": "ValueError", "msg": "bad"}})
add("build_description", cmd=cmd("a"), seams={"describe": None})
add("build_description", cmd=cmd("a", args_hint="[x]"),
    seams={"describe": "D", "t": {"__raise__": "RuntimeError", "msg": "t failed"}})
add("build_description", cmd=cmd("a"),
    seams={"describe": {"__raise__": "KeyboardInterrupt", "msg": "stop"}})
add("build_description", cmd=cmd("a", args_hint="[x]"),
    seams={"describe": "D", "t": None})
add("build_description", cmd=cmd("a", args_hint="[x]"),
    seams={"describe": {"__raise__": "SystemExit", "msg": "bye"}})

# ── is_gateway_known_command ──
add("is_gateway_known_command", name=None, registry=REG)
add("is_gateway_known_command", name="", registry=REG)
add("is_gateway_known_command", name="start", registry=REG)
add("is_gateway_known_command", name="clear", registry=REG,
    seams={"_iter_plugin_command_entries": [[]]})
add("is_gateway_known_command", name="pluginx", registry=REG,
    seams={"_iter_plugin_command_entries": [[["pluginx", "d", ""]]]})
add("is_gateway_known_command", name="pluginx", registry=REG,
    seams={"_iter_plugin_command_entries": []})
add("is_gateway_known_command", name="adv", registry=REG)
add("is_gateway_known_command", name="pluginx", registry=REG,
    seams={"_iter_plugin_command_entries": {"__raise__": "RuntimeError", "msg": "boom"}})
add("is_gateway_known_command", name="pluginx", registry=REG,
    seams={"_iter_plugin_command_entries": {"__raise__": "SystemExit", "msg": "bye"}})

# ── is_interrupt_then_dispatch ──
for n in ("new", "reset", "/new", "stop", "model", "unknown", "", None):
    add("is_interrupt_then_dispatch", name=n, registry=REG)

# ── should_bypass_active_session ──
for n in ("new", "model", "plain", "unknown", "/reset", "RESET", None, ""):
    add("should_bypass_active_session", name=n, registry=REG)

# ── resolve_config_gates ──
add("resolve_config_gates", registry=[cmd("plain")])
add("resolve_config_gates", registry=REG[:6],
    import_gates={"hermes_cli.config": True})
add("resolve_config_gates", registry=REG[:6],
    seams={"read_raw_config": {"__raise__": "ValueError", "msg": "bad cfg"}})
add("resolve_config_gates", registry=REG[:6],
    seams={"read_raw_config": {"__raise__": "KeyboardInterrupt", "msg": "stop"}})
add("resolve_config_gates", registry=[cmd("adv", gateway_config_gate="features.adv")],
    seams={"read_raw_config": {}, "cfg_get": "yes", "is_truthy_value": True})
add("resolve_config_gates", registry=[cmd("adv", gateway_config_gate="features.adv")],
    seams={"read_raw_config": {}, "cfg_get": "no", "is_truthy_value": False})
add("resolve_config_gates",
    registry=[cmd("adv", gateway_config_gate="features.adv"),
              cmd("beta", gateway_config_gate="features.beta")],
    seams={"read_raw_config": {}, "cfg_get": ["v1", "v2"],
           "is_truthy_value": [True, False]})
add("resolve_config_gates", registry=[cmd("adv", gateway_config_gate="features.adv")],
    seams={"read_raw_config": {},
           "cfg_get": {"__raise__": "RuntimeError", "msg": "cfg boom"}})
add("resolve_config_gates", registry=[cmd("adv", gateway_config_gate="features.adv")],
    seams={"read_raw_config": {}, "cfg_get": "v",
           "is_truthy_value": {"__raise__": "TypeError", "msg": "truth boom"}})
add("resolve_config_gates", registry=[cmd("adv", gateway_config_gate="features.adv")],
    seams={"read_raw_config": None, "cfg_get": "v", "is_truthy_value": True})

# ── gateway_help_lines ──
add("gateway_help_lines", registry=[], seams={"_resolve_config_gates": [[]]})
add("gateway_help_lines", registry=[cmd("a")],
    seams={"_resolve_config_gates": [[]], "_is_gateway_available": True, "describe": "Desc A"})
add("gateway_help_lines", registry=[cmd("a", args_hint="[name]")],
    seams={"_resolve_config_gates": [[]], "_is_gateway_available": True, "describe": "Desc A"})
add("gateway_help_lines", registry=[cmd("a", aliases=["b"])],
    seams={"_resolve_config_gates": [[]], "_is_gateway_available": True,
           "describe": "Desc A", "t": " (alias: `/b`)"})
add("gateway_help_lines", registry=[cmd("reload_mcp", aliases=["reload-mcp"])],
    seams={"_resolve_config_gates": [[]], "_is_gateway_available": True, "describe": "Reload"})
add("gateway_help_lines", registry=[cmd("a")],
    seams={"_resolve_config_gates": [[]], "_is_gateway_available": False})
add("gateway_help_lines", registry=[cmd("a"), cmd("b")], allowed=["b"],
    seams={"_resolve_config_gates": [[]], "_is_gateway_available": [True, True],
           "describe": ["A", "B"]})
add("gateway_help_lines", registry=[cmd("a")], allowed=[],
    seams={"_resolve_config_gates": [[]], "_is_gateway_available": True})
add("gateway_help_lines", registry=[cmd("a")],
    seams={"_resolve_config_gates": {"__raise__": "ValueError", "msg": "gates boom"}})
add("gateway_help_lines", registry=[cmd("a")],
    seams={"_resolve_config_gates": [[]], "_is_gateway_available": True,
           "describe": {"__raise__": "RuntimeError", "msg": "desc boom"}})
add("gateway_help_lines", registry=[cmd("a")],
    seams={"_resolve_config_gates": [[]],
           "_is_gateway_available": {"__raise__": "TypeError", "msg": "avail boom"}})

# ── iter_plugin_command_entries ──
add("iter_plugin_command_entries", import_gates={"hermes_cli.plugins": True})
add("iter_plugin_command_entries", seams={"get_plugin_commands": {}})
add("iter_plugin_command_entries", seams={"get_plugin_commands": None})
add("iter_plugin_command_entries",
    seams={"get_plugin_commands": {"p1": {"description": "D1", "args_hint": " [x] "}}})
add("iter_plugin_command_entries",
    seams={"get_plugin_commands": {"p2": {}}, "t": "<default>"})
add("iter_plugin_command_entries",
    seams={"get_plugin_commands": {"p3": {"description": "", "args_hint": None}},
           "t": "<default>"})
add("iter_plugin_command_entries",
    seams={"get_plugin_commands": {"p4": {"description": 123, "args_hint": "[y]"}}})
add("iter_plugin_command_entries",
    seams={"get_plugin_commands": {"__raise__": "ValueError", "msg": "plugin bad"}})
add("iter_plugin_command_entries",
    seams={"get_plugin_commands": {"__raise__": "KeyboardInterrupt", "msg": "stop"}})
add("iter_plugin_command_entries",
    seams={"get_plugin_commands": {"p1": {"description": "D"}, "p2": {}},
           "t": ["<d1>", "<d2>"]})
add("iter_plugin_command_entries",
    seams={"get_plugin_commands": {"pBad": [1, 2]}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    print("per-func:", json.dumps(dict(sorted(counts.items()))))
    assert all(v >= 5 for v in counts.values()), "every func needs >=5 cases"


if __name__ == "__main__":
    main()
