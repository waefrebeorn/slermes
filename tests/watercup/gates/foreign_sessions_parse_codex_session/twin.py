"""twin for foreign_sessions_parse_codex_session.

Mirrors LIVE hermes_cli/foreign_sessions.py::parse_codex_session over an
in-memory JSONL string (the live fn reads a Path; this twin feeds the same
line loop directly and reuses the module's own helpers), tracing the same
result-shape rungs as the C harness (which compiles the REAL port).
"""
import os, sys, json as _json
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_foreign_sessions", os.path.join(DEV, "hermes_cli", "foreign_sessions.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_foreign_sessions"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("parse_codex_session")
        turns = []
        cwd = None
        session_id = None
        for line in raw.split("\n"):
            line = line.strip()
            if not line:
                continue
            try:
                obj = _json.loads(line)
            except Exception:
                continue
            if not isinstance(obj, dict):
                continue
            otype = obj.get("type")
            payload = obj.get("payload")
            if not isinstance(payload, dict):
                continue
            if otype == "session_meta":
                if isinstance(payload.get("cwd"), str):
                    cwd = payload["cwd"]
                sid = payload.get("session_id") or payload.get("id")
                if isinstance(sid, str):
                    session_id = sid
                continue
            if otype != "response_item":
                continue
            ptype = payload.get("type")
            if ptype == "message":
                role = payload.get("role")
                if role not in ("user", "assistant"):
                    continue
                text = mod._flatten_blocks(payload.get("content"), source="codex")
                if not text or (role == "user" and mod._is_wrapper_text(text)):
                    continue
                turns.append((role, text))
            elif ptype in ("custom_tool_call", "function_call", "local_shell_call"):
                name = payload.get("name") or payload.get("tool") or "tool"
                turns.append(("assistant", "[ran tool: %s]" % name))
        merged = mod._merge_turns(turns)
        c.branch(len(merged) > 0)
        for entry in merged:
            c.loop()
            c.valstr("%s|%s" % (entry["role"][:60], entry["content"][:900]))
        c.valstr(cwd or "")
        c.valstr(mod._first_user_line(turns) or "")
        c.valstr(session_id or "")
        cups.append(c.finalize())
    return cups
