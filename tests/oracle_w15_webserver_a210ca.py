#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_oauth.py (w15_webserver_a210ca)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/web_server_oauth.py"
spec = importlib.util.spec_from_file_location("w15_webserver_a210ca_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func not in ("truncate", "profile", "token_status"):
        continue
    cases += 1
    try:
        if func == "truncate":
            value = (lambda: "unused") if r.get("callable") else r.get("value", "")
            py = live._truncate_token(value, int(r.get("visible", 6)))
        elif func == "profile":
            py = live._oauth_profile_name(r.get("profile"))
        else:
            creds = dict(r.get("creds", {}) or {})
            if r.get("access_token_callable"):
                creds["accessToken"] = lambda: "unused"
            py = live._token_status(r.get("source", ""), r.get("label", ""), creds)
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w15_webserver_a210ca: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
