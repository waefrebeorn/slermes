#!/usr/bin/env python3
"""Differential oracle for w11_approvalen_62f1c4 (tools/approval_env_split.py).

The two function-local imports are stubbed with their real quarry semantics:
env's option-with-argument set and the getopt short-bundle walker.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


tools = _mod("tools")
tools.__path__ = []
_mod("tools.approval_detection",
     _COMMAND_WRAPPER_OPTIONS_WITH_ARG={
         "env": {"-a", "--argv0", "-C", "--chdir", "-S", "--split-string", "-u", "--unset"}})


def _short_bundle(base, word):
    valued = {"-u", "-C", "-a", "-S"}
    for index, char in enumerate(word[1:], start=2):
        if "-" + char in valued:
            return word[1:index - 1], "-" + char, word[index:]
    return word[1:], None, ""


_mod("tools.approval_wrappers", _short_bundle=_short_bundle)

SRC = Q + "/tools/approval_env_split.py"
spec = importlib.util.spec_from_file_location("w11_approvalen_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_approvalen_live"] = live
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
        if func == "split_env_string":
            py = live._split_env_string(r.get("payload", ""))
        elif func == "env_split_payload":
            py = live._env_split_payload(r.get("tokens", []))
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
print(f"ORACLE w11_approvalen_62f1c4: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
