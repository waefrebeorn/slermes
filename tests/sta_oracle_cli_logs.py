#!/usr/bin/env python3
"""
sta_oracle_cli_logs.py — Oracle for hermes_cli/logs.py
(port src/cli/port_cli_logs.c).

Replays the identical operations against LIVE Python and compares each case's
emitted JSON with the C harness output (read from stdin).
"""
import sys, os, io, json, re, tempfile, contextlib
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from hermes_cli import logs as L

def main():
    data = sys.stdin.read()
    c_lines = {}
    for ln in data.splitlines():
        if not ln.strip():
            continue
        try:
            obj = json.loads(ln)
        except Exception:
            continue
        c_lines[obj["case"]] = obj

    # Reconstruct the identical log fixture in THIS HERMES_HOME (T2).
    from pathlib import Path
    hh = os.environ["HERMES_HOME"]
    logdir = Path(hh) / "logs"
    logdir.mkdir(parents=True, exist_ok=True)
    (logdir / "agent.log").write_text(
        "2026-04-05 22:35:00,123 INFO  agent.run: started\n"
        "2026-04-05 22:35:01,000 WARNING gateway.run: slow link\n"
        "2026-04-05 22:35:02,500 ERROR tools.terminal_tool: boom\n"
        "2026-04-05 22:35:03,000 INFO  [sess_abc] tools.kanban: ok\n"
        "2026-04-05 22:35:04,000 DEBUG agent.loop: tick\n"
        "2026-04-05 22:35:05,000 INFO  gui.pty_bridge: spawn\n"
        "plain line without a timestamp or level\n",
        encoding="utf-8",
    )
    (logdir / "errors.log").write_text(
        "2026-04-05 22:35:09,000 ERROR boom\n", encoding="utf-8")

    results = {}

    # 1. parse_since
    results["parse_since"] = {"store": None, "ret": "1" if L._parse_since("1h") else "0"}
    results["parse_since_bad"] = {"store": None, "ret": "1" if L._parse_since("xyz") is None else "0"}

    # 2. parse_line_timestamp
    L1 = "2026-04-05 22:35:02,500 ERROR tools.terminal_tool: boom"
    results["parse_ts"] = {"store": None, "ret": "1" if L._parse_line_timestamp(L1) else "0"}
    results["parse_ts_bad"] = {"store": None, "ret": "1" if L._parse_line_timestamp("no ts here") is None else "0"}

    # 3. extract_level
    lv = L._extract_level(L1)
    results["extract_level"] = {"store": None, "ret": lv if lv is not None else "NONE"}
    results["extract_level_none"] = {"store": None, "ret": "NONE" if L._extract_level("plain") is None else "X"}

    # 4. extract_logger_name
    results["extract_logger"] = {"store": None, "ret": L._extract_logger_name(L1) or "NONE"}
    results["extract_logger_sess"] = {"store": None, "ret": L._extract_logger_name("2026-04-05 22:35:03,000 INFO  [sess_abc] tools.kanban: ok") or "NONE"}

    # 5. line_matches_component
    gw = ("gateway", "hermes_plugins", "plugins.platforms")
    results["match_comp_gateway"] = {"store": None, "ret": "1" if L._line_matches_component("2026-04-05 22:35:01,000 WARNING gateway.run: x", gw) else "0"}
    results["match_comp_tools"] = {"store": None, "ret": "1" if L._line_matches_component("2026-04-05 22:35:02,500 ERROR tools.terminal_tool: x", ("tools",)) else "0"}
    results["match_comp_no"] = {"store": None, "ret": "0" if L._line_matches_component("2026-04-05 22:35:02,500 ERROR agent.run: x", ("tools",)) else "1"}

    # 6. matches_filters
    since = L._parse_line_timestamp("2026-04-05 22:35:03,000")
    results["filt_level_warn"] = {"store": None, "ret": "1" if L._matches_filters("2026-04-05 22:35:02,500 ERROR x.y: z", min_level="WARNING") else "0"}
    results["filt_level_warn_drop"] = {"store": None, "ret": "0" if L._matches_filters("2026-04-05 22:35:00,123 INFO x.y: z", min_level="WARNING") else "1"}
    results["filt_since"] = {"store": None, "ret": "1" if L._matches_filters("2026-04-05 22:35:04,000 INFO x.y: z", since=since) else "0"}
    results["filt_since_drop"] = {"store": None, "ret": "0" if L._matches_filters("2026-04-05 22:35:01,000 INFO x.y: z", since=since) else "1"}
    results["filt_session"] = {"store": None, "ret": "1" if L._matches_filters("2026-04-05 22:35:03,000 INFO [sess_abc] x.y: z", session_filter="sess_abc") else "0"}
    results["filt_comp"] = {"store": None, "ret": "1" if L._line_matches_component("2026-04-05 22:35:01,000 WARNING gateway.run: z", gw) else "0"}

    # 7. read_tail raw last 3
    hh = os.environ["HERMES_HOME"]
    agent = Path(os.path.join(hh, "logs", "agent.log"))
    tail = L._read_tail(agent, 3, has_filters=False)
    results["read_tail_3"] = {"store": [t.rstrip("\n") for t in tail], "ret": len(tail)}

    # 8. read_tail filtered level>=WARNING
    ft = L._read_tail(agent, 10, has_filters=True, min_level="WARNING")
    results["read_tail_filt"] = {"store": [t.rstrip("\n") for t in ft], "ret": len(ft)}

    # 9. list_logs — capture stdout both ways; compare listing block
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        L.list_logs()
    py_listing = buf.getvalue()
    # C emitted between list_logs_start and list_logs_end
    m = re.search(r'list_logs_start"\}\n(.*?)\n\{"case":"list_logs_end"', data, re.S)
    c_listing = m.group(1) if m else ""
    # normalize: keep only the per-file rows (header embeds the random temp
    # HERMES_HOME path, which differs between the C run (T) and Python (T2))
    def norm_listing(s):
        rows = []
        for line in s.splitlines():
            line = line.strip()
            if line.endswith(".log"):
                rows.append(line)
        return "\n".join(rows)
    results["list_logs"] = {"store": None, "ret": "1" if norm_listing(py_listing) == norm_listing(c_listing) else "0"}

    # ---- compare ----
    order = ["parse_since","parse_since_bad","parse_ts","parse_ts_bad",
             "extract_level","extract_level_none","extract_logger","extract_logger_sess",
             "match_comp_gateway","match_comp_tools","match_comp_no",
             "filt_level_warn","filt_level_warn_drop","filt_since","filt_since_drop",
             "filt_session","filt_comp","read_tail_3","read_tail_filt","list_logs"]
    mism = 0; total = 0
    for case in order:
        if case not in results:
            print(f"MISSING PY case {case}"); mism += 1; total += 1; continue
        total += 1
        py = results[case]
        if case == "list_logs":
            # not a JSON case line; compare captured listing blocks
            c_listing_norm = norm_listing(c_listing)
            py_listing_norm = norm_listing(py_listing)
            ok = (c_listing_norm == py_listing_norm)
            if not ok:
                print("  C_block=", repr(c_listing_norm))
                print("  PY_block=", repr(py_listing_norm))
                print("  LEN C=", len(c_listing_norm), "LEN PY=", len(py_listing_norm), "EQ=", c_listing_norm == py_listing_norm)
            c_ret = "1" if ok else "0"
            py_ret = "1" if ok else "0"
        else:
            c = c_lines.get(case)
            if c is None:
                print(f"MISSING C case {case}"); mism += 1; continue
            c_store = c.get("store"); py_store = py.get("store")
            c_ret = c.get("ret"); py_ret = py.get("ret")
            try:
                cs = json.loads(c_store) if c_store not in (None, "null") else None
            except Exception:
                cs = c_store
            ps = py_store
            store_ok = (cs == ps)
            ret_ok = (json.dumps(c_ret) == json.dumps(py_ret))
            ok = store_ok and ret_ok
        if not ok:
            mism += 1
            print(f"MISMATCH [{case}]")
            print(f"  C_ret={c_ret!r} PY_ret={py_ret!r}")
            if case != "list_logs":
                if c_store is not None: print(f"  C_store={c_store[:200]}")
                if py_store is not None: print(f"  PY_store={str(py_store)[:200]}")
        else:
            print(f"ok [{case}]")
    print(f"\nRESULT: {total-mism}/{total} match, {mism} mismatch")
    sys.exit(1 if mism else 0)

if __name__ == "__main__":
    main()
