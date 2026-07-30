#!/usr/bin/env python3
"""sta_oracle_toolsets.py — live-Python oracle for the toolsets +
platform-tools stack. Reads fixture lines:
    resolve <name>
    validate <name>
    bundle <name>
    platform <label> <platform> <config-json>
Emits one JSON line per case: {"case": ..., "out": [...]}, matching
tests/t_port_toolsets.c.
"""
import json
import os
import sys

sys.path.insert(0, os.environ.get(
    "HERMES_AGENT_DIR", os.path.expanduser("~/.hermes/hermes-agent")))
os.environ.pop("HASS_TOKEN", None)
os.environ.pop("XAI_API_KEY", None)

from toolsets import resolve_toolset, validate_toolset, bundle_non_core_tools  # noqa: E402
import hermes_cli.tools_config as tc  # noqa: E402

# Deterministic: no live credentials / plugin registry in the oracle contract.
tc._xai_credentials_present = lambda: False
tc._get_plugin_toolset_keys = lambda: set()


def main() -> int:
    path = sys.argv[1]
    out = []
    with open(path, "r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            if not line.strip():
                continue
            kind, rest = line.split(" ", 1)
            if kind == "resolve":
                out.append({"case": line,
                            "out": resolve_toolset(rest, include_registry=False)})
            elif kind == "validate":
                out.append({"case": line, "out": validate_toolset(rest)})
            elif kind == "bundle":
                out.append({"case": line,
                            "out": sorted(bundle_non_core_tools(rest))})
            elif kind == "platform":
                label, platform, cfg_json = rest.split(" ", 2)
                cfg = json.loads(cfg_json)
                out.append({"case": f"platform {label}",
                            "out": sorted(tc._get_platform_tools(cfg, platform))})
            else:
                raise SystemExit(f"unknown case kind: {kind}")
    for entry in out:
        print(json.dumps(entry, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
