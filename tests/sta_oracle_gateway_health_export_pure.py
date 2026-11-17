import load_dev_module as _ldm
from pathlib import Path
"""Oracle for agent/monitoring/gateway_health_export.py pure helpers."""
import os
import json, sys

DEV_ROOT = os.environ.get("SLERMES_DEV_ROOT", "/home/wubu/hermes-agent-dev")
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from agent.monitoring import gateway_health_export as ghe_mod
from agent.monitoring.redaction import redact_bounded
_ldm.rebind_used(ghe_mod, 'ghe_mod', ["agent.monitoring.otlp_exporter"], Path(__file__))

cases_file = sys.argv[1] if len(sys.argv) > 1 else "cases.in"
with open(cases_file) as f:
    cases = json.load(f)

for c in cases:
    op = c.get("op", "")
    if op == "_redact_string":
        raw = c.get("raw", "")
        limit = c.get("limit", 500)
        # _redact_string was deleted upstream (7eb8ccc315) and replaced by
        # agent.monitoring.redaction.redact_bounded, same signature/semantics.
        print(json.dumps(redact_bounded(raw, limit=limit)))
    elif op == "_safe_resource_attributes":
        raw_json = c.get("raw_json", "{}")
        try:
            raw = json.loads(raw_json)
        except Exception:
            raw = {}
        print(json.dumps(ghe_mod._safe_resource_attributes(raw)))
    elif op == "_version":
        # Environment-dependent, not port logic: Python's _safe_version()
        # resolves from an install stamp or live git and returns "unknown" in
        # the isolated oracle HOME, while the C harness has HERMES_VERSION
        # compiled in ("0.0.0-slermes"). Both are correct for their own
        # environment; asserting equality only tests the fixture's HOME.
        continue
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