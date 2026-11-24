#!/usr/bin/env python3
"""Differential oracle for plugins/model-providers/gemini/interactions.py, request side
(gate w33_interactio_95c25f_p1).

The wire (``POST {base}/interactions``) is a seam: nothing here opens an HTTP client.
What is compared is the pure OpenAI-messages -> Interactions-steps shaping --
``build_input_steps`` (content blocks, image data URLs, tool results, assistant
thought/model_output/function_call steps), ``build_interactions_request``
(tools, tool_choice, thinking config, output budget, response_format, store),
``continuation_request`` and the small helpers they are built from.

The live module is imported by file path (the ``model-providers`` directory is not an
importable package name) with the quarry on ``sys.path`` so its ``agent.*`` imports
resolve; the host-blocked heavy deps are stubbed first.

Nondeterministic values are normalized on both sides: a minted uuid call id
(``call_<12 hex>``) becomes ``call-X``.
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
GATE = "w33_interactio_95c25f_p1"
LIVE = QUARRY / "plugins" / "model-providers" / "gemini" / "interactions.py"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location("gemini_interactions_live_p1", LIVE)
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

WANTED = {
    "input_steps", "request", "continuation", "tools", "tool_choice", "response_format",
    "image", "content_blocks", "function_result", "call_arguments",
}
_MODEL_DEFAULT = "gemini-3-pro"
_CALL_RE = re.compile(r"^call_[0-9a-f]{12}$")
_ENV_RE = re.compile(r"^chatcmpl-[0-9a-f]{12}$")

REQUEST_KEYS = ("model", "messages", "tools", "tool_choice", "temperature", "max_tokens",
                "top_p", "stop", "thinking_config", "response_format")


def plain(obj):
    """SimpleNamespace tree -> JSON-able values."""
    if isinstance(obj, SimpleNamespace):
        return {k: plain(v) for k, v in vars(obj).items()}
    if isinstance(obj, dict):
        return {k: plain(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [plain(v) for v in obj]
    return obj


def norm(obj):
    """Erase the values a mint / a clock decides."""
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


def op_input_steps(req):
    steps, system = live.build_input_steps(req.get("messages") or [])
    return {"steps": steps, "system_instruction": system}


def op_request(req):
    kwargs = {k: req[k] for k in REQUEST_KEYS if k in req}
    kwargs.setdefault("model", _MODEL_DEFAULT)
    kwargs.setdefault("messages", [])
    return live.build_interactions_request(**kwargs)


def op_continuation(req):
    return live.continuation_request(req.get("request") or {}, req.get("interaction_id") or "",
                                     req.get("token") or "")


def op_tools(req):
    return live._tools(req.get("tools"))


def op_tool_choice(req):
    return live._tool_choice(req.get("tool_choice"))


def op_response_format(req):
    return live._response_format(req.get("response_format"))


def op_image(req):
    item = req.get("item")
    return live._image_content(item) if isinstance(item, dict) else None


def op_content_blocks(req):
    return live._content_blocks(req.get("content"))


def op_function_result(req):
    names = req.get("names")
    names = dict(names) if isinstance(names, dict) else {}
    message = req.get("message")
    return live._function_result(message if isinstance(message, dict) else {}, names)


def op_call_arguments(req):
    tc = req.get("tool_call")
    return live._call_arguments(tc if isinstance(tc, dict) else {})


OPS = {
    "input_steps": op_input_steps,
    "request": op_request,
    "continuation": op_continuation,
    "tools": op_tools,
    "tool_choice": op_tool_choice,
    "response_format": op_response_format,
    "image": op_image,
    "content_blocks": op_content_blocks,
    "function_result": op_function_result,
    "call_arguments": op_call_arguments,
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
