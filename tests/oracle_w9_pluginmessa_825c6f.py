#!/usr/bin/env python3
"""Differential oracle for gateway/plugin_messages.py (gate w9_pluginmessa_825c6f)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
import gateway  # parent-package bootstrap

# _supports_edit needs the base adapter class; stub it to avoid loading the
# whole platform stack.
base_mod = types.ModuleType("gateway.platforms.base")


class BasePlatformAdapter:
    def edit_message(self, *a, **kw):
        return None


base_mod.BasePlatformAdapter = BasePlatformAdapter
platforms_pkg = types.ModuleType("gateway.platforms")
platforms_pkg.__path__ = [QUARRY + "/gateway/platforms"]
sys.modules.setdefault("gateway.platforms", platforms_pkg)
sys.modules["gateway.platforms.base"] = base_mod

SRC = QUARRY + "/gateway/plugin_messages.py"
NAME = "plugin_messages_live_w9_pluginmessa"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


class FakeAdapter:
    def __init__(self, limit):
        self._limit = limit

    def max_message_length_for_chat(self, chat_id):
        return self._limit

    def message_len_fn_for_chat(self, chat_id):
        return len


def compute(r):
    func = r.get("func")
    if func == "err":
        return live._err(r.get("code", ""), r.get("detail", ""))
    if func == "fit":
        text = r.get("text", "")
        if "limit" in r:
            adapter = FakeAdapter(int(r["limit"]))
        else:
            adapter = object()  # no limit methods -> 4096/len fallback
        return live._fit(adapter, r.get("chat_id", "chat"), text)
    if func == "supports_edit":
        attrs = {}
        if "flag" in r:
            attrs["SUPPORTS_MESSAGE_EDITING"] = r["flag"]
        if r.get("has_edit"):
            def edit_message(self, *a, **kw):
                return None
            attrs["edit_message"] = edit_message
        Fake = type("FakeEditAdapter", (), attrs)
        return bool(live._supports_edit(Fake()))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_pluginmessa_825c6f: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
