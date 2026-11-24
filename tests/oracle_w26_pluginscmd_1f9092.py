#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_cmd_capabilities.py
(gate w26_pluginscmd_1f9092).

The live consent helpers are called directly: `_declared_capabilities_from_manifest`
(the manifest declaration normalizer) and `plugin_capabilities._known` (the
registry filter).  Nothing on the consent path touches the console, the TTY or
the config file here -- those are seams.  The registry table the screen is built
from is compared row for row.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_pluginscmd_1f9092"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import plugin_capabilities as pc  # noqa: E402
from hermes_cli import plugins_cmd_capabilities as live  # noqa: E402

WANTED = {"declared", "known", "constants"}


def declared(req):
    manifest = req.get("manifest")
    name = req.get("plugin_name")
    if name is None:
        return live._declared_capabilities_from_manifest(manifest)
    return live._declared_capabilities_from_manifest(manifest, name)


def known(req):
    caps = req.get("capabilities")
    return pc._known(caps if isinstance(caps, list) else [])


def constants():
    rows = []
    for cid, path, desc in pc._CAPABILITY_ROWS:
        rows.append({"id": cid, "legacy_path": list(path), "description": desc})
    return {"capabilities": rows, "granted_key": pc.GRANTED_KEY, "consent_key": pc.CONSENT_KEY}


def expected(r):
    f = r.get("func")
    if f == "declared":
        return declared(r)
    if f == "known":
        return known(r)
    if f == "constants":
        return constants()
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
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
