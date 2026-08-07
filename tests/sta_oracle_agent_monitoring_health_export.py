"""Oracle for agent/monitoring/gateway_health_export.py pure helpers."""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT")
if DEV_ROOT and DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from agent.monitoring.gateway_health_export import _resolve_headers


def run(c):
    op = c.get("op", "")
    if op == "he_resolve_headers":
        # Set env vars from fixture
        env = c.get("env", {})
        for name, val in env.items():
            os.environ[name] = val
        try:
            result = _resolve_headers(c.get("value", {}))
        finally:
            for name in env:
                os.environ.pop(name, None)
        return json.dumps(result, sort_keys=True)
    return None


def main():
    fixture = sys.argv[1]
    cases = json.load(open(fixture))
    for c in cases:
        r = run(c)
        if r is None:
            print("none")
        else:
            print(r)


if __name__ == "__main__":
    main()
