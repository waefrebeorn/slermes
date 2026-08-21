#!/usr/bin/env python3
"""sta_oracle_stderr_timestamp_pure.py — oracle twin for stderr_timestamp_pure.c.

Calls the LIVE hermes_cli/stderr_timestamp.py pure helpers and emits one JSON
line per op. The C harness (t_port_stderr_timestamp_pure.c) emits the same
shape. run_oracle.sh diffs them.

NOT oracled: _timestamp() (uses datetime.now()).

Fixture shape:
{
  "ops": [
    {"op": "has_timestamp_prefix", "line": "2024-01-02 03:04:05,123 already stamped"},
    {"op": "command_exit_code", "rc": -9},
    {"op": "is_launchd_supervised", "xpc": "com.hermes.gateway"},
    {"op": "with_external_supervisor_flag", "argv": ["hermes","gateway","run"]},
    {"op": "prepare_child_command", "argv": ["hermes","gateway","run","--replace"], "xpc": "com.hermes"},
    {"op": "is_gateway_run_argv", "argv": ["hermes","gateway","run"]}
  ]
}
"""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "hermes_cli", "stderr_timestamp.py")
    spec = importlib.util.spec_from_file_location("live_stderr_ts", cand)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    path = sys.argv[1]
    with open(path, encoding="utf-8") as f:
        spec = json.load(f)
    mod = _load_module()
    out = []
    for case in spec.get("ops", []):
        op = case.get("op")
        if op == "has_timestamp_prefix":
            out.append({"op": op, "out": bool(mod._TIMESTAMP_PREFIX.match(case.get("line", "")))})
        elif op == "command_exit_code":
            out.append({"op": op, "out": mod._command_exit_code(int(case.get("rc", 0)))})
        elif op == "is_launchd_supervised":
            out.append({"op": op, "out": bool(mod._is_launchd_supervised({"XPC_SERVICE_NAME": case.get("xpc", "")}))})
        elif op == "with_external_supervisor_flag":
            argv = list(case.get("argv", []))
            out.append({"op": op, "out": mod._with_external_supervisor_flag(argv)})
        elif op == "prepare_child_command":
            argv = list(case.get("argv", []))
            out.append({"op": op, "out": mod._prepare_child_command(argv, {"XPC_SERVICE_NAME": case.get("xpc", "")})})
        elif op == "is_gateway_run_argv":
            out.append({"op": op, "out": bool(mod._is_hermes_gateway_run_argv(list(case.get("argv", []))))})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
