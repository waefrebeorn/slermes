#!/usr/bin/env python3
"""Assert every tool the MCP server publishes carries a usable JSON-Schema object.

MCP requires ``inputSchema`` to be an object. The registry stores schemas as C string literals, so a
typo in one of those literals (a stray quote, a trailing comma inside the string) used to travel all
the way to the client as a *string* that could not be parsed -- invisible until somebody tried to
consume it. This check is the tripwire: it fails on a schema that is not an object, and on one that
parses but declares no properties.

Usage: start the server (`slermes.exe mcp-serve`), then
    python scripts/check_tool_schemas.py [--url http://127.0.0.1:9100/mcp]
Exit 0 = every schema good; 1 = problems (listed on stdout).
"""
from __future__ import annotations

import argparse
import json
import urllib.request


def fetch(url: str) -> dict:
    body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": "tools/list"}).encode()
    req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as resp:   # noqa: S310 - local server
        raw = resp.read().decode("utf-8", "replace")
    return json.loads(raw)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:9100/mcp")
    args = ap.parse_args()

    payload = fetch(args.url)
    tools = payload["result"]["tools"]
    problems: list[str] = []

    for tool in tools:
        name = tool.get("name") or "<unnamed>"
        schema = tool.get("inputSchema")
        if not isinstance(schema, dict):
            problems.append(f"{name}: inputSchema is {type(schema).__name__}, not an object")
            continue
        if schema.get("type") != "object":
            problems.append(f"{name}: inputSchema type is {schema.get('type')!r}, expected 'object'")
        if "x-schema-error" in schema:
            problems.append(f"{name}: registry schema literal does not parse as JSON")
        elif "properties" not in schema:
            problems.append(f"{name}: inputSchema has no properties")
        if not tool.get("description"):
            problems.append(f"{name}: no description")

    print(f"tool schemas: {len(tools)} published, {len(problems)} problem(s)")
    for p in problems:
        print(f"  {p}")
    return 0 if not problems else 1


if __name__ == "__main__":
    raise SystemExit(main())
