#!/usr/bin/env python3
"""
sta_oracle_acp_client.py — Oracle for the JSON-RPC / session-model shaping in
agent/acp_client.py (src/cli/port_acp_client.c).

The module imports the OpenAI bridge (openai SDK) servicing names it never uses
for these helpers, so that one dependency is stubbed; everything compared here
is the live function body.
"""
import json
import sys
import types

sys.path.insert(0, "C:/Users/eman5/hermes-agent")

_bridge = types.ModuleType("agent.acp_openai_bridge")
_bridge.completion_to_stream_chunks = lambda *a, **k: None
_bridge.extract_tool_calls_from_text = lambda *a, **k: []
_bridge.render_tool_bridge_sections = lambda *a, **k: []
sys.modules["agent.acp_openai_bridge"] = _bridge

import agent.acp_client as ACP  # noqa: E402

ENTRIES = [
    "[]",
    '[{"value":"a"},{"value":" b "},{"value":"a"},{"value":""},null,7,{"value":null},{"other":1}]',
    '{"value":"x"}',
    "null",
    '[{"modelId":"m1"},{"modelId":"m2"},{"modelId":"m1"}]',
    '[{"value":5},{"value":true}]',
]

SESSIONS = [
    "{}",
    '{"note":"no session keys"}',
    '{"configOptions":[]}',
    '{"configOptions":[{"id":"model","options":[{"value":"m-a"},{"value":"m-b"}]}]}',
    '{"configOptions":[{"category":"model","id":"cfg1","options":[{"value":"m-a"}]},{"id":"model","options":[{"value":"zzz"}]}]}',
    '{"configOptions":[{"category":"other","id":"notmodel"}]}',
    '{"models":{"availableModels":[{"modelId":"legacy-1"},{"modelId":"legacy-1"},{"modelId":"legacy-2"}]}}',
    '{"configOptions":[{"id":"model","options":"nope"}],"models":{"availableModels":[{"modelId":"fallback"}]}}',
]

SELECTIONS = [
    ('{"configOptions":[{"id":"model","options":[{"value":"m-a"}]}],"sessionId":"s1"}', "m-a", ""),
    ('{"configOptions":[{"id":"model","options":[{"value":"m-a"}]}],"sessionId":"s1"}', "m-b", ""),
    ('{"configOptions":[{"category":"model","id":"cfg9","options":[{"value":"m-a"}]}],"sessionId":" s1 "}', "m-a", ""),
    ('{"models":{"availableModels":[{"modelId":"legacy-1"}]},"sessionId":"s2"}', "legacy-1", ""),
    ('{"models":{"availableModels":[{"modelId":"legacy-1"}]},"sessionId":"s2"}', "legacy-9", ""),
    ('{"sessionId":"s3"}', "anything", ""),
    ('{"sessionId":""}', "m-a", ""),
    ('{"sessionId":"s4","models":{"availableModels":[{"modelId":"m-a"}]}}', "m-a", "m-a"),
    ('{"sessionId":"s5"}', "  m-a  ", ""),
]

TIMEOUTS = {
    "number_int": 30,
    "number_float": 30.5,
    "empty_object": {},
    "read_write": {"read": 10, "write": 20},
    "connect_pool": {"connect": 5, "pool": 7.5},
    "timeout_only": {"timeout": 1.25},
    "no_components": {},
}


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE acp_client: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        if fam == "result":
            idvals = {"string_id": ("m1", {"ok": True}),
                      "number_id": (7, None),
                      "null_id": (None, "text")}
            mid, result = idvals[case]
            want = ACP._jsonrpc_result(mid, result)
        elif fam == "error":
            if case == "basic":
                want = ACP._jsonrpc_error("m2", -32601, "Method not found")
            else:
                want = ACP._jsonrpc_error("m3", 1, "")
        elif fam == "ids":
            idx = int(case.split("_")[1])
            key = "modelId" if idx == 4 else "value"
            want = ACP._enabled_id_list(json.loads(ENTRIES[idx]), key)
        elif fam == "config_option":
            want = ACP._model_config_option(json.loads(SESSIONS[int(case.split("_")[1])]))
        elif fam == "session_ids":
            want = ACP._session_model_ids(json.loads(SESSIONS[int(case.split("_")[1])]))
        elif fam == "legacy_ids":
            want = ACP._legacy_session_model_ids(json.loads(SESSIONS[int(case.split("_")[1])]))
        elif fam == "selection":
            names = ["v1_ok", "v1_unknown", "v1_custom_config_id", "legacy_ok",
                     "legacy_unknown", "no_list_at_all", "empty_session",
                     "placeholder_id", "trimmed_model"]
            session, model, ph = SELECTIONS[names.index(case)]
            got_tuple = ACP._model_selection_request(
                json.loads(session), model, placeholder=ph)
            want = None if got_tuple is None else {"method": got_tuple[0],
                                                   "params": got_tuple[1]}
        elif fam == "timeout":
            raw = TIMEOUTS[case]
            want = ACP._effective_timeout(types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw)
            if abs(want - c["out"]) > 1e-9:
                mism += 1
                print(f"MISMATCH timeout {case}: C={c['out']} PY={want}")
            continue
        else:
            continue

        if c["out"] != want:
            mism += 1
            print(f"MISMATCH {case} [{fam}]:\n  C ={c['out']}\n  PY={want}")

    print(f"ORACLE acp_client: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
