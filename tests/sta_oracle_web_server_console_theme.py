#!/usr/bin/env python3
"""sta_oracle_web_server_console_theme.py — LIVE-Python oracle for the
console frame protocol + theme cluster. Extracts real sources from
web_server.py and execs them; the console dispatch loop is replayed through
a faithful synchronous reimplementation of the SAME control flow the
endpoint runs (documented inline against the source lines)."""
import ast
import asyncio
import base64
import json
import sys
from pathlib import Path

WEB_SERVER = Path("/home/wubu/hermes-agent-dev/hermes_cli/web_server.py")

WANT_FUNCS = {
    "_console_json_payload", "_console_send", "_console_send_result",
    "_parse_theme_layer", "_normalise_theme_definition",
    "_discover_user_themes",
}
WANT_CONSTS = {
    "_CONSOLE_PROMPT", "_BUILTIN_DASHBOARD_THEMES",
    "_THEME_DEFAULT_TYPOGRAPHY", "_THEME_DEFAULT_LAYOUT",
    "_THEME_OVERRIDE_KEYS", "_THEME_NAMED_ASSET_KEYS",
    "_THEME_COMPONENT_BUCKETS", "_THEME_LAYOUT_VARIANTS",
    "_THEME_CUSTOM_CSS_MAX", "_FONT_CHOICES", "_FONT_DEFAULT_ID",
}


class FakeWS:
    """Collects frames sent via _console_send."""
    def __init__(self):
        self.frames = []

    async def send_json(self, payload):
        self.frames.append(payload)


class FakeLock:
    async def __aenter__(self):
        return self

    async def __aexit__(self, *a):
        return False


class Result:
    def __init__(self, d):
        self.status = d.get("status", "ok")
        self.command = d.get("command")
        self.output = d.get("output")
        self.confirmation_message = d.get("confirm_msg")


def load_ns(home=None):
    src = WEB_SERVER.read_text(encoding="utf-8")
    tree = ast.parse(src)
    pieces = []
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANT_FUNCS:
            pieces.append(ast.get_source_segment(src, node))
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            for t in targets:
                if isinstance(t, ast.Name) and t.id in WANT_CONSTS:
                    pieces.append(ast.get_source_segment(src, node))

    import os
    import typing
    import yaml as yaml_mod

    class _HomeStub:
        def __init__(self, p):
            self._p = Path(p)

        def __truediv__(self, other):
            return self._p / other

    ns = {
        "json": json, "yaml": yaml_mod, "os": os,
        "Optional": typing.Optional, "Dict": typing.Dict, "Any": typing.Any,
        "get_process_hermes_home": lambda: _HomeStub(home or "/nonexistent"),
        "frozenset": frozenset,
    }
    exec("from __future__ import annotations\n" + "\n\n".join(pieces), ns)
    missing = (WANT_FUNCS | WANT_CONSTS) - set(ns)
    if missing:
        raise SystemExit(f"oracle failed to extract: {missing}")
    return ns


def dump(o):
    print(json.dumps(o, separators=(",", ":"), ensure_ascii=False))


def replay_frames(ns, fx):
    """Faithful synchronous replay of console_ws frame dispatch
    (web_server.py console_ws body) with a synchronous run_command:
    exec inline, same generation/pending/busy rules."""
    prompt = ns["_CONSOLE_PROMPT"]
    exec_table = fx.get("exec", {})
    pending = None
    generation = 0
    busy = False
    trace = []

    def send_result_frames(result, command_id):
        ws = FakeWS()
        asyncio.get_event_loop().run_until_complete(
            ns["_console_send_result"](ws, FakeLock(), result,
                                       command_id=command_id))
        return ws.frames

    for step in fx["script"]:
        if "_busy" in step:
            busy = step["_busy"]
            continue
        frames = []
        close = False
        payload = step
        frame_type = str(payload.get("type") or "").strip().lower()

        if frame_type == "ping":
            frames.append({"type": "pong", "prompt": prompt})
        elif frame_type == "cancel":
            if busy:
                generation += 1
                busy = False
                pending = None
                frames.append({"type": "complete", "status": "cancelled",
                               "prompt": prompt})
            elif pending:
                pending = None
                frames.append({"type": "complete", "status": "cancelled",
                               "prompt": prompt})
            else:
                frames.append({"type": "complete", "status": "idle",
                               "prompt": prompt})
        elif busy:
            frames.append({"type": "error",
                           "message": "A console command is already running.",
                           "prompt": prompt})
        elif frame_type == "confirm":
            command = str(payload.get("command") or pending or "").strip()
            if not pending:
                frames.append({"type": "error",
                               "message": "No command is waiting for confirmation.",
                               "prompt": prompt})
            elif command != pending:
                frames.append({"type": "error",
                               "message": "Confirmation does not match the pending command.",
                               "prompt": prompt})
            else:
                pending = None
                generation += 1
                cid = generation
                spec = exec_table.get(command)
                if spec is None or not spec.get("ok", True):
                    pending = None
                    msg = (spec or {}).get("output") or "no such command"
                    if spec is not None and not spec.get("ok", True):
                        msg = spec.get("output") or "Exception"
                    frames.append({"type": "error", "id": cid, "message": msg,
                                   "command": command})
                    frames.append({"type": "complete", "id": cid,
                                   "status": "error", "command": command,
                                   "prompt": prompt})
                else:
                    result = Result({**spec, "command": spec.get("command", command)})
                    pending = result.command if result.status == "confirm_required" else None
                    frames.extend(send_result_frames(result, cid))
                    if result.status == "exit":
                        close = True
        elif frame_type in {"input", "command"}:
            line = str(payload.get("line") or payload.get("command") or "").strip()
            if not line:
                frames.append({"type": "complete", "status": "ok",
                               "prompt": prompt})
            elif pending:
                frames.append({"type": "error",
                               "message": ("Confirm or cancel the pending command "
                                           "before running another one."),
                               "prompt": prompt})
            else:
                generation += 1
                cid = generation
                spec = exec_table.get(line)
                if spec is None or not spec.get("ok", True):
                    pending = None
                    msg = "no such command" if spec is None else (
                        spec.get("output") or "Exception")
                    frames.append({"type": "error", "id": cid, "message": msg,
                                   "command": line})
                    frames.append({"type": "complete", "id": cid,
                                   "status": "error", "command": line,
                                   "prompt": prompt})
                else:
                    result = Result({**spec, "command": spec.get("command", line)})
                    pending = result.command if result.status == "confirm_required" else None
                    frames.extend(send_result_frames(result, cid))
                    if result.status == "exit":
                        close = True
        else:
            frames.append({"type": "error",
                           "message": f"Unsupported console frame: {frame_type or '?'}",
                           "prompt": prompt})
        trace.append({"frames": frames, "close": close})
    return trace


def main():
    fx = json.loads(Path(sys.argv[1]).read_text())
    op = fx["op"]
    ns = load_ns(home=fx.get("home"))

    if op == "json_payload":
        msg = {}
        if "bytes_b64" in fx:
            msg = {"bytes": base64.b64decode(fx["bytes_b64"])}
        elif "text" in fx and fx["text"] is not None:
            msg = {"text": fx["text"]}
        payload, error = ns["_console_json_payload"](msg)
        dump({"payload": payload, "error": error})
    elif op == "profile":
        q = fx.get("q")
        profile = (q or "").strip()
        dump({"profile": profile or None})
    elif op == "send_result":
        ws = FakeWS()
        asyncio.new_event_loop().run_until_complete(
            ns["_console_send_result"](ws, FakeLock(), Result(fx),
                                       command_id=fx.get("id", 1)))
        dump(ws.frames)
    elif op == "frames":
        asyncio.set_event_loop(asyncio.new_event_loop())
        dump(replay_frames(ns, fx))
    elif op == "parse_layer":
        r = ns["_parse_theme_layer"](fx.get("value"), fx.get("hex", "#000"),
                                     fx.get("alpha", 1.0))
        dump(r)
    elif op == "normalise":
        dump(ns["_normalise_theme_definition"](fx.get("data")))
    elif op == "builtins":
        dump(ns["_BUILTIN_DASHBOARD_THEMES"])
    elif op == "discover":
        dump(ns["_discover_user_themes"]())
    elif op == "themes_response":
        active = fx.get("active") or "default"
        user_themes = ns["_discover_user_themes"]()
        seen = set()
        themes = []
        for t in ns["_BUILTIN_DASHBOARD_THEMES"]:
            seen.add(t["name"])
            themes.append(t)
        for t in user_themes:
            if t["name"] in seen:
                continue
            themes.append({"name": t["name"], "label": t["label"],
                           "description": t["description"], "definition": t})
            seen.add(t["name"])
        dump({"themes": themes, "active": active})
    elif op == "bootstrap_css":
        active = fx.get("active")
        if not active or any(b["name"] == active
                             for b in ns["_BUILTIN_DASHBOARD_THEMES"]):
            dump({"css": ""})
            return
        css = ""
        for theme in ns["_discover_user_themes"]():
            if theme.get("name") != active:
                continue
            palette = theme.get("palette") or {}
            bg = palette.get("background") or {}
            mg = palette.get("midground") or {}
            bg_hex = bg.get("hex", "#0a0a0a") if isinstance(bg, dict) else "#0a0a0a"
            mg_hex = mg.get("hex", "#e5e5e5") if isinstance(mg, dict) else "#e5e5e5"
            typo = theme.get("typography") or {}
            font_sans = typo.get("fontSans") or ns["_THEME_DEFAULT_TYPOGRAPHY"]["fontSans"]
            base_size = typo.get("baseSize") or ns["_THEME_DEFAULT_TYPOGRAPHY"]["baseSize"]

            def _esc(s):
                return str(s).replace("</", "<\\/")
            css = (
                '<style id="hermes-theme-bootstrap">'
                ":root{"
                f"--background-base:{_esc(bg_hex)};"
                f"--midground-base:{_esc(mg_hex)};"
                f"--theme-font-sans:{_esc(font_sans)};"
                f"--theme-base-size:{_esc(base_size)};"
                "}"
                "html,body{background-color:var(--background-base);"
                "color:var(--midground-base);"
                "font-family:var(--theme-font-sans);"
                "font-size:var(--theme-base-size);}"
                "</style>"
            )
            break
        dump({"css": css})
    elif op == "font_allowed":
        fid = fx.get("id")
        allowed = bool(fid) and (fid == ns["_FONT_DEFAULT_ID"]
                                 or fid in ns["_FONT_CHOICES"])
        dump({"allowed": allowed})
    else:
        raise SystemExit(f"unknown op {op}")


if __name__ == "__main__":
    main()
