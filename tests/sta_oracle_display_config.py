#!/usr/bin/env python3
"""Oracle: gateway/display_config.py (_normalise / resolve_display_setting)
vs LIVE Python. Reads the same self-describing fixture JSON from argv[1].

Both sides emit canonical JSON: json.dumps(value, ensure_ascii=False).
"""
import json
import sys
import importlib.util

SPEC = "/home/wubu/hermes-agent-dev/gateway/display_config.py"


def load_module():
    spec = importlib.util.spec_from_file_location("display_config", SPEC)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    path = sys.argv[1]
    with open(path, "r", encoding="utf-8") as f:
        fx = json.load(f)

    mod = load_module()

    if fx.get("mode") == "normalise":
        value = fx.get("value")
        result = mod._normalise(fx.get("setting"), value)
    elif fx.get("mode") == "resolve":
        result = mod.resolve_display_setting(
            fx.get("user_config") or {},
            fx.get("platform_key"),
            fx.get("setting"),
            fx.get("fallback"),
        )
    else:
        sys.stderr.write("unknown mode %r\n" % fx.get("mode"))
        return 1

    sys.stdout.write(json.dumps(result, ensure_ascii=False))
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
