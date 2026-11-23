#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_content.py (gate w13_pluginscon_bbba05)."""
import importlib.util
import json
import re
import sys
import tempfile
import types
from pathlib import Path

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
cli = types.ModuleType("hermes_cli")
cli.__path__ = [QUARRY + "/hermes_cli"]
sys.modules.setdefault("hermes_cli", cli)


def _stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("hermes_cli.plugins_loader", _serialized_replacement=lambda f: f)
_stub("hermes_cli.plugins_ledger", PluginRegistration=dict)
_stub("agent", __path__=[])
_stub("agent.skill_utils", _NAMESPACE_RE=re.compile(r"^[a-zA-Z0-9_-]+$"))


def _slug(key):
    slug = "".join(ch if ch.isascii() and (ch.isalnum() or ch in "_-") else "-" for ch in key.lower())
    return slug.strip("-_") or "plugin"


_stub("hermes_cli.plugins_manifest", _portable_slug=_slug)

SRC = QUARRY + "/hermes_cli/plugins_content.py"
NAME = "plugins_content_live_w13_pluginscon"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


class FakeManager:
    def __init__(self, skills):
        self._plugin_skills = skills
        self._automation_blueprints = {}


class FakeManifest:
    def __init__(self, name, portable=False):
        self.name = name
        self.portable = portable


class FakeSelf:
    def __init__(self, name, portable, skills):
        self.manifest = FakeManifest(name, portable)
        self._manager = FakeManager(skills)
        self.plugin_id = "test-plugin"

    def _register_entry(self, kind, qualified, registry, entry, fmt, *args):
        return "registered"


def compute(r):
    func = r.get("func")
    if func == "plugin_skill_owners":
        skills = r.get("skills", {})
        owners = live.plugin_skill_owners(FakeManager(skills), r.get("qualified", ""))
        return sorted(owners)
    if func == "skill_name_valid":
        name = r.get("name", "")
        pname = r.get("plugin_name", "demo")
        ctx = FakeSelf(pname, False, {})
        with tempfile.NamedTemporaryFile(suffix=".md", delete=False) as tf:
            path = Path(tf.name)
        try:
            try:
                live.register_skill(ctx, name, path)
                return {"ok": True, "value": "registered"}
            except ValueError as exc:
                return {"ok": False, "error": str(exc)}
        finally:
            try:
                path.unlink()
            except OSError:
                pass
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
print("ORACLE w13_pluginscon_bbba05: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
