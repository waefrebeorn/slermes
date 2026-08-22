#!/usr/bin/env python3
"""sta_oracle_setup_mcp_tool.py — Python twin for tools/setup_mcp_tool.py,
mirrored by src/tools/setup_mcp_tool.c. The real module imports
tools.registry (heavy package graph), so this twin mirrors the dispatcher's
decision tree exactly (loops_pure precedent) with a scripted fake bridge.

Fixture ops:
  {"kind":"setup","server":...,"action":...,"reason":...,
   "cb":"ok_json"|"ok_text"|"none"|"raise"|"nocb"}
  {"kind":"schema"}
"""
import json
import sys

SETUP_MCP_SCHEMA = {
    "name": "setup_mcp",
    "description": (
        "Propose an MCP server to the user as an inline consent card in the "
        "Hermes desktop chat. The card lets them install a catalog entry, "
        "re-enable a disabled server, or run an OAuth login — right there, "
        "without opening the Capabilities tab — and blocks until they act or "
        "decline. Use when the user asks to add/set up an MCP (e.g. \"add the "
        "linear mcp\"), or when a task clearly needs one that is missing or "
        "unauthorized. Never call it twice for the same server after a "
        "decline. Returns JSON {status: installed|enabled|authorized|declined|"
        "unanswered|error, server, detail?, tools?}. On declined/unanswered, "
        "continue without the server. Catalog names: run `hermes mcp catalog` "
        "in the terminal to list them."
    ),
    "parameters": {
        "type": "object",
        "properties": {
            "server": {
                "type": "string",
                "description": (
                    "The server's catalog name (for install) or its name in "
                    "mcp_servers config (for enable/authorize)."
                ),
            },
            "action": {
                "type": "string",
                "enum": ["install", "enable", "authorize"],
                "description": (
                    "install: add a catalog entry (prompts for any required "
                    "keys). enable: re-enable a disabled configured server. "
                    "authorize: run the OAuth browser flow for a configured "
                    "server. Defaults to install."
                ),
            },
            "reason": {
                "type": "string",
                "description": (
                    "One short sentence shown on the card: why this server "
                    "helps right now (e.g. \"To read the JIRA ticket you "
                    "linked\")."
                ),
            },
        },
        "required": ["server"],
    },
}

_ACTIONS = ("install", "enable", "authorize")


def tool_error(message):
    return {"error": message}


def setup_mcp_tool(server, action, reason, cb_mode):
    """Mirror of tools/setup_mcp_tool.py:setup_mcp_tool.
    Returns (result_obj, cb_server, cb_action, cb_reason)."""
    if cb_mode == "nocb":
        return (tool_error(
            "setup_mcp is only available in the Hermes desktop app. Use the "
            "terminal instead: `hermes mcp install <name>` for catalog entries, "
            "`hermes mcp login <name>` for OAuth."), "", "", "")
    name = (server or "").strip()
    if not name:
        return (tool_error(
            "server is required — the catalog or config name of the MCP server."
        ), "", "", "")
    action = (action or "install").strip().lower()
    if action not in _ACTIONS:
        return (tool_error(f"action must be one of {', '.join(_ACTIONS)}."),
                "", "", "")
    reason_t = (reason or "").strip()
    if cb_mode == "raise":
        return (tool_error("MCP setup flow failed: callback raised"),
                name, action, reason_t)
    if cb_mode == "none":
        return ({
            "status": "unanswered",
            "server": name,
            "note": (
                "The user did not respond to the setup card. Do not retry "
                "immediately; continue without the server or ask in chat."
            ),
        }, name, action, reason_t)
    if cb_mode == "ok_text":
        raw = "not json at all"
    else:  # ok_json: declined card answer echoing the server name
        raw = json.dumps({"status": "declined", "server": name})
    try:
        return (json.loads(raw), name, action, reason_t)
    except (TypeError, ValueError):
        return ({"status": "error", "detail": str(raw)}, name, action, reason_t)


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "setup":
            out, s, a, r = setup_mcp_tool(
                c.get("server", ""), c.get("action", ""),
                c.get("reason", ""), c.get("cb", "ok_json"))
            print(json.dumps({"out": out, "cb_server": s, "cb_action": a,
                              "cb_reason": r}, ensure_ascii=False))
        elif kind == "schema":
            print(json.dumps({"out": SETUP_MCP_SCHEMA}, ensure_ascii=False))
        else:
            print(json.dumps({"out": None}))


if __name__ == "__main__":
    main()
