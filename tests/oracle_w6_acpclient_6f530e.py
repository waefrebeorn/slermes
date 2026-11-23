#!/usr/bin/env python3
"""Differential oracle for the C11 port of agent/acp_client.py (w6_acpclient_6f530e).

Loads the LIVE module by path with the heavy subprocess/bridge imports stubbed
via sys.modules registration and a parent-package bootstrap. Replays each
harness output line against the identical live call and compares `out`.
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


# Parent-package bootstrap: a stub `agent` package stops the real __init__ from
# running; the four leaf imports acp_client needs at load time are registered
# before spec.loader.exec_module runs.
agent_pkg = _mod("agent")
agent_pkg.__path__ = [Q + "/agent"]
_mod("agent.acp_openai_bridge",
     completion_to_stream_chunks=lambda *a, **k: None,
     extract_tool_calls_from_text=lambda *a, **k: ([], ""),
     render_tool_bridge_sections=lambda *a, **k: [])
_mod("agent.file_safety",
     get_nt_namespace_error=lambda *a, **k: None,
     get_read_block_error=lambda *a, **k: None,
     get_write_denied_error=lambda *a, **k: None,
     is_write_approval_required=lambda *a, **k: False)
_mod("agent.redact", redact_sensitive_text=lambda s, **k: s)
_mod("agent.runtime_cwd", resolve_agent_cwd=lambda *a, **k: ".")

SRC = Q + "/agent/acp_client.py"
spec = importlib.util.spec_from_file_location("w6_acpclient_6f530e_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_acpclient_6f530e_live"] = live
spec.loader.exec_module(live)

NEVER = live._never_disabled


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def _selection_want(session, model, ph):
    r = live._model_selection_request(session, model, placeholder=ph, disabled=NEVER)
    if r is None:
        return None
    return {"method": r[0], "params": r[1]}


def _timeout_want(r):
    if r.get("is_number"):
        return live._effective_timeout(float(r.get("number", 0)))
    obj = types.SimpleNamespace()
    for attr in ("read", "write", "connect", "pool", "timeout"):
        if r.get("has_" + attr):
            setattr(obj, attr, float(r.get(attr, 0)))
    return live._effective_timeout(obj)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "result":
            want = live._jsonrpc_result(r.get("id"), r.get("result"))
        elif func == "error":
            want = live._jsonrpc_error(r.get("id"), r.get("code"), r.get("message") or "")
        elif func == "ids":
            want = live._enabled_id_list(r.get("entries"), r.get("key") or "", NEVER)
        elif func == "config_option":
            want = live._model_config_option(r.get("session"))
        elif func == "session_ids":
            want = live._session_model_ids(r.get("session"), NEVER)
        elif func == "legacy_ids":
            want = live._legacy_session_model_ids(r.get("session"), NEVER)
        elif func == "selection":
            want = _selection_want(r.get("session"), r.get("model") or "", r.get("ph") or "")
        elif func == "timeout":
            want = _timeout_want(r)
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s case=%s: %r" % (func, r.get("case"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if _canon(want) != _canon(got):
        mism += 1
        print("MISMATCH %s case=%s C=%r PY=%r" % (func, r.get("case"), got, want),
              file=sys.stderr)

if cases == 0:
    print("ORACLE w6_acpclient_6f530e: 0 cases (link failure in disguise)")
    sys.exit(1)
print("ORACLE w6_acpclient_6f530e: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
