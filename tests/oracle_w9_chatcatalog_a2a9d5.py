#!/usr/bin/env python3
"""Differential oracle for hermes_cli/chat_catalog.py (w9_chatcatalog_a2a9d5)."""
import importlib.util
import json
import sys
import types

G = "w9_chatcatalog_a2a9d5"

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
# Parent-package bootstrap (read-only quarry).
if "hermes_cli" not in sys.modules:
    _hc = types.ModuleType("hermes_cli")
    _hc.__path__ = [Q + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hc

SRC = Q + "/hermes_cli/chat_catalog.py"
spec = importlib.util.spec_from_file_location("chatcat_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["chatcat_live"] = live
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
    try:
        if func == "model_id_is_generation":
            py = live.model_id_is_generation(r.get("model_id"))
        elif func == "tag_list_is_generation":
            py = live._tag_list_is_generation(r.get("tags"))
        elif func == "output_modalities_are_generation":
            py = live._output_modalities_are_generation(r.get("outputs"))
        elif func == "catalog_item_is_generation":
            py = live.catalog_item_is_generation(r.get("item"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if bool(py) != bool(got):
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
