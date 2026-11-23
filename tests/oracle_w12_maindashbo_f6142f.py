#!/usr/bin/env python3
"""Oracle for hermes_cli/main_dashboard.py pure helpers."""
import ast, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/main_dashboard.py"
source = open(SRC, encoding="utf-8").read()
tree = ast.parse(source)
wanted = ("_dashboard_probe_host", "_extract_scope_from_cgroup",
          "_systemd_restart_commands", "_format_restart_commands",
          "_explicit_endpoint_flags", "_is_electron_packaged_web_dist")
body = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in wanted]
module = ast.fix_missing_locations(ast.Module(
    body=[ast.ImportFrom(module="__future__", names=[ast.alias(name="annotations")], level=0)] + body,
    type_ignores=[]))
ns = {}
exec(compile(module, SRC, "exec"), ns)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func == "probe_host":
        py = ns["_dashboard_probe_host"](r.get("host"))
    elif func == "scope":
        py = ns["_extract_scope_from_cgroup"](r.get("entry"))
    elif func == "restart_commands":
        py = ns["_systemd_restart_commands"](r.get("unit", ""), r.get("scope"))
    elif func == "format_restart":
        py = ns["_format_restart_commands"](r.get("commands", []))
    elif func == "endpoint_flags":
        # Python returns a set -> sorted for a stable wire form.
        py = sorted(ns["_explicit_endpoint_flags"](r.get("argv", [])))
    elif func == "electron_dist":
        py = ns["_is_electron_packaged_web_dist"](r.get("path"))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_maindashbo_f6142f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
