#!/usr/bin/env python3
"""Reference oracle for hermes_cli/logs.py.

Replays the same fixture and operations as tests/t_port_cli_logs.c and emits
one JSON record per case in the harness's shape. No C output is read.
"""
import contextlib
import io
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
from hermes_cli import logs as L

AGENT_LOG = (
    "2026-04-05 22:35:00,123 INFO  agent.run: started\n"
    "2026-04-05 22:35:01,000 WARNING gateway.run: slow link\n"
    "2026-04-05 22:35:02,500 ERROR tools.terminal_tool: boom\n"
    "2026-04-05 22:35:03,000 INFO  [sess_abc] tools.kanban: ok\n"
    "2026-04-05 22:35:04,000 DEBUG agent.loop: tick\n"
    "2026-04-05 22:35:05,000 INFO  gui.pty_bridge: spawn\n"
    "plain line without a timestamp or level\n"
)
L1 = "2026-04-05 22:35:02,500 ERROR tools.terminal_tool: boom"


def emit(case, ret, store=None):
    print(json.dumps({"case": case, "store": store, "ret": ret},
                     separators=(",", ":"), ensure_ascii=False))


def main():
    hh = Path(os.environ["HERMES_HOME"])
    logdir = hh / "logs"
    logdir.mkdir(parents=True, exist_ok=True)
    (logdir / "agent.log").write_text(AGENT_LOG, encoding="utf-8", newline="")
    (logdir / "errors.log").write_text(
        "2026-04-05 22:35:09,000 ERROR boom\n", encoding="utf-8", newline="")

    emit("parse_since", "1" if L._parse_since("1h") else "0")
    emit("parse_since_bad", "1" if L._parse_since("xyz") is None else "0")
    emit("parse_ts", "1" if L._parse_line_timestamp(L1) else "0")
    emit("parse_ts_bad", "1" if L._parse_line_timestamp("no ts here") is None else "0")
    lv = L._extract_level(L1)
    emit("extract_level", lv if lv is not None else "NONE")
    emit("extract_level_none", "NONE" if L._extract_level("plain") is None else "X")
    emit("extract_logger", L._extract_logger_name(L1) or "NONE")
    emit("extract_logger_sess",
         L._extract_logger_name(
             "2026-04-05 22:35:03,000 INFO  [sess_abc] tools.kanban: ok") or "NONE")

    gw = ("gateway", "hermes_plugins", "plugins.platforms")
    tw = ("tools", "terminal", "write_file")
    emit("match_comp_gateway", "1" if L._line_matches_component(
        "2026-04-05 22:35:01,000 WARNING gateway.run: x", gw) else "0")
    emit("match_comp_tools", "1" if L._line_matches_component(
        "2026-04-05 22:35:02,500 ERROR tools.terminal_tool: x", tw) else "0")
    # The harness labels this "no": it emits 0 on a match, 1 on no match.
    emit("match_comp_no", "0" if L._line_matches_component(
        "2026-04-05 22:35:02,500 ERROR agent.run: x", tw) else "1")

    since = L._parse_line_timestamp("2026-04-05 22:35:03,000")
    emit("filt_level_warn", "1" if L._matches_filters(
        "2026-04-05 22:35:02,500 ERROR x.y: z", min_level="WARNING") else "0")
    # "drop" cases emit 1 when the filter DOES reject the line.
    emit("filt_level_warn_drop", "0" if L._matches_filters(
        "2026-04-05 22:35:00,123 INFO x.y: z", min_level="WARNING") else "1")
    emit("filt_since", "1" if L._matches_filters(
        "2026-04-05 22:35:04,000 INFO x.y: z", since=since) else "0")
    emit("filt_since_drop", "0" if L._matches_filters(
        "2026-04-05 22:35:01,000 INFO x.y: z", since=since) else "1")
    emit("filt_session", "1" if L._matches_filters(
        "2026-04-05 22:35:03,000 INFO [sess_abc] x.y: z",
        session_filter="sess_abc") else "0")
    emit("filt_comp", "1" if L._line_matches_component(
        "2026-04-05 22:35:01,000 WARNING gateway.run: z", gw) else "0")

    agent = logdir / "agent.log"
    tail = L._read_tail(agent, 3, has_filters=False)
    emit("read_tail_3", len(tail),
         [t.rstrip("\r\n") for t in tail])
    ft = L._read_tail(agent, 10, has_filters=True, min_level="WARNING")
    emit("read_tail_filt", len(ft), [t.rstrip("\r\n") for t in ft])

    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        L.list_logs()
    rows = [ln.strip() for ln in buf.getvalue().splitlines()
            if ln.strip().endswith(".log")]
    emit("list_logs", rows)


if __name__ == "__main__":
    main()
