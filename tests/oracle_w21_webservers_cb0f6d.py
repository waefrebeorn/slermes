#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_static_assets.py (gate w21_webservers_cb0f6d).

`is_static_asset_path` and `public_font_file` are the live functions. The module's
only non-pure value is `_WEB_PUBLIC` (`Path(project_root_str())/"web"/"public"`,
the config seam), repointed at the isolated SLERMES_HOME the harness also uses;
the fixture's `files` are materialized there so `is_file()` / `is_relative_to()`
run against a real tree. Results are reported relative to that home, so the C
side's absolute-with-'/' answer and this one compare as the same text.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "web_server_static_assets.py"
GATE = "w21_webservers_cb0f6d"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

BASE = Path(os.environ.get("SLERMES_HOME") or os.environ.get("TMPDIR") or ".")

_spec = importlib.util.spec_from_file_location("web_server_static_assets_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["web_server_static_assets_live"] = live
_spec.loader.exec_module(live)

# config seam: _WEB_PUBLIC is import-time state derived from the project root.
live._WEB_PUBLIC = BASE / "webpublic"


def materialize(files):
    for rel in files or []:
        path = BASE / rel
        if rel.endswith("/"):
            path.mkdir(parents=True, exist_ok=True)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"")


def expected(r):
    f = r.get("func")
    materialize(r.get("files"))
    if f == "is_static_asset":
        return live.is_static_asset_path(r.get("full_path"))
    if f == "font":
        hit = live.public_font_file(BASE / "dist", BASE / "dash", r.get("full_path"))
        if hit is None:
            return None
        return os.path.relpath(str(hit), str(BASE)).replace("\\", "/")
    if f == "vocabulary":
        return {"prefixes": list(live.STATIC_ASSET_PREFIXES),
                "files": sorted(live.STATIC_ASSET_FILES),
                "public_font_dirs": list(live._PUBLIC_FONT_DIRS)}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"is_static_asset", "font", "vocabulary"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
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
