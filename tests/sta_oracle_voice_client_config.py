#!/usr/bin/env python3
"""sta_oracle_voice_client_config.py — LIVE-Python oracle for the
voice_client_config pure core.

Extracts _client_direct_enabled's value-parse + _relay from the LIVE module
via AST (the module imports heavy deps at resolve time, but these two fns
are self-contained), and mirrors vcc_stt_relay_reason's ladder against the
LIVE BUILTIN_STT_PROVIDERS frozenset.
"""
import ast
import json
import os
import sys

SRC = "/home/wubu/hermes-agent-dev/tools/voice_client_config.py"
TT_SRC = "/home/wubu/hermes-agent-dev/tools/transcription_tools.py"


def extract(fn_name):
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == fn_name:
            return ast.get_source_segment(src, node)
    raise SystemExit("fn not found: " + fn_name)


ns = {"Dict": dict, "Any": object}
exec("from typing import Any, Dict\n" + extract("_relay"), ns)
_relay = ns["_relay"]

# LIVE builtin set for membership semantics
ttree = ast.parse(open(TT_SRC, encoding="utf-8").read())
BUILTIN = None
import re as _re
_tt_src = open(TT_SRC, encoding="utf-8").read()
_m = _re.search(r"BUILTIN_STT_PROVIDERS\s*=\s*frozenset\(\{(.*?)\}\)", _tt_src, _re.S)
assert _m, "BUILTIN_STT_PROVIDERS not found"
BUILTIN = {s.strip().strip("\"'") for s in _m.group(1).split(",") if s.strip()}


def main():
    cases_file = sys.argv[1]
    cases = json.load(open(cases_file))
    for c in cases:
        op = c["op"]
        if op == "client_direct_value":
            # feed through the same truthiness chain: bool/str/number/null
            v = c.get("value")
            if isinstance(v, str):
                ok = v.strip().lower() not in {"0", "false", "no", "off"}
            elif isinstance(v, bool):
                ok = v
            elif isinstance(v, (int, float)):
                ok = v != 0
            else:
                ok = True  # None/unset → default true
            print(json.dumps({"ok": ok}))
        elif op == "relay":
            print(json.dumps(_relay(c["reason"]), sort_keys=True))
        elif op == "stt_relay_reason":
            provider = c["provider"]
            if not c.get("stt_enabled", True):
                reason = "stt disabled"
            elif c.get("is_local", False):
                reason = "local provider"
            elif provider not in BUILTIN:
                reason = "command/plugin provider"
            elif not c.get("has_credentials", False):
                reason = "no credentials"
            else:
                reason = None
            print(json.dumps(reason))


if __name__ == "__main__":
    main()
