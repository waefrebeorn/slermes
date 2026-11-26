#!/usr/bin/env python3
"""computer_use must answer with data, not with instructions.

It used to reply {"ok": true, "message": "Use tasklist or EnumWindows"} for list_apps -- a success
flag, no apps -- and focus_app("anything") called FindWindowA(NULL, NULL), which returns *some*
window, so it reported success and stole focus from whatever was in front.

    python scripts/probe_computer_use.py [--url http://127.0.0.1:9100/mcp]
"""
from __future__ import annotations

import argparse
import json
import sys
import urllib.request


def rpc(url: str, method: str, params=None, timeout: float = 120):
    body = {"jsonrpc": "2.0", "id": 1, "method": method}
    if params:
        body["params"] = params
    req = urllib.request.Request(url, data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def call(url: str, tool: str, arguments: dict) -> dict:
    res = rpc(url, "tools/call", {"name": tool, "arguments": arguments})["result"]
    for block in res.get("content", []):
        if isinstance(block, dict) and block.get("type") == "text":
            try:
                return json.loads(block["text"])
            except Exception:                                    # noqa: BLE001
                return {"_raw": block["text"][:200]}
    return res if isinstance(res, dict) else {"_raw": str(res)[:200]}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:9100/mcp")
    args = ap.parse_args()
    fails = 0

    def check(name: str, ok: bool, detail: str = "") -> None:
        nonlocal fails
        print(f"{'PASS' if ok else 'FAIL'}  {name}{'' if ok else '  <- ' + detail}")
        if not ok:
            fails += 1

    apps = call(args.url, "computer_use", {"action": "list_apps"})
    rows = apps.get("apps")
    check("list_apps returns an apps array", isinstance(rows, list), json.dumps(apps)[:160])
    check("list_apps is not a sentence in message",
          not str(apps.get("message", "")).lower().startswith("use "),
          f"message={apps.get('message')!r}")
    if isinstance(rows, list) and rows:
        first = rows[0]
        check("an app row carries a pid", isinstance(first.get("pid"), (int, float)),
              f"row={json.dumps(first)[:120]}")
        check("an app row carries a title", bool(first.get("title")),
              f"row={json.dumps(first)[:120]}")
        check("list_apps reports the same count it returned",
              apps.get("count") in (None, len(rows)), f"count={apps.get('count')!r} rows={len(rows)}")

    bogus = call(args.url, "computer_use",
                 {"action": "focus_app", "app": "zzz-no-such-window-title-zzz"})
    check("focus_app on a missing window reports failure", bogus.get("ok") is False,
          json.dumps(bogus)[:160])

    print(f"\n{fails} failed assertion(s)")
    return fails


if __name__ == "__main__":
    sys.exit(main())
