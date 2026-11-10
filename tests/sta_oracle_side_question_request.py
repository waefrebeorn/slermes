#!/usr/bin/env python3
"""
sta_oracle_side_question_request.py — Oracle for has_server_side_tools and
runtime_key in agent/side_question_request.py.
"""
import json
import sys
import types

sys.path.insert(0, "C:/Users/eman5/hermes-agent")

# The module pulls provider/background-review helpers at import time that are
# irrelevant to these two pure functions; stub the ones whose absence would
# abort the import.
for name in ("agent.background_review", "hermes_cli.runtime_provider_backends"):
    if name not in sys.modules:
        sys.modules[name] = types.ModuleType(name)
sys.modules["agent.background_review"]._parent_can_emit_tool_calls = lambda agent: True
sys.modules["agent.background_review"].build_cache_parity_fork = lambda *a, **k: None
sys.modules["hermes_cli.runtime_provider_backends"]._is_external_process_provider = (
    lambda provider: False)

try:
    import agent.side_question_request as SQ  # noqa: E402
except Exception:
    import importlib.util  # noqa: E402

    spec = importlib.util.spec_from_file_location(
        "agent.side_question_request",
        "C:/Users/eman5/hermes-agent/agent/side_question_request.py")
    SQ = importlib.util.module_from_spec(spec)
    sys.modules["agent.side_question_request"] = SQ
    spec.loader.exec_module(SQ)

KWARGS = [
    "{}",
    '{"tools":[{"type":"function","name":"read_file"}]}',
    '{"tools":[{"type":"web_search"}]}',
    '{"tools":[{"name":"no_type"},{"type":"custom"}]}',
    '{"tools":"nope"}',
    '{"web_search_options":{}}',
    '{"extra_body":{"web_search_options":null}}',
    '{"extra_body":{"tools":[{"type":"code_interpreter"}]}}',
    '{"plugins":[{"id":"pareto-router"}]}',
    '{"plugins":[{"id":"web"}]}',
    '{"plugins":["bare-string"]}',
    '{"plugins":[]}',
    '{"extra_body":{"plugins":[{"id":"pareto-router"},{"id":"web"}]}}',
    '{"extra_body":"not-an-object"}',
    '{"tools":[7,null,{"type":"function"}]}',
    "[]",
    "null",
]


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
        print("ORACLE side_question_request: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        if fam == "server_tools":
            kwargs = json.loads(KWARGS[int(case.split("_")[1])])
            if not isinstance(kwargs, dict):
                # Python indexes api_kwargs; a non-dict is a caller bug, not a case.
                continue
            want = SQ.has_server_side_tools(kwargs)
            got = bool(c["ret"])
        elif fam == "runtime_key":
            keys = [
                ("m1", "openai", "https://api.openai.com/v1", "chat", None),
                (None, None, None, None, None),
                ("m2", "", None, "responses", '{"effort":"high"}'),
            ]
            model, provider, base, mode, reason = keys[int(case.split("_")[1])]
            agent = types.SimpleNamespace()
            if model is not None:
                agent.model = model
            if provider is not None:
                agent.provider = provider
            if base is not None:
                agent.base_url = base
            if mode is not None:
                agent.api_mode = mode
            if reason is not None:
                agent.reasoning_config = json.loads(reason)
            want = list(SQ.runtime_key(agent))
            got = c["out"]
        else:
            continue

        if got != want:
            mism += 1
            print(f"MISMATCH {case} [{fam}]:\n  C ={got}\n  PY={want}")

    print(f"ORACLE side_question_request: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
