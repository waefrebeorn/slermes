#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_files.py (w15_webserver_995db6).

fastapi is stubbed (only HTTPException/Request are imported at module scope);
the live module itself is imported from the quarry and its helpers are called
directly. pathlib containment is evaluated on the same platform as the C build.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


class HTTPException(Exception):
    def __init__(self, status_code, detail):
        super().__init__(str(detail))
        self.status_code = status_code
        self.detail = detail


fastapi = types.ModuleType("fastapi")
fastapi.HTTPException = HTTPException
fastapi.Request = type("Request", (), {})
sys.modules["fastapi"] = fastapi

SRC = Q + "/hermes_cli/web_server_files.py"
spec = importlib.util.spec_from_file_location("w15_webserver_995db6_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func not in ("path_text", "container_cwd", "path_is_under"):
        continue
    cases += 1
    try:
        if func == "path_text":
            raw = r.get("raw", "")
            if r.get("raw_nul"):
                raw += "\x00"
            try:
                value = live._path_text(raw)
                py = {"ok": True, "value": value}
            except HTTPException as exc:
                py = {"error": exc.detail, "status": exc.status_code}
        elif func == "container_cwd":
            py = live._is_container_cwd(r.get("host", ""))
        else:
            py = live._path_is_under(Path(r.get("root", "")), Path(r.get("target", "")))
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w15_webserver_995db6: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
