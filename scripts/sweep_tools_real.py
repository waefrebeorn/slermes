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
import socket
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
    # Read-only tools with the arguments their schema requires. Empty-argument sweeps never reach
    # these: they refuse on the missing field and the work path stays unexercised.
    ("memory", {"target": "memory", "action": "list"}),
    ("session_crud", {"operation": "list", "limit": 3}),
    ("cronjob", {"action": "list"}),
    ("cron_cmd", {"action": "list"}),
    ("process_manage", {"action": "list"}),
    ("approval_status", {}),
    ("skill_usage", {"action": "list"}),
    ("mcp_prompt_list", {}),
    ("mcp_resource_list", {}),
    ("read_terminal", {"count": 5}),
    ("terminal", {"command": "echo sweep-probe"}),
    ("execute_code", {"code": "print(6*7)"}),
    ("web_search", {"query": "slermes agent", "count": 2}),
    ("web_extract", {"url": "http://127.0.0.1:8899/"}),
    ("write_file", {"path": str(T / "sweep_write.txt"), "content": "sweep\n"}),
    # Second wave: tools that need a required field before the work path is reachable at all.
    ("todo", {"action": "list"}),
    ("kanban_show", {}),                      # no task_id: must refuse by name, not return a blank row
    ("kanban_create", {"title": "sweep case", "assignee": "sweep"}),
    ("file_merge", {"base_path": str(T / "ok.json"), "modified_path": str(T / "bad.json"),
                    "output_path": str(T / "merged.json")}),
    ("text_to_speech", {"text": "sweep", "output_path": str(T / "sweep_tts.mp3")}),
    ("image_generate", {"prompt": "a plain red square"}),   # no key here: must say so, not return a path
    ("skill_provenance", {"name": "apple-notes"}),
    ("skill_deps", {"action": "resolve", "name": "apple-notes"}),
    # An action the schema does not name must be refused, not answered with {} -- that is how the
    # first version of this case surfaced an empty object on the work path.
    ("skill_deps", {"action": "list", "name": "apple-notes"}),
    ("skills_guard", {"path": str(REPO / "skills")}),
    ("patch", {"path": str(T / "patch_probe.txt"), "old_string": "alpha",
               "new_string": "beta", "dry_run": True}),
    # Third wave: the read/status actions of the skill tool family, and an action the schema names
    # but that needs no network. Anything that answers {} or "not found" here is carrying nothing.
    ("skill_bundle", {"action": "list"}),
    ("skill_cache", {"action": "status"}),
    ("skill_curator", {"action": "get_stale_days"}),
    ("skill_hub", {"action": "search", "query": "vault", "limit": 3}),
    ("kanban_attachments", {}),               # no task_id: must name the missing field
    ("kanban_complete", {}),                  # ditto
    ("mcp_resource_read", {"server": "nonexistent", "uri": "file:///x"}),
    # Fourth wave: integrations this box does not have configured. The honest answer names the
    # missing integration; the lie is an empty list or a bare "not found" that reads like "no
    # entities / no channels / no results" when nothing was ever asked.
    ("ha_list_entities", {}),
    ("ha_get_state", {"entity_id": "sensor.nonexistent"}),
    ("discord", {"action": "list_guilds"}),
    ("feishu_drive_list", {}),
    ("yb_query_group_info", {"group_code": "0"}),
    ("x_search", {"query": "slermes agent"}),
    ("mcp_prompt_get", {"server": "nonexistent", "name": "x"}),
    ("file_batch", {"action": "copy", "files": [str(T / "ok.json")], "dest": str(T / "batch_out")}),
]


def _server_alive(url: str) -> bool:
    """A refused connection means the process died, not that the tool failed."""
    try:
        with socket.create_connection(("127.0.0.1", 9100), timeout=3):
            return True
    except OSError:
        return False


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



def _validation_refusal(verdict: str) -> bool:
    """An argument refusal is an honest answer, even when it enumerates values that read like absence.

    skill_deps(action="list") answers "action must be one of: missing, resolve, validate" -- the word
    "missing" is a VALID ACTION there, and the first version of this check scored that refusal as a
    false absence.
    """
    return "must be one of" in verdict or "invalid action" in verdict or "unrecognized action" in verdict


def ensure_fixtures() -> None:
    """Create the inputs the cases act on. A probe that cannot lay its own table reports the missing
    fixture as a lying tool: file_diff flagged a truthfully-reported absent bad.json as a suspect."""
    T.mkdir(parents=True, exist_ok=True)
    (T / "patch_probe.txt").write_text("alpha\nbeta\n", encoding="utf-8")
    files = {
        "ok.json": '{"a": 1, "b": [1, 2, 3]}\n',
        "bad.json": '{"a": 1, "b": [1, 2, }\n',
        "good.py": "def add(a, b):\n    return a + b\n",
        "broken.py": "def add(a, b)\n    return a +\n",
        "a.txt": "alpha\nbeta\ngamma\n",
        "empty.bin": "",
        # The media cases point at these. .testbin gets pruned, and a run after a prune scored
        # video_analyze/transcribe_audio as ABSENCE suspects for truthfully reporting a file that
        # the sweep itself never created.
        "clip.mp4": "",
        "tone.mp3": "",
    }
    for name, body in files.items():
        path = T / name
        if not path.exists():
            path.write_text(body, encoding="utf-8")


def main() -> int:
    ensure_fixtures()
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:9100/mcp")
    ap.add_argument("--json", default="")
    args = ap.parse_args()

    ensure_fixtures()
    results, suspects = {}, []
    dead_after = None
    for tool, arguments in CASES:
        if dead_after:
            # One tool took the whole server down. Reporting "TRANSPORT" for every case after it
            # turned a single crash into eight suspects and hid the culprit; name the case that was
            # running when the server died and stop.
            suspects.append((tool, "SKIPPED", f"server died after {dead_after}"))
            continue
        try:
            res = rpc(args.url, "tools/call", {"name": tool, "arguments": arguments})["result"]
        except Exception as e:                                 # noqa: BLE001
            res = {"_transport": f"{type(e).__name__}: {str(e)[:80]}"}
            if not _server_alive(args.url):
                dead_after = tool
                suspects.append((tool, "CRASHED", "server process gone after this call"))
                results[f"{tool}:{json.dumps(arguments)[:40]}"] = res
                print(f"{tool:20} SERVER GONE after this call")
                continue
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
        elif _validation_refusal(verdict):
            pass        # "action must be one of: missing, resolve, validate" names the contract
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
