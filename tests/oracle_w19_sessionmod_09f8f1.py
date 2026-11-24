#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/session_model.py (gate w19_sessionmod_09f8f1)."""
import ast
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "gateway" / "platforms" / "session_model.py"
GATE = "w19_sessionmod_09f8f1"
WANTED = {"_display_base_url", "_resolve_session_model_settings"}
WANT_CLASSES = {"_SessionModelSettingsMixin"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "sm_live", "SessionModelSettings": dict}
    from urllib.parse import urlsplit, urlunsplit
    g.update(dict(urlsplit=urlsplit, urlunsplit=urlunsplit,
                  TYPE_CHECKING=False, TypedDict=dict, contextmanager=lambda f: f,
                  nullcontext=None, copy=lambda x: x, deepcopy=lambda x: x))
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=0, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in WANT_CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=0, dont_inherit=True), g)
    return g


G = extract()


class Channel:
    def __init__(self, model, provider):
        self.model = model
        self.provider = provider


class Store:
    def __init__(self, override):
        self._override = override

    def get_model_override(self, key):
        return self._override


class Runner:
    def __init__(self, key, override, store_override, channel):
        self._key = key
        self._override = override
        self.session_store = Store(store_override) if store_override is not None else None
        self._channel = channel

    def _session_key_for_source(self, source):
        return self._key

    def _session_model_override(self, key):
        return self._override

    def _channel_override_for(self, source):
        return self._channel


class Mixin(G["_SessionModelSettingsMixin"]):
    def __init__(self, settings):
        self._settings = settings
        self.gateway_runner = object()

    def _session_model_settings_context(self, source):
        from contextlib import contextmanager

        @contextmanager
        def ctx():
            yield (self._settings, {})
        return ctx()


def run_case(r):
    f = r.get("func")
    if f == "display_base_url":
        return G["_display_base_url"](r.get("value"))
    if f == "settings_display":
        return dict(Mixin(dict(r.get("settings") or {})).get_session_model_settings(object()))
    if f == "resolve_settings":
        cfg = {"model": r.get("model_cfg")}
        channel = Channel(r.get("channel_model"), r.get("channel_provider")) if r.get("has_channel") else None
        runner = Runner(r.get("key", ""), r.get("override"), r.get("store_override"), channel)
        return G["_resolve_session_model_settings"](runner, object(), cfg)
    return None


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
        f = r.get("func")
        if f not in {"display_base_url", "resolve_settings", "settings_display"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s args=%r py=%r c=%r" % (f, r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
