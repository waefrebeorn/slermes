"""Oracle for agent/monitoring/gateway_health_export.py pure helpers."""
import json, sys

DEV_ROOT = "/home/wubu/hermes-agent-dev"
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from agent.monitoring import gateway_health_export as ghe_mod

cases_file = sys.argv[1] if len(sys.argv) > 1 else "cases.in"
with open(cases_file) as f:
    cases = json.load(f)

for c in cases:
    op = c.get("op", "")
    if op == "_redact_string":
        raw = c.get("raw", "")
        limit = c.get("limit", 500)
        print(json.dumps(ghe_mod._redact_string(raw, limit=limit)))
    elif op == "_safe_resource_attributes":
        raw_json = c.get("raw_json", "{}")
        try:
            raw = json.loads(raw_json)
        except Exception:
            raw = {}
        print(json.dumps(ghe_mod._safe_resource_attributes(raw)))
    elif op == "_version":
        print(json.dumps(ghe_mod._version()))
    elif op == "_profile":
        print(json.dumps(ghe_mod._profile()))
    elif op == "_supervision_mode":
        print(json.dumps(ghe_mod._supervision_mode()))
    elif op == "_install_id":
        config = c.get("config", {})
        print(json.dumps(ghe_mod._install_id(config)))
    elif op == "_runtime_resource_attributes":
        config = c.get("config", {})
        scope = c.get("telemetry_scope", "hermes.gateway.diagnostics")
        print(json.dumps(ghe_mod._runtime_resource_attributes(config, telemetry_scope=scope)))
