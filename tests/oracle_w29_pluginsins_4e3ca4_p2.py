#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_install_scan_context.py (gate
w29_pluginsins_4e3ca4_p2).

Same seams as gate p1: the live module is imported BY FILE PATH from the
read-only quarry, a Finding is rebuilt from the fixture's (pid, sev, cat), and
every other argument is taken from the fixture by parameter name.  Paths are
posix relatives; nothing touches the filesystem or the host.

These are the literal/exec predicates: what a line runs versus what it merely
quotes.

Fail-closed: 0 cases is a failed gate.  Summary line:
'ORACLE w29_pluginsins_4e3ca4_p2: N cases, M mismatches'.
"""
import importlib.util
import inspect
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
LIVE_FILE = QUARRY / "hermes_cli" / "plugins_install_scan_context.py"
GATE = "w29_pluginsins_4e3ca4_p2"

WANTED = {
    "is_inert_fixture_line",
    "is_base64_media",
    "is_base64_archive_roundtrip",
    "is_regex_alternation_token",
    "is_sudo_inert",
    "is_env_dump_in_safety_clause",
    "is_quoted_injection_phrase",
    "is_benign_lazy_os_import",
    "is_authorized_keys_prose",
    "is_loopback_only",
    "is_loopback_continuation",
    "is_coin_name_only",
    "is_public_client_key",
    "is_own_proc_read",
}


def load_live():
    if str(QUARRY) not in sys.path:
        sys.path.insert(0, str(QUARRY))
    spec = importlib.util.spec_from_file_location("pisc_live_p2", LIVE_FILE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


LIVE = load_live()


def expected(req):
    fn = getattr(LIVE, req["func"])
    args = []
    for name in inspect.signature(fn).parameters:
        if name == "finding":
            args.append(LIVE.Finding(
                pattern_id=req.get("pid", ""),
                severity=req.get("sev", ""),
                category=req.get("cat", ""),
                file=req.get("rel_path", ""),
                line=0,
                match="",
                description="",
            ))
        else:
            args.append(req[name])
    return fn(*args)


def main():
    cases = mism = 0
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            req = json.loads(raw)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r line=%r rel=%r"
                  % (req.get("func"), req.get("case"), py, got,
                     req.get("line"), req.get("rel_path")), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or cases == 0) else 0)


if __name__ == "__main__":
    main()
