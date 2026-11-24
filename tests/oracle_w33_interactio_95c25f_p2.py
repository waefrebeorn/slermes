#!/usr/bin/env python3
"""Differential oracle for plugins/model-providers/gemini/interactions.py, response side
(gate w33_interactio_95c25f_p2).

The SSE transport and the HTTP client are seams: fixtures feed the decoded
``InteractionSseEvent`` payloads (and the finished interaction bodies) that the wire
would have produced. What is compared is pure response shaping:

  * ``usage_namespace`` / ``merge_usage`` -- Interactions Usage -> OpenAI usage, and the
    multi-hop sum with hop 1's prompt as the context size;
  * ``_finish_reason`` -- Interaction.status -> OpenAI finish_reason;
  * ``interaction_error`` -- an in-band google.rpc error -> GeminiAPIError fields;
  * ``translate_interaction`` -- one (or every hop of a) chat.completion;
  * the ``StreamState`` machine behind ``translate_stream_event`` (step start/delta/stop,
    status checkpoints, hop completion, ``next_hop``, ``close_dropped_hop``,
    ``finish_ceiling``), ``event_token`` and ``iter_events``.

Minted ids and wall-clock fields (``chatcmpl-<hex>``, ``created``) are normalized on both
sides. A raise (failed interaction / stream error event) is reported as ``__raise__``.

The live module is imported by file path with the quarry on ``sys.path``.
"""
import importlib.util
import json
import re
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_interactio_95c25f_p2"
LIVE = QUARRY / "plugins" / "model-providers" / "gemini" / "interactions.py"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location("gemini_interactions_live_p2", LIVE)
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

WANTED = {"usage", "merge_usage", "finish_reason", "interaction_error", "translate",
          "stream", "event_token", "finish_ceiling", "iter_events"}
_MODEL_DEFAULT = "gemini-3-pro"
_CALL_RE = re.compile(r"^call_[0-9a-f]{12}$")
_ENV_RE = re.compile(r"^chatcmpl-[0-9a-f]{12}$")


def plain(obj):
    if isinstance(obj, SimpleNamespace):
        return {k: plain(v) for k, v in vars(obj).items()}
    if isinstance(obj, dict):
        return {k: plain(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [plain(v) for v in obj]
    return obj


def norm(obj):
    if isinstance(obj, dict):
        out = {}
        for key, value in obj.items():
            if key == "created":
                continue
            if key == "id" and isinstance(value, str):
                if _ENV_RE.match(value):
                    value = "chatcmpl-X"
                elif _CALL_RE.match(value):
                    value = "call-X"
            out[key] = norm(value)
        return out
    if isinstance(obj, list):
        return [norm(v) for v in obj]
    return obj


def error_body(exc):
    return {"message": str(exc), "code": exc.code, "status_code": exc.status_code,
            "details": plain(exc.details)}


def op_usage(req):
    return plain(live.usage_namespace(req.get("usage")))


def op_merge_usage(req):
    usages = req.get("usages")
    return live.merge_usage(usages if isinstance(usages, list) else [])


def op_finish_reason(req):
    return live._finish_reason(req.get("status") or "", bool(req.get("has_tool_calls")))


def op_interaction_error(req):
    kwargs = {}
    if "fallback" in req:
        kwargs["fallback"] = req["fallback"]
    err = live.interaction_error(req.get("errors"), **kwargs)
    return error_body(err)


def op_translate(req):
    try:
        out = live.translate_interaction(req.get("interactions") or [], req.get("model") or _MODEL_DEFAULT)
    except live.GeminiAPIError as exc:
        return {"__raise__": error_body(exc)}
    return plain(out)


def stream_state(state):
    return {
        "interaction_id": state.interaction_id,
        "status": state.status,
        "continuation_token": state.continuation_token,
        "tool_calls": state.tool_calls,
        "offset": state.offset,
        "hop_done": state.hop_done,
        "completed_hops": state.completed_hops,
        "hop_usage": plain(state.hop_usage),
        "checkpoint_usage": plain(state.checkpoint_usage),
    }


def op_stream(req):
    model = req.get("model") or _MODEL_DEFAULT
    state = live.StreamState()
    chunks = []
    try:
        for item in req.get("script") or []:
            if not isinstance(item, dict):
                continue
            if "op" in item:
                op = item["op"]
                if op == "next_hop":
                    state.next_hop()
                elif op == "close_dropped_hop":
                    state.close_dropped_hop()
                elif op == "finish_ceiling":
                    chunks.append(live.finish_ceiling(model, state))
                else:
                    raise AssertionError("unknown op %r" % (op,))
            else:
                event = item.get("event")
                chunks.extend(live.translate_stream_event(event if isinstance(event, dict) else {},
                                                          model, state))
    except live.GeminiAPIError as exc:
        return {"__raise__": error_body(exc)}
    return {"chunks": plain(chunks), "state": stream_state(state)}


def op_event_token(req):
    event = req.get("event")
    return live.event_token(event if isinstance(event, dict) else {})


def op_finish_ceiling(req):
    state = live.StreamState()
    hops = req.get("hop_usage")
    state.hop_usage = [u for u in hops if isinstance(u, dict)] if isinstance(hops, list) else []
    return plain(live.finish_ceiling(req.get("model") or _MODEL_DEFAULT, state))


def op_iter_events(req):
    payloads = req.get("lines")
    return [p for p in live.iter_events(payloads if isinstance(payloads, list) else [])]


OPS = {
    "usage": op_usage,
    "merge_usage": op_merge_usage,
    "finish_reason": op_finish_reason,
    "interaction_error": op_interaction_error,
    "translate": op_translate,
    "stream": op_stream,
    "event_token": op_event_token,
    "finish_ceiling": op_finish_ceiling,
    "iter_events": op_iter_events,
}


def expected(r):
    return norm(plain(OPS[r["func"]](r)))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = norm(r.get("out"))
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%s c=%s"
                  % (r.get("func"), r.get("case"), json.dumps(py, sort_keys=True),
                     json.dumps(got, sort_keys=True)), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
