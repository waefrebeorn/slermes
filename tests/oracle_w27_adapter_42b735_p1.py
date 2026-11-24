#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/whatsapp/adapter.py slice 1
(gate w27_adapter_42b735_p1): bridge token + spawn-config identity.

The live module is imported by FILE PATH -- ``plugins/`` is not an installed
package, and its ``__init__`` would drag the whole gateway stack -- with the
quarry root on ``sys.path`` for adapter.py's own absolute imports.  Only the
pure value functions are compared; the socket, the pidfiles and the Node
bridge never start:

  * ``_safe_ints`` / ``_fmt_uptime``      value coercion + uptime rendering;
  * ``_bridge_auth_headers`` / ``_bridge_auth_proof``  bearer + HMAC proof;
  * ``_bridge_config_fingerprint``        the 16-hex spawn-config digest.
"""
import contextlib
import importlib.util
import io
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_42b735_p1"

# The msys bash that runs this oracle has no USERPROFILE, and pathlib.Path.home()
# (touched at import time by hermes_constants) raises without it.  Give it the
# real Windows home the repo lives under.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(Path(REPO).parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = os.environ["USERPROFILE"]

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location(
    "wa_adapter_live_" + GATE, QUARRY / "plugins" / "platforms" / "whatsapp" / "adapter.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
with contextlib.redirect_stdout(io.StringIO()):
    _spec.loader.exec_module(live)

WANTED = {"safe_ints", "auth_headers", "auth_proof", "config_fingerprint", "fmt_uptime"}


def as_dict(value):
    return value if isinstance(value, dict) else {}


def as_list(value):
    return value if isinstance(value, list) else []


def safe_ints(req):
    return live._safe_ints(as_list(req.get("tokens")))


def auth_headers(req):
    return live._bridge_auth_headers(str(req.get("token") or ""))


def auth_proof(req):
    return live._bridge_auth_proof(str(req.get("token") or ""), str(req.get("challenge") or ""))


def config_fingerprint(req):
    return live._bridge_config_fingerprint(as_dict(req.get("env")), str(req.get("package_hash") or ""))


def fmt_uptime(req):
    return live._fmt_uptime(req.get("seconds"))


DISPATCH = {
    "safe_ints": safe_ints,
    "auth_headers": auth_headers,
    "auth_proof": auth_proof,
    "config_fingerprint": config_fingerprint,
    "fmt_uptime": fmt_uptime,
}


def expected(req):
    return DISPATCH[req["func"]](req)


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
            with contextlib.redirect_stdout(io.StringIO()):
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
