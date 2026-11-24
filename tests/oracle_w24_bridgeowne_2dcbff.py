#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/whatsapp/bridge_ownership.py
(gate w24_bridgeowne_2dcbff).

The live module is executed from its own file and driven against throwaway
profile trees: the port record is a real file, sibling claims are real sibling
records, and only the bind probe is substituted (``port_is_free`` -> the
fixture's busy set), because binding a socket is the OS seam this port cannot
own.  The record-parse failure text names the profile path, so that one
ValueError is compared as its classification.
"""
import importlib.util
import json
import shutil
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_bridgeowne_2dcbff"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The live module file, executed as itself: importing it through the package
# would run plugins/platforms/whatsapp/__init__.py -> adapter -> the gateway
# home + bind chain, which is precisely the seam this port replaces.  The module
# has no package-relative import at module level, so its globals are unchanged.
_spec = importlib.util.spec_from_file_location(
    "w24_bridgeowne_2dcbff_live", QUARRY / "plugins/platforms/whatsapp/bridge_ownership.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"standalone_port", "secondary_port", "read_port_record", "constants"}
TMP = Path(tempfile.mkdtemp(prefix="w24bo_"))
NOT_A_PORT = "is not a port number; delete it to allocate a new port"


def norm(fn):
    """Call the live resolver and turn its ValueError into a value."""
    try:
        return fn()
    except ValueError as exc:
        text = str(exc)
        if text.endswith(NOT_A_PORT):
            return {"error": "not_a_port"}
        return {"error": text}


def profile_tree(record_exists, record_text):
    """A fresh profile home; the record file only exists when asked for."""
    home = Path(tempfile.mkdtemp(dir=TMP)) / "profile"
    record = home / "platforms" / "whatsapp" / "bridge_port"
    record.parent.mkdir(parents=True)
    if record_exists:
        record.write_text("" if record_text is None else str(record_text), encoding="utf-8")
    return home, record


def standalone(req):
    home, _ = profile_tree(bool(req.get("record_exists")), req.get("record_text"))
    return norm(lambda: live.standalone_bridge_port(home, req.get("explicit")))


def secondary(req):
    base = Path(tempfile.mkdtemp(dir=TMP))
    home = base / "profile"
    (home / "platforms" / "whatsapp").mkdir(parents=True)
    if req.get("record_exists"):
        text = req.get("record_text")
        (home / "platforms" / "whatsapp" / "bridge_port").write_text(
            "" if text is None else str(text), encoding="utf-8")
    # sibling profiles: each claim is a real record beside this home
    sibs = req.get("claimed") or []
    for index, port in enumerate(sibs if isinstance(sibs, list) else []):
        sib = base / ("sib%d" % index)
        (sib / "platforms" / "whatsapp").mkdir(parents=True)
        (sib / "platforms" / "whatsapp" / "bridge_port").write_text(str(port), encoding="utf-8")
    busy = set(req.get("busy") or [])
    live.port_is_free = lambda port, _busy=busy: port not in _busy
    return norm(lambda: live.secondary_bridge_port(home, req.get("explicit")))


def read_port_record(req):
    record = Path(tempfile.mkdtemp(dir=TMP)) / "bridge_port"
    text = req.get("text")
    record.write_text("" if text is None else str(text), encoding="utf-8")
    return norm(lambda: live._read_port_record(record))


def constants():
    home = Path(tempfile.mkdtemp(dir=TMP)) / "empty-profile"
    home.mkdir(parents=True, exist_ok=True)
    return {
        "secondary_port_first": live.SECONDARY_PORT_FIRST,
        "secondary_port_last": live.SECONDARY_PORT_LAST,
        "default_port": live.standalone_bridge_port(home, None),
    }


def expected(req):
    func = req.get("func")
    if func == "standalone_port":
        return standalone(req)
    if func == "secondary_port":
        return secondary(req)
    if func == "read_port_record":
        return read_port_record(req)
    if func == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
    try:
        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue
            try:
                req = json.loads(line)
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
                print("MISMATCH %s case=%r py=%r c=%r"
                      % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
