#!/usr/bin/env python3
"""sta_oracle_console_engine_pure.py — oracle twin for console_engine_pure.c.

Calls the LIVE hermes_cli/console_engine.py pure helpers and emits one JSON
line per op. The C harness (t_port_console_engine_pure.c) emits the same
shape; run_oracle.sh diffs them.

Fixture shape:
{
  "ops": [
    {"op": "is_footer_rule", "line": "..."},
    {"op": "strip_footer", "text": "..."},
    {"op": "table_summary", "summary": "...", "limit": 76},
    {"op": "split_line", "line": "..."},
    {"op": "contains_shell", "line": "...", "tokens": "|,&,&&"},
    {"op": "format_sessions", "sessions": [...]},
    {"op": "format_job", "job": {...}, "action": "..."},
    {"op": "clean_summary", "text": "..."}
  ]
}
"""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "hermes_cli", "console_engine.py")
    spec = importlib.util.spec_from_file_location("live_console_engine", cand)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    path = sys.argv[1]
    with open(path, encoding="utf-8") as f:
        spec = json.load(f)
    mod = _load_module()
    out = []
    for case in spec.get("ops", []):
        op = case.get("op")
        if op == "is_footer_rule":
            out.append({"op": op, "out": bool(mod._is_status_footer_rule(case.get("line", "")))})
        elif op == "strip_footer":
            out.append({"op": op, "out": mod._strip_console_status_footer(case.get("text", "")) or ""})
        elif op == "table_summary":
            out.append({"op": op, "out": mod._table_summary(case.get("summary", ""), case.get("limit", 76)) or ""})
        elif op == "split_line":
            try:
                toks = mod._split_line(case.get("line", ""))
                out.append({"op": op, "out": json.dumps(toks, ensure_ascii=False)})
            except Exception as e:
                out.append({"op": op, "out": "__ERR__: " + str(e)})
        elif op == "contains_shell":
            out.append({"op": op, "out": bool(mod._contains_shell_syntax(case.get("line", ""), case.get("tokens", "")))})
        elif op == "format_sessions":
            out.append({"op": op, "out": mod._format_sessions(case.get("sessions", [])) or ""})
        elif op == "format_job":
            out.append({"op": op, "out": mod._format_job(case.get("job", {}), case.get("action", "")) or ""})
        elif op == "clean_summary":
            out.append({"op": op, "out": mod._clean_summary(case.get("text", "")) or ""})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
