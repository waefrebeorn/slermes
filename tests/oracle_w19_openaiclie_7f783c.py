#!/usr/bin/env python3
"""Differential oracle for plugins/google_meet/realtime/openai_client.py (gate w19_openaiclie_7f783c).

``_decode_audio`` and the ``RealtimeSpeaker`` class body are extracted from the live
AST; the queue methods run against real temp files, so the JSONL wire form compared
here is CPython's own ``json.dumps``.  ``uuid.uuid4`` is injected so the generated id
is deterministic.  The WebSocket session methods are live network wrappers and are
not exercised.
"""
import ast, base64, contextlib, json, sys, tempfile, threading, time, types, __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "plugins" / "google_meet" / "realtime" / "openai_client.py"
GATE = "w19_openaiclie_7f783c"
_ANN = __future__.annotations.compiler_flag
TMP = Path(tempfile.mkdtemp(prefix="w19_openaiclie_"))


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "openai_client_live", "json": json, "contextlib": contextlib,
         "time": time, "threading": threading, "base64": base64, "Path": Path,
         "Any": object, "Callable": object, "Optional": object,
         "REALTIME_URL": "wss://api.openai.com/v1/realtime",
         "_TERMINAL_FRAMES": {"response.done", "response.completed", "response.cancelled"},
         "say_queue_lock": lambda path: contextlib.nullcontext(),
         "uuid": types.SimpleNamespace(uuid4=lambda: g_uuid[0])}
    wanted = {"_decode_audio", "RealtimeSpeaker"}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


g_uuid = [""]
G = extract()
Speaker = G["RealtimeSpeaker"]


def run_case(r):
    f = r.get("func")
    if f == "realtime_url":
        return G["REALTIME_URL"]
    if f == "decode_audio":
        return {"hex": G["_decode_audio"](r.get("b64") or "").hex()}
    if f == "read_queue":
        g_uuid[0] = r.get("uuid") or ""
        path = TMP / "read_queue.jsonl"
        if path.exists():
            path.unlink()
        if r.get("exists", True):
            path.write_text(r.get("text") or "", encoding="utf-8")
        sp = Speaker(None, path)
        return sp._read_queue()
    if f == "rewrite_queue":
        path = TMP / "rewrite_queue.jsonl"
        if path.exists():
            path.unlink()
        sp = Speaker(None, path)
        sp._rewrite_queue(r.get("entries") or [])
        return path.read_text(encoding="utf-8")
    if f == "append_processed":
        path = TMP / "processed.jsonl"
        if path.exists():
            path.unlink()
        sp = Speaker(None, TMP / "unused.jsonl", path)
        sp._append_processed(r.get("entry") or {}, r.get("result"))
        return path.read_text(encoding="utf-8") if path.exists() else ""
    return "<unknown>"


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    f = r.get("func")
    if f not in {"realtime_url", "decode_audio", "read_queue", "rewrite_queue", "append_processed"}:
        continue
    cases += 1
    try:
        py = run_case(r)
    except BaseException as exc:
        mism += 1
        print("MISMATCH %s oracle raised %r args=%s" % (f, exc, r), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)

print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
sys.exit(1 if mism else 0)
