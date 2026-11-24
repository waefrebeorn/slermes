#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_install_scan_context.py (gate
w29_pluginsins_4e3ca4_p1).

The live module is imported BY FILE PATH from the read-only quarry (a sibling
checkout), so there is no package-cache shadowing of the C port and no build
step in the way of the truth.  The module import chain is
``tools.skills_guard`` (for Finding + the compiled threat patterns), which is
why the quarry root goes on sys.path first.

What is compared: the pure predicates of gate p1, called with a Finding built
from the fixture's (pid, sev, cat) and every other argument taken from the
fixture by parameter name.  The seam is the filesystem: ``rel_path`` is a
posix relative path handed in, never resolved against a real tree.

Fail-closed: 0 cases is a failed gate.  Summary line:
'ORACLE w29_pluginsins_4e3ca4_p1: N cases, M mismatches'.
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
GATE = "w29_pluginsins_4e3ca4_p1"

WANTED = {
    "is_doc_prose",
    "is_ci_workflow",
    "is_agent_facing",
    "prose_cap",
    "is_trusted_installer_pipe",
    "is_locale_catalog",
    "catalog_cap",
    "is_self_uninstall_doc",
    "is_test_tree",
    "is_disclosed_config_note",
    "is_json_prose_value",
    "is_repo_root_traversal",
    "is_pip_install_in_prose_literal",
    "is_hex_in_char_class",
    "logical_line",
}


def load_live():
    if str(QUARRY) not in sys.path:
        sys.path.insert(0, str(QUARRY))
    spec = importlib.util.spec_from_file_location("pisc_live_p1", LIVE_FILE)
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
