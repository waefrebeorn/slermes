#!/usr/bin/env python3
"""Differential oracle for _openrouter_exchange_code in
hermes_cli/auth_openrouter.py (gate w13_authopenro_0d3257).

The live httpx.post is stubbed with the response described by each case; the
LIVE function's status/error/JSON branching then runs unchanged.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/auth_openrouter.py"
spec = importlib.util.spec_from_file_location("w13openro_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)


class Resp:
    def __init__(self, status, text, payload, json_error):
        self.status_code = status
        self.text = text
        self._payload = payload
        self._json_error = json_error

    def json(self):
        if self._json_error:
            raise ValueError("not json")
        return self._payload


def replay(r):
    status = int(r.get("status", 200))
    text = r.get("text", "") or ""
    payload = r.get("payload")
    json_error = bool(r.get("json_error", False))
    post_error = r.get("post_error", "") or ""

    def fake_post(url, json=None, headers=None, timeout=None):
        # Confirm the LIVE function passes every argument it is supposed to.
        assert url == live.OPENROUTER_AUTH_KEYS_URL, url
        assert json == {"code": r.get("code", "CODE"),
                        "code_verifier": r.get("verifier", "VER"),
                        "code_challenge_method": "S256"}, json
        assert headers == {"Content-Type": "application/json"}, headers
        assert timeout == 20.0, timeout
        if post_error:
            raise RuntimeError(post_error)
        return Resp(status, text, payload, json_error)

    live.httpx = types.SimpleNamespace(post=fake_post)
    try:
        key = live._openrouter_exchange_code(r.get("code", "CODE"),
                                             r.get("verifier", "VER"))
        return {"ok": True, "key": key, "code": None, "message": None,
                "provider": None, "relogin": False}
    except Exception as e:
        return {"ok": False, "key": None, "code": getattr(e, "code", None),
                "message": str(e), "provider": getattr(e, "provider", None),
                "relogin": getattr(e, "relogin_required", None)}


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "exchange_code":
        continue
    want = replay(r)
    got = r.get("out")
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_authopenro_0d3257: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
