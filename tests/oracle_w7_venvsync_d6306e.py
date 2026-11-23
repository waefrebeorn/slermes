#!/usr/bin/env python3
"""Differential oracle for hermes_cli/venv_sync.py (w7_venvsync_d6306e)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, pkg=False, **attrs):
    m = types.ModuleType(name)
    if pkg:
        m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_mod("hermes_cli", pkg=True)
_mod("hermes_cli.steward",
     UPDATE_MECHANISMS=frozenset({"self", "external", "nix", "default"}),
     read_install_stamp=lambda root: {}, sealed_steward=lambda root: None)

SRC = Q + "/hermes_cli/venv_sync.py"
spec = importlib.util.spec_from_file_location("w7_venvsync_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_venvsync_live"] = live
spec.loader.exec_module(live)

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
    try:
        if func == "is_read_only_gateway_status":
            py = live._is_read_only_gateway_status(r.get("command", []))
        elif func == "supervised_child":
            env = {
                "HERMES_SUPERVISED_CHILD": r.get("supervised", ""),
                "HERMES_S6_SUPERVISED_CHILD": r.get("s6", ""),
            }
            live.os = types.SimpleNamespace(environ=env)
            py = live._supervised_child()
        elif func == "completion_retry_constants":
            py = {
                "backoff_attempts": live.COMPLETION_RETRY_BACKOFF_ATTEMPTS,
                "backoff_seconds": live.COMPLETION_RETRY_BACKOFF_SECONDS,
                "max_attempts": live.COMPLETION_RETRY_MAX_ATTEMPTS,
            }
        elif func == "string_set":
            which = r.get("which")
            if which == "metadata_flags":
                py = sorted(live._METADATA_FLAGS)
            elif which == "report_only_update_flags":
                py = sorted(live._REPORT_ONLY_UPDATE_FLAGS)
            elif which == "tail_scripts":
                py = sorted(live._TAIL_SCRIPTS)
            elif which == "sync_inputs":
                py = list(live._SYNC_INPUTS)
            else:
                continue
        elif func == "relaunch_command":
            # python/root are injected as their str() values (plain strings).
            py = live.relaunch_command(
                r.get("python", ""), r.get("root", ""),
                r.get("argv", []), r.get("original", []), r.get("module"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_venvsync_d6306e: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
