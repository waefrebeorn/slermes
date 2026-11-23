#!/usr/bin/env python3
"""Differential oracle for acp_adapter/model_catalog.py (gate w11_modelcatal_563c9e)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

# Stub the one heavy top-level import (acp.schema); the three ported pure
# helpers never touch these objects.
acp = types.ModuleType("acp")
acp.__path__ = []
schema = types.ModuleType("acp.schema")
class ModelInfo:
    def __init__(self, **kw): self.__dict__.update(kw)
class SessionModelState:
    def __init__(self, **kw): self.__dict__.update(kw)
schema.ModelInfo = ModelInfo
schema.SessionModelState = SessionModelState
sys.modules["acp"] = acp
sys.modules["acp.schema"] = schema

SRC = QUARRY + "/acp_adapter/model_catalog.py"
spec = importlib.util.spec_from_file_location("w11_modelcatal_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_modelcatal_live"] = live
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
    got = r.get("out")
    try:
        if func == "encode":
            want = live.encode_model_choice(r.get("provider", ""), r.get("model", ""))
        elif func == "semantic":
            want = live._semantic_provider(r.get("provider_id", ""), lambda s: r.get("normalized", ""))
        elif func == "empty_applies":
            norm_map = r.get("norm_map", {}) or {}
            want = live._empty_catalog_applies(
                r.get("provider_id", ""), set(r.get("candidates", []) or []),
                lambda s: norm_map.get(s, s))
        else:
            continue
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_modelcatal_563c9e: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
