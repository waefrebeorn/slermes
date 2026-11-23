#!/usr/bin/env python3
"""Differential oracle for two pure helpers in hermes_cli/cli_config_load.py
(gate w13_cliconfig_2dca4f). The env-dependent helper uses an injected env
carrier via a stubbed agent.session_client_env.session_env (no env mutation).
"""
import copy
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/cli_config_load.py"
spec = importlib.util.spec_from_file_location("w13cfgload_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

# Parent-package bootstrap + late-import seam for session_env.
agent_pkg = types.ModuleType("agent"); agent_pkg.__path__ = []
sce = types.ModuleType("agent.session_client_env")
sys.modules["agent"] = agent_pkg
sys.modules["agent.session_client_env"] = sce


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
    got = r.get("out")
    if func == "merge_file_config":
        defaults = copy.deepcopy(r.get("defaults") or {})
        file_config = r.get("file_config") or {}
        live._merge_file_config(defaults, file_config)
        want = defaults
    elif func == "resolve_prefill_messages_file":
        sce.session_env = lambda key: r.get("env_value", "") or ""
        want = live._resolve_prefill_messages_file(r.get("config") or {})
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_cliconfig_2dca4f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
