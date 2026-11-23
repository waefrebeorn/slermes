#!/usr/bin/env python3
"""Differential oracle for adapter_slash_payload.py (gate slslash)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"; sys.path.insert(0, Q)
MAP = {"new":"/new","model":"/model","compact":"/compress","help":"/help","status":"/status",
       "config":"/config","sessions":"/sessions","resume":"/resume","compress":"/compress"}
cp = types.ModuleType("hermes_cli.commands_platforms")
cp.slack_subcommand_map = lambda: dict(MAP)
sys.modules["hermes_cli.commands_platforms"] = cp

def stub_pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name); m.__path__ = [os.path.join(Q, *name.split("."))]; sys.modules[name] = m
stub_pkg("hermes_cli")
name = "adapter_slash_payload_live"
sp = importlib.util.spec_from_file_location(
    name, os.path.join(Q, "plugins/platforms/slack/adapter_slash_payload.py"))
live = importlib.util.module_from_spec(sp); sys.modules[name] = live; sp.loader.exec_module(live)

def compute(r):
    cmd = r.get("command", {})
    if r.get("func") == "command_text": return live.slash_command_text(cmd)
    if r.get("func") == "thread_id": return live.slash_thread_id(cmd)
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") is None: continue
    cases += 1
    py, got = compute(r), r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH slslash %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE slslash: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
