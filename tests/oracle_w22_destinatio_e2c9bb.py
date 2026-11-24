#!/usr/bin/env python3
"""Differential oracle for plugins/config_backends/remote/destination.py
(gate w22_destinatio_e2c9bb).

Nothing in this module touches the network until `open_credentialed`, so the
three ported functions run live with no seam at all:

  loopback -- live `_is_loopback` (ipaddress decides, including the
              "LOCALHOST"/"127.1" rejections).
  display  -- live `display_url`.
  checked  -- live `checked_url`, whose CredentialDestinationError is carried
              as {"ok": false, "error": str(exc)}.

The urllib opener (`_RefuseRedirect`) is the network seam and stays upstream,
which is why no fingerprinted request is made here.
"""
import importlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w22_destinatio_e2c9bb"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("plugins.config_backends.remote.destination")

WANTED = {"loopback", "display", "checked"}


def expected(r):
    f = r.get("func")
    if f == "loopback":
        return 1 if live._is_loopback(r.get("host") or "") else 0
    if f == "display":
        return live.display_url(r.get("url") or "")
    if f == "checked":
        url = r.get("url") or ""
        env_name = r.get("env_name") or ""
        try:
            return {"ok": True, "url": live.checked_url(url, env_name=env_name)}
        except live.CredentialDestinationError as exc:
            return {"ok": False, "error": str(exc)}
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
