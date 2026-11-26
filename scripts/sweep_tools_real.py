#!/usr/bin/env python3
"""Call read-only tools with arguments they can actually act on, and show what comes back.

sweep_tools.py proves the refusal paths (every tool rejects empty arguments sensibly). This proves the
*work* paths: each case below hands a tool something real -- a file that exists, a page that loads, a
query with hits -- and prints the answer. Anything that still answers "missing", "not found" or an empty
list against a real input is a lie worth chasing.

    python scripts/sweep_tools_real.py [--url ...] [--all]

Only read-only tools are listed. Nothing here sends a message, writes a file outside .testbin, schedules
a job, or restores a snapshot.
"""
from __future__ import annotations

import argparse
import json
import sys
import urllib.request
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
T = REPO / ".testbin"
PAGE = "http://127.0.0.1:8899/"

CASES = [
    ("read_file", {"path": str(REPO / "README.md")}),
    ("file_hash", {"path": str(T / "empty.bin")}),
    ("file_hex", {"path": str(T / "empty.bin")}),
    ("file_diff", {"path_a": str(T / "ok.json"), "path_b": str(T / "bad.json")}),
    ("file_permissions", {"path": str(REPO / "README.md")}),
    ("file_syntax", {"path": str(T / "good.py")}),
    ("file_syntax", {"path": str(T / "broken.py")}),
    ("search_files", {"pattern": "SKILL_PRUNED", "path": str(REPO / "src"), "target": "content"}),
    ("session_search", {"query": "vault"}),
    ("skill_search", {"query": "vault"}),
    ("skill_view", {"name": "apple-notes"}),
    ("skill_validate", {"name": "apple-notes"}),
    ("skills_list", {}),
    ("skill_scan", {}),
    ("env_probe", {}),
    ("path_security", {"path": str(REPO / "README.md")}),
    ("strip_ansi", {"text": "\u001b[31mred\u001b[0m"}),
    ("strip_unicode_tags", {"text": "a\U000E0041b"}),
    ("browser_navigate", {"url": PAGE}),
    ("browser_snapshot", {}),
    ("browser_get_images", {}),
    ("web_get", {"url": "https://example.com/"}),
    ("video_analyze", {"video_url": str(T / "clip.mp4")}),
    ("transcribe_audio", {"file_path": str(T / "tone.mp3")}),
    ("vision_analyze", {"image_url": str(T / "empty.bin")}),
    ("curator_list", {}),
    ("mcp_status", {}),
    ("kanban_list", {}),
    ("todo_list", {}),
    ("process", {"action": "list"}),
    # computer_use list_apps answered {"ok": true, "message": "Use tasklist or EnumWindows"} until
    # the Windows backend was implemented; keep it in the sweep so PROSE-only success stays caught.
    ("computer_use", {"action": "list_apps"}),
]


def rpc(url: str, method: str, params=None, timeout: float = 90):
    body = {"jsonrpc": "2.0", "id": 1, "method": method}
    if params:
        body["params"] = params
    req = urllib.request.Request(url, data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def brief(res: dict, limit: int = 150) -> str:
    """One line that shows the shape of the answer, not a wall of JSON."""
    if not isinstance(res, dict):
        return str(res)[:limit]
    keep = {}
    for k, v in res.items():
        if isinstance(v, (list, dict)):
            keep[k] = f"<{type(v).__name__} {len(v)}>"
        else:
            keep[k] = v
    return json.dumps(keep)[:limit]



_PROSE_ONLY = {"ok", "success", "action", "message", "status", "kind", "mode", "hint", "detail", "error"}


def _empty_success(res: dict) -> bool:
    """A success reply that carries no data at all -- only words about itself."""
    succeeded = res.get("ok") is True or res.get("success") is True
    if not succeeded:
        return False
    data = {k: v for k, v in res.items() if k not in _PROSE_ONLY}
    return len(data) == 0 and isinstance(res.get("message"), str) and bool(res["message"])


def ensure_fixtures() -> None:
    """Create the inputs the cases act on. A probe that cannot lay its own table reports the missing
    fixture as a lying tool: file_diff flagged a truthfully-reported absent bad.json as a suspect."""
    T.mkdir(parents=True, exist_ok=True)
    files = {
        "ok.json": '{"a": 1, "b": [1, 2, 3]}\n',
        "bad.json": '{"a": 1, "b": [1, 2, }\n',
        "good.py": "def add(a, b):\n    return a + b\n",
        "broken.py": "def add(a, b)\n    return a +\n",
        "a.txt": "alpha\nbeta\ngamma\n",
        "empty.bin": "",
    }
    for name, body in files.items():
        path = T / name
        if not path.exists():
            path.write_text(body, encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:9100/mcp")
    ap.add_argument("--json", default="")
    args = ap.parse_args()

    ensure_fixtures()
    results, suspects = {}, []
    for tool, arguments in CASES:
        try:
            res = rpc(args.url, "tools/call", {"name": tool, "arguments": arguments})["result"]
        except Exception as e:                                 # noqa: BLE001
            res = {"_transport": f"{type(e).__name__}: {str(e)[:80]}"}
        results[f"{tool}:{json.dumps(arguments)[:40]}"] = res
        line = brief(res)
        print(f"{tool:20} {line}")

        # Only the answer's own verdict counts. Scanning the whole payload flagged skill_scan for
        # returning 232 skills -- one of them happens to have "missing" in its name.
        verdict = ""
        if isinstance(res, dict):
            verdict = " ".join(str(res[k]) for k in ("error", "reason", "detail") if k in res).lower()
            if res.get("success") is False:
                verdict += " success=false"
            elif res.get("ok") is False:
                verdict += " ok=false"
        if "_transport" in json.dumps(res):
            suspects.append((tool, "TRANSPORT", line))
        elif any(w in verdict for w in ("not found", "missing", "no such file", "no skills directory")):
            suspects.append((tool, "ABSENCE", line))
        elif isinstance(res, dict) and res == {}:
            suspects.append((tool, "EMPTY", line))
        elif isinstance(res, dict) and _empty_success(res):
            # ok:true and nothing but a sentence. computer_use list_apps answered
            # {"ok": true, "message": "Use tasklist or EnumWindows"} for weeks.
            suspects.append((tool, "PROSE", line))

    if args.json:
        Path(args.json).write_text(json.dumps(results, indent=1), encoding="utf-8")

    print(f"\n{len(CASES)} case(s), {len(suspects)} suspect")
    for tool, kind, line in suspects:
        print(f"  {kind:9} {tool:20} {line}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
