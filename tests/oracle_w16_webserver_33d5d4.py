#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_config.py (w16_webserver_33d5d4)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/web_server_config.py"
spec = importlib.util.spec_from_file_location("w16_webserver_33d5d4_live", SRC)
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
    if func not in ("infer", "cmdblock", "normalize", "known"):
        continue
    cases += 1
    try:
        if func == "infer":
            py = live._infer_type(r.get("value"))
        elif func == "cmdblock":
            py = live._is_command_provider_block(r.get("value"))
        elif func == "normalize":
            py = live._normalize_config_for_web(r.get("config", {}))
        else:
            py = live._endpoint_known_to_config(r.get("cfg", {}), r.get("base_url", ""))
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w16_webserver_33d5d4: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
