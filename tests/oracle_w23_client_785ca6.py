#!/usr/bin/env python3
"""Differential oracle for agent/lsp/client.py (gate w23_client_785ca6).

The live module is executed from the quarry; the LSP server process, its stdio
pipes and the event loop are never started.  What is compared is the part of
the client that decides things from values the transport already produced:

  * ``file_uri`` / ``uri_to_path`` / ``_folder`` -- abspath (the host's ntpath,
    which stays ntpath even when the fixture asks for the POSIX os.name branch,
    exactly as in the live module), quote/unquote and normpath;
  * ``_end_position`` (UTF-16 code units) and ``as_disk_read`` (BOM + newline
    normalisation);
  * ``_diagnostic_key`` / ``_dedupe`` and ``diagnostics_for`` over a
    ``_DocState`` map injected straight into a real client instance;
  * ``_config_section`` / the workspace/configuration answer.

os.name is the seam for the two URI branches, so the fixture names it and it is
restored after every case.
"""
import asyncio
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "agent" / "lsp" / "client.py"
GATE = "w23_client_785ca6"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("lsp_client_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["lsp_client_live"] = live
_spec.loader.exec_module(live)

WANTED = {"file_uri", "uri_to_path", "folder", "end_position", "as_disk_read",
          "diagnostic_key", "dedupe", "doc_fresh", "diagnostics_for",
          "document_text", "config_section", "workspace_configuration", "limits"}


def norm(value):
    """JSON-visible shape: tuples become lists (json.dumps does the same)."""
    if isinstance(value, tuple):
        return [norm(v) for v in value]
    if isinstance(value, list):
        return [norm(v) for v in value]
    if isinstance(value, dict):
        return {k: norm(v) for k, v in value.items()}
    return value


def with_os_name(name, fn):
    saved = live.os.name
    live.os.name = name
    try:
        return fn()
    finally:
        live.os.name = saved


def make_client(init_options):
    return asyncio.run(_make_client(init_options))


async def _make_client(init_options):
    return live.LSPClient(server_id="probe", workspace_root="C:/w",
                          command=["srv"], initialization_options=init_options)


def doc_states(req):
    docs = {}
    for entry in req.get("docs") or []:
        path = entry.get("path")
        state = live._DocState(
            version=entry.get("version", 0),
            text=entry.get("text", ""),
            push=list(entry.get("push") or []),
            pull=list(entry.get("pull") or []),
            push_version=entry.get("push_version", -1),
            pull_version=entry.get("pull_version", -1),
            seed_seen=bool(entry.get("seed_seen", False)),
        )
        docs[live.os.path.abspath(path)] = state
    return docs


def expected(r):
    f = r.get("func")
    if f == "file_uri":
        return with_os_name(r.get("os_name", "nt"), lambda: live.file_uri(r.get("path", "")))
    if f == "uri_to_path":
        return with_os_name(r.get("os_name", "nt"), lambda: live.uri_to_path(r.get("uri", "")))
    if f == "folder":
        return with_os_name(r.get("os_name", "nt"), lambda: live._folder(r.get("root", "")))
    if f == "end_position":
        return live._end_position(r.get("text", ""))
    if f == "as_disk_read":
        return live.as_disk_read(r.get("text", ""))
    if f == "diagnostic_key":
        return live._diagnostic_key(r.get("diag") or {})
    if f == "dedupe":
        return live._dedupe(*[lst for lst in r.get("lists") or []])
    if f == "doc_fresh":
        entry = r.get("doc") or {}
        state = live._DocState(
            version=entry.get("version", 0),
            push_version=entry.get("push_version", -1),
            pull_version=entry.get("pull_version", -1),
        )
        at = r.get("at")
        kind = r.get("kind", "any")
        if kind == "push":
            return state.fresh_push(at)
        if kind == "pull":
            return state.fresh_pull(at)
        return state.fresh(at)
    if f == "diagnostics_for":
        client = make_client(None)
        client._docs = doc_states(r)
        return client.diagnostics_for(r.get("path", ""), fresh_only=bool(r.get("fresh_only")))
    if f == "document_text":
        client = make_client(None)
        client._docs = doc_states(r)
        return client.document_text(r.get("path", ""))
    if f == "config_section":
        client = make_client(r.get("init") if isinstance(r.get("init"), dict) else None)
        return client._config_section(r.get("item"))
    if f == "workspace_configuration":
        client = make_client(r.get("init") if isinstance(r.get("init"), dict) else None)
        return asyncio.run(client._handle_workspace_configuration(r.get("params")))
    if f == "limits":
        return {
            "max_content_modified_retries": live.MAX_CONTENT_MODIFIED_RETRIES,
            "stderr_tail_lines": live.STDERR_TAIL_LINES,
            "max_tracked_files": live.MAX_TRACKED_FILES,
            "stream_limit": live._STREAM_LIMIT,
            "initialize_timeout": live.INITIALIZE_TIMEOUT,
            "diagnostics_document_wait": live.DIAGNOSTICS_DOCUMENT_WAIT,
            "diagnostics_full_wait": live.DIAGNOSTICS_FULL_WAIT,
            "diagnostics_request_timeout": live.DIAGNOSTICS_REQUEST_TIMEOUT,
            "push_debounce": live.PUSH_DEBOUNCE,
            "shutdown_grace": live.SHUTDOWN_GRACE,
            "retry_base_delay": live.RETRY_BASE_DELAY,
            "live_states": sorted(live._LIVE_STATES),
        }
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = norm(expected(r))
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
