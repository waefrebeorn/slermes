#!/usr/bin/env python3
"""Differential oracle for plugins/web/openai_native/provider.py
(w20_provider_8409e9).

The live module is imported against the real ``plugins.web._common``; only
``hermes_constants.get_hermes_home`` is redirected to a scratch directory, where
each case writes the fixture's auth.json body. Compared values: the credential
probe, the unsupported-search refusal, the picker schema and the identity.
"""
import importlib.util
import json
import os
import shutil
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

HOME = {"path": Path(tempfile.mkdtemp(prefix="on8409_"))}
sys.modules["hermes_constants"] = types.SimpleNamespace(
    get_hermes_home=lambda: HOME["path"])

NAME = "w20_provider_8409e9_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "web", "openai_native", "provider.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def expected(r):
    f = r.get("func")
    if f == "has_credentials":
        auth = HOME["path"] / "auth.json"
        if auth.exists():
            auth.unlink()
        if r.get("file_exists", True):
            auth.write_text(r.get("auth_text", ""), encoding="utf-8")
        return bool(live.has_codex_credentials())
    provider = live.OpenAINativeWebSearchProvider()
    if f == "search_unsupported":
        return provider.search("anything")
    if f == "setup_schema":
        return provider.get_setup_schema()
    if f == "identity":
        return {"name": provider.NAME, "display_name": provider.DISPLAY_NAME,
                "extract": provider.EXTRACT}
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
try:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") is None:
            continue
        cases += 1
        try:
            exp = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if exp != got:
            mism += 1
            print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
                  file=sys.stderr)
finally:
    shutil.rmtree(HOME["path"], ignore_errors=True)
print("ORACLE w20_provider_8409e9: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
