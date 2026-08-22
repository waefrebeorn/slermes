#!/usr/bin/env python3
"""sta_oracle_plugin_guard.py — Python twin for tools/plugin_guard.py,
mirrored by src/tools/plugin_guard.c. Plugin dirs resolve against
$ORACLE_FIXDIR. scanned_at is excluded (nondeterministic).
"""
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from tools.plugin_guard import scan_plugin, should_allow_plugin_install


def out_line(o):
    print(json.dumps(o, ensure_ascii=False, separators=(",", ":")))


def main():
    fixdir = os.environ.get("ORACLE_FIXDIR", ".")
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        fx = json.load(f)
    for c in fx.get("ops", []):
        kind = c.get("kind", "")
        o = {}
        if kind == "scan":
            d = Path(fixdir) / c.get("dir", "")
            result = scan_plugin(d, source=c.get("source", ""))
            o["verdict"] = result.verdict
            o["trust_level"] = result.trust_level
            o["summary"] = result.summary
            o["findings"] = [
                {
                    "pattern_id": f.pattern_id,
                    "severity": f.severity,
                    "category": f.category,
                    "file": f.file,
                    "line": f.line,
                    "match": f.match,
                    "description": f.description,
                }
                for f in result.findings
            ]
            allowed, reason = should_allow_plugin_install(result, force=False)
            o["allowed_null_forced"] = (
                "None" if allowed is None else ("True" if allowed else "False"))
            o["reason"] = reason
            forced_allowed, _ = should_allow_plugin_install(result, force=True)
            o["force_allowed"] = bool(forced_allowed)
        out_line(o)


if __name__ == "__main__":
    main()
