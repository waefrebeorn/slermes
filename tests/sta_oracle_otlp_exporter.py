"""Oracle for agent/monitoring/otlp_exporter.py pure helpers."""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT")
if DEV_ROOT and DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from agent.monitoring.otlp_exporter import (
    _otlp_config,
    _resolve_headers,
    is_enabled,
    _span_attrs,
)

cases_file = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__),
    "oracle", "fixtures", "otlp_exporter", "cases.in",
)

with open(cases_file) as f:
    cases = json.load(f)

for c in cases:
    op = c.get("op", "")
    val = c.get("value", "")

    if op == "_otlp_config":
        print(json.dumps(_otlp_config(json.loads(val))))
    elif op == "_resolve_headers":
        print(json.dumps(_resolve_headers(json.loads(val))))
    elif op == "is_enabled":
        print(is_enabled(json.loads(val)))
    elif op == "_span_attrs":
        print(json.dumps(_span_attrs(json.loads(val))))
