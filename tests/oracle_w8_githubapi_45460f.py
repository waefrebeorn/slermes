#!/usr/bin/env python3
"""Differential oracle for hermes_cli/github_api.py (w8_githubapi_45460f gate)."""
import importlib.util
import json
import sys
import types
import urllib.error

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/github_api.py"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

spec = importlib.util.spec_from_file_location("githubapi_w8_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["githubapi_w8_live"] = live
spec.loader.exec_module(live)


def make_exc(spec):
    kind = spec.get("kind", "other")
    if kind == "http":
        return urllib.error.HTTPError(
            "http://api.github.com/x", spec.get("status", 0), "msg",
            dict(spec.get("headers") or {}), None)
    if kind == "url":
        return urllib.error.URLError(spec.get("reason", ""))
    if kind == "url_timeout":
        return urllib.error.URLError(TimeoutError())
    if kind == "timeout":
        return TimeoutError()
    return Exception(spec.get("message", ""))


def compute(r):
    f = r["func"]
    if f == "token":
        return live.github_token_from_env(r.get("env") or {})
    if f == "header_int":
        return live._header_int(r.get("headers") or {}, r.get("name", ""))
    if f == "describe":
        spec = r.get("spec") or {}
        exc = make_exc(spec)
        return live.describe_github_failure(
            exc, bool(spec.get("authenticated", False)), now=spec.get("now"))
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
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE githubapi: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH githubapi {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE githubapi: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
