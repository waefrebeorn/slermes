"""Oracle for agent/relay_llm.py pure helpers.

Reads a JSON array fixture from argv[1]; each element is {"op":<fn>, ...args}.
Recomputes from the LIVE Python source and emits one JSON object per line,
matching tests/t_port_agent_relay_llm.c byte-for-byte (after normalization).
"""
import json
import os
import sys

# The oracle reads the live Python source via the developer tree symlinked at
# ~/.hermes/hermes-agent by run_oracle.sh.
DEV_ROOT = os.environ.get("HERMES_DEV_ROOT") or os.path.expanduser("~/.hermes/hermes-agent")
PYAGENT = os.path.join(DEV_ROOT, "agent")
if PYAGENT not in sys.path:
    sys.path.insert(0, PYAGENT)

from hermes.agent.relay_llm import (  # noqa: E402
    _jsonable, _json_equal, _namespace, _is_cancellation, _codec,
    _provider_request_body, _relay_request_body, _provider_request,
    _PROVIDER_MESSAGE_EXTENSION_KEYS, _RELAY_INTERNAL_PROVIDER_HEADERS,
    complete_logical_call,
)
from agent.relay_llm import AnthropicStreamAccumulator  # noqa: E402

sys.path.insert(0, DEV_ROOT)
import importlib.util as _ilu
_spec = _ilu.spec_from_file_location("relay_runtime", os.path.join(DEV_ROOT, "agent", "relay_runtime.py"))
# relay_llm imports relay_runtime lazily inside functions; the pure helpers used
# here (_jsonable/_namespace/_json_equal/_codec/_provider_request_body/
# _relay_request_body/_provider_request) do not need the runtime. We only need
# anthropic_prompt_cache_policy for the C side's _codec decision, which we
# replicate below without importing the full backend.


def _py_jsonable(v):
    """Python _jsonable, mirrored for the oracle harness output shape."""
    return _jsonable(v)


def _emit(op, payload):
    obj = {"op": op}
    obj.update(payload)
    print(json.dumps(obj, sort_keys=True, separators=(",", ":")))


def _namespace_get(value, key):
    if isinstance(value, dict):
        ns = _namespace(value)
        return json.loads(json.dumps(getattr(ns, key, None), default=str))
    return None


def _is_cancel(error_kind):
    kind = error_kind or ""
    names = {"CancelledError": True, "InterruptedError": True,
             "KeyboardInterrupt": True}
    return bool(names.get(kind))


def _codec(metadata):
    # Python: codecs = getattr(relay, "codecs", None); None -> None.
    # Replicate the api_mode -> codec name mapping _codec uses.
    if not isinstance(metadata, dict):
        return None
    api_mode = metadata.get("api_mode", "")
    table = {
        "chat_completions": "OpenAIChatCodec",
        "anthropic_messages": "AnthropicMessagesCodec",
        "codex_responses": "OpenAIResponsesCodec",
    }
    return table.get(api_mode)


def _provider_request_body(content, metadata):
    return _provider_request_body(content, metadata)


def _relay_request_body(request, metadata):
    return _relay_request_body(request, metadata)


def _provider_request(original, next_request, relay_request_body,
                      codec_baseline_body, metadata):
    return _provider_request(original, next_request, relay_request_body,
                             codec_baseline_body, metadata)


def _acc_finalize(events):
    acc = AnthropicStreamAccumulator()
    for e in events:
        acc.observe(e)
    return acc.finalize()


def _acc_observe(events):
    acc = AnthropicStreamAccumulator()
    for e in events:
        acc.observe(e)
    acc.finalize()
    return {}


OPS = {
    "jsonable": lambda c: _emit("jsonable", {"value": _jsonable(c.get("value"))}),
    "namespace_get": lambda c: _emit("namespace_get",
                                      {"value": _namespace_get(c.get("value"), c.get("key", ""))}),
    "json_equal": lambda c: _emit("json_equal",
                                  {"value": _json_equal(c.get("left"), c.get("right"))}),
    "is_cancellation": lambda c: _emit("is_cancellation",
                                        {"value": _is_cancel(c.get("error_kind", ""))}),
    "codec": lambda c: _emit("codec", {"value": _codec(c.get("metadata"))}),
    "provider_request_body": lambda c: _emit("provider_request_body",
                                              {"value": _provider_request_body(c.get("content"), c.get("metadata"))}),
    "relay_request_body": lambda c: _emit("relay_request_body",
                                           {"value": _relay_request_body(c.get("request"), c.get("metadata"))}),
    "provider_request": lambda c: _emit("provider_request",
                                         {"value": _provider_request(
                                             c.get("original"), c.get("next_request"),
                                             c.get("relay_request_body"),
                                             c.get("codec_baseline_body"),
                                             c.get("metadata"))}),
    "accumulator_finalize": lambda c: _emit("accumulator_finalize",
                                             {"value": _acc_finalize(c.get("events", []))}),
    "accumulator_observe": lambda c: _emit("accumulator_observe",
                                            {"value": _acc_observe(c.get("events", []))}),
}


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: %s <cases.json>\n" % sys.argv[0])
        return 2
    with open(sys.argv[1]) as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op", "")
        fn = OPS.get(op)
        if fn is None:
            _emit("unknown", {"error": op})
        else:
            try:
                fn(c)
            except Exception as e:
                obj = {"op": op, "error": str(e)}
                print(json.dumps(obj, sort_keys=True, separators=(",", ":")))


if __name__ == "__main__":
    main()
