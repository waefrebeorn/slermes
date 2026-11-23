#!/usr/bin/env python3
"""Differential oracle for agent/sdk_transform_bypass.py (w13_sdktransfo_241aa3).

The escape-hatch env read and the OpenAI-SDK detection are explicit seams:
`env_value` is injected into a private os namespace (no process env mutation,
avoiding the Windows CRT empty-string-unset pitfall), and `is_openai` selects
the __module__ of a stand-in completions object.
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

SRC = Q + "/agent/sdk_transform_bypass.py"
spec = importlib.util.spec_from_file_location("sdk_transform_bypass_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sdk_transform_bypass_live"] = live
spec.loader.exec_module(live)
live.os = types.SimpleNamespace(environ={})

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
    live.os.environ[live.ESCAPE_HATCH_ENV] = r.get("env_value", "")
    try:
        if func == "plain":
            py = live._is_plain_json_data(r.get("value"))
        elif func == "bypass":
            fields = r.get("fields")
            kwargs = r.get("request_kwargs") or {}
            if fields is None:
                py = live.bypass_sdk_request_transform(
                    kwargs, keep_slots=bool(r.get("keep_slots", False)))
            else:
                py = live.bypass_sdk_request_transform(
                    kwargs, tuple(fields), keep_slots=bool(r.get("keep_slots", False)))
        elif func == "chat":
            mod = "openai.resources.chat" if r.get("is_openai") else "hermes.fake"
            completions = type("Completions", (), {"__module__": mod})()
            client = types.SimpleNamespace(chat=types.SimpleNamespace(completions=completions))
            py = live.bypass_chat_sdk_request_transform(r.get("request_kwargs") or {}, client)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} case={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w13_sdktransfo_241aa3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
