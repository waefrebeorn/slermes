#!/usr/bin/env python3
"""Differential oracle for upstream Hermes utils/env.py (gate w5_env_ee14b1).

All ported functions are pure data. Env-reading helpers get their value
injected from the fixture into a throwaway os.environ key so the live module
exercises the same code path without depending on the process environment.
"""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/utils/env.py"
spec = importlib.util.spec_from_file_location("w5_env_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_env_live"] = live
spec.loader.exec_module(live)

KEY = "W5_ENV_EE14B1_INJECTED"
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
    got = r.get("out")
    args = {k: v for k, v in r.items() if k not in ("func", "out")}
    try:
        if func == "truthy":
            py = live.is_truthy_value(args.get("value"), args.get("default", False))
        elif func == "proxy":
            py = live.normalize_proxy_url(args.get("proxy_url"))
        elif func == "ipv6":
            py = live._bare_ipv6_literal(args.get("entry"))
        elif func == "sanitize":
            py = live.sanitize_no_proxy_entries(args.get("no_proxy_value"))
        elif func == "int":
            raw = args.get("raw")
            if raw is None:
                os.environ.pop(KEY, None)
            else:
                os.environ[KEY] = raw
            py = live.env_int(KEY, args.get("default", 0))
        elif func == "float":
            raw = args.get("raw")
            if raw is None:
                os.environ.pop(KEY, None)
            else:
                os.environ[KEY] = raw
            py = live.env_float(KEY, args.get("default", 0.0))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR func={func} args={args}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH func={func} args={args} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_env_ee14b1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
