#!/usr/bin/env python3
"""Call every registered MCP tool with empty arguments and report the answers that cannot be right.

    python scripts/sweep_tools.py [--url http://127.0.0.1:9100/mcp] [--json out.json]

A registered tool with no arguments should refuse, ask for what it needs, or answer honestly. This finds
the ones that instead:

  * kill the server (a crash takes every other tool with it -- session_search did exactly that)
  * answer with nothing at all   `{}` for a list tool
  * answer "everything is missing" on a machine where it is not (the curator said "No skills directory
    found" with 258 skills on disk)

Side-effecting tools are skipped by name: this sweep must never send a message, spawn a shell, write a
file or generate media.
"""
from __future__ import annotations

import argparse
import json
import sys
import urllib.request

SKIP = {
    # sends things to people / the outside world
    "send_message", "react_to_message", "discord", "discord_admin", "feishu_drive_add_comment",
    "feishu_drive_reply_comment", "yb_send_dm", "yb_send_sticker", "yb_search_sticker",
    # generates media or spends money
    "image_generate", "video_generate", "xai_video_edit", "xai_video_extend", "xai_http",
    "text_to_speech", "voice_speak", "voice_listen", "delegate_task", "mixture_of_agents",
    # runs commands / writes files
    "terminal", "execute_code", "write_file", "patch", "skill_manage", "memory", "computer_use",
    "gui_tour", "browser_generate_pdf", "close_terminal",
    # schedules or mutates state
    "cronjob", "cronjob_manage", "cron_cmd", "kanban_create", "kanban_comment", "kanban_attach",
    "curator_rollback", "curator_snapshot",   # restore/overwrite the skills tree
    "kanban_attach_url", "session_crud", "set_theme", "todo", "todo_list", "approval_status",
}

# "Nothing here" answers that are only believable if the thing really is absent.
FALSE_ABSENCE = ("No skills directory found", "not found", "No such file")


def rpc(url: str, method: str, params: dict | None = None, timeout: float = 30):
    body = {"jsonrpc": "2.0", "id": 1, "method": method}
    if params:
        body["params"] = params
    req = urllib.request.Request(url, data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:9100/mcp")
    ap.add_argument("--json", default="")
    args = ap.parse_args()

    try:
        tools = rpc(args.url, "tools/list")["result"]["tools"]
    except Exception as e:                                     # noqa: BLE001
        print(f"no server at {args.url}: {type(e).__name__}: {e}")
        return 2

    names = sorted(t["name"] for t in tools if t["name"] not in SKIP)
    results, findings = {}, []
    for name in names:
        try:
            results[name] = rpc(args.url, "tools/call", {"name": name, "arguments": {}})["result"]
        except Exception as e:                                 # noqa: BLE001
            results[name] = {"_transport": str(e)[:120]}
            # A timeout is the tool being slow; a dead connection means the server is gone and every
            # later call would report the same thing. Tell them apart before giving up.
            try:
                rpc(args.url, "tools/list", timeout=8)
                findings.append((name, "SLOW", str(e)[:100]))
            except Exception:                                  # noqa: BLE001
                findings.append((name, "CRASH", str(e)[:100]))
                break

        res = results[name]
        text = json.dumps(res)
        if res == {}:
            findings.append((name, "EMPTY", "returned {} -- no shape, no reason"))
        elif res.get("error") and any(f in str(res["error"]) for f in FALSE_ABSENCE):
            findings.append((name, "ABSENCE", str(res["error"])[:100]))

    if args.json:
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump(results, fh, indent=1)

    print(f"swept {len(results)} tool(s); {len(names)} in scope")
    for name, kind, detail in findings:
        print(f"  {kind:8} {name:26} {detail}")
    if not findings:
        print("  every tool refused, asked, or answered honestly")
    return 1 if any(k == "CRASH" for _, k, _ in findings) else 0


if __name__ == "__main__":
    sys.exit(main())
