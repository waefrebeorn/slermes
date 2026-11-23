#!/usr/bin/env python3
"""Differential oracle for utils/__init__.py (gate w5_init_7d1849).

The namespace re-exports pure helpers from utils.env / utils.files / utils.urls;
only the deterministic ones are exercised. env reads use an injected value via
utils.env.os so no process environment is mutated (Windows CRT unsets on "").
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/utils/__init__.py"
spec = importlib.util.spec_from_file_location("utils_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["utils_live"] = live
spec.loader.exec_module(live)

uenv = sys.modules["utils.env"]

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
    try:
        if func == "truthy":
            py = live.is_truthy_value(r.get("value"), bool(r.get("default", False)))
        elif func == "hostname":
            py = live.base_url_hostname(r.get("url", ""))
        elif func == "path":
            py = live.base_url_path(r.get("url", ""))
        elif func == "origin":
            py = list(live.base_url_origin(r.get("url", "")))
        elif func == "same":
            py = live.same_base_url_origin(r.get("a", ""), r.get("b", ""))
        elif func == "matches":
            py = live.base_url_host_matches(r.get("url", ""), r.get("domain", ""))
        elif func == "proxy":
            py = live.normalize_proxy_url(r.get("url"))
        elif func == "env_int":
            raw = r.get("raw", "")
            uenv.os = types.SimpleNamespace(getenv=lambda k, d="": raw)
            py = live.env_int("K", int(r.get("default", "0")))
        elif func == "env_float":
            raw = r.get("raw", "")
            uenv.os = types.SimpleNamespace(getenv=lambda k, d="": raw)
            py = live.env_float("K", float(r.get("default", "0")))
        elif func == "noproxy":
            py = live.sanitize_no_proxy_entries(r.get("value", ""))
        elif func == "all":
            py = list(live.__all__)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_init_7d1849: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
