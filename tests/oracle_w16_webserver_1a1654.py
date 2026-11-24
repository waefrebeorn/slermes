#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_memory.py (w16_webserver_1a1654).

normalize_schema's provider.get_config_schema() is the injected seam: a fake
provider hands the fixture's raw_schema to the live normalizer.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/web_server_memory.py"
spec = importlib.util.spec_from_file_location("w16_webserver_1a1654_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class FakeProvider:
    def __init__(self, raw_schema):
        self.raw_schema = raw_schema

    def get_config_schema(self):
        return self.raw_schema


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
    if func not in ("strlist", "kind", "coerce", "normschema"):
        continue
    cases += 1
    try:
        if func == "strlist":
            py = live._string_list(r.get("value"))
        elif func == "kind":
            py = live._schema_field_kind(r.get("raw", {}), r.get("choices", []))
        elif func == "coerce":
            try:
                py = live._coerce_bool(r.get("value"), default=r.get("default", False))
            except ValueError as exc:
                py = {"error": str(exc)}
        else:
            py = live._normalize_memory_provider_schema("x", FakeProvider(r.get("raw_schema", [])))
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w16_webserver_1a1654: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
