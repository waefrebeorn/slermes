#!/usr/bin/env python3
"""Differential oracle for providers/base.py (gate w5_base_c4cc9e).

ProviderProfile is a stdlib-only declarative dataclass, so the live module
imports directly. Only its pure default hooks are exercised.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/providers/base.py"
spec = importlib.util.spec_from_file_location("providersbase_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["providersbase_live"] = live
spec.loader.exec_module(live)


def profile(**kw):
    kw.setdefault("name", "p")
    return live.ProviderProfile(**kw)


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
        if func == "hostname":
            py = profile(hostname=r.get("hostname", ""), base_url=r.get("base_url", "")).get_hostname()
        elif func == "max_tokens":
            v = r.get("default_max_tokens")
            py = profile(default_max_tokens=(None if v is None else int(v))).get_max_tokens(None)
        elif func == "api_kwargs":
            extra, top = profile().build_api_kwargs_extras()
            py = {"extra_body": extra, "top_level": top}
        elif func == "prepare_messages":
            py = profile().prepare_messages(r.get("messages"))
        elif func == "reasoning_efforts":
            py = profile().supported_reasoning_efforts(None)
        elif func == "vision_model":
            py = profile().default_vision_model()
        elif func == "aux_model":
            py = profile().resolve_aux_model()
        elif func == "extra_body":
            py = profile().build_extra_body()
        elif func == "reasoning_config":
            py = profile().default_reasoning_config(None)
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

print(f"ORACLE w5_base_c4cc9e: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
