#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_custom_credentials.py
(gate w25_auxiliaryc_9372f1).

The live module is imported.  Three config seams are replaced by the fixture's
answers -- which ``providers:`` entry owns a URL (``custom_entry_for_endpoint`` /
``named_custom_entry``), what the profile secret scope returned for a ``key_env``
(``_scoped_key_env``) and which names the auth registry knows
(``is_known_auth_provider``).  ``entry_declares_credential`` is left live: it is a
pure predicate over the entry dict, and the port owns it too.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_auxiliaryc_9372f1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import agent.auxiliary_client as aux  # noqa: E402
import agent.custom_entry_pool as pool  # noqa: E402
import hermes_cli.auth as ha  # noqa: E402
from agent import auxiliary_custom_credentials as live  # noqa: E402

WANTED = {"normalize_api_key", "identity", "fast_provider",
          "declares_credential", "constants"}

CASE = {}


def _install(req):
    CASE.clear()
    CASE.update(req)
    entry = req.get("entry")
    found = bool(req.get("found")) and isinstance(entry, dict)
    owns = bool(req["owns"]) if "owns" in req else True
    pair = ("entry", entry) if found else None

    pool.custom_entry_for_endpoint = (
        lambda base_url, provider, _pair=pair, _owns=owns: _pair if (_pair and _owns) else None)
    pool.named_custom_entry = lambda name, _pair=pair: _pair
    aux._scoped_key_env = lambda name: req.get("scoped_key_env") or ""
    known = req.get("known") or []
    ha.is_known_auth_provider = lambda key, _known=known: key in _known


def normalize_api_key(req):
    raw = req.get("raw")
    if isinstance(raw, dict) and raw.get("__callable__"):
        raw = lambda: "minted"
    value = live._normalize_api_key(raw)
    if callable(value) and not isinstance(value, str):
        return {"callable": True, "value": None}
    return {"callable": False, "value": value}


def identity(req):
    _install(req)
    result = live.declared_custom_credential_identity(req.get("base_url") or "", req.get("provider") or "")
    if result == "":
        return {"found": False, "parts": []}
    return {"found": True, "parts": result.split("\0")}


def fast_provider(req):
    _install(req)
    return live.fast_catalog_provider(req.get("runtime") or {}, req.get("main_provider"))


def declares_credential(req):
    return bool(pool.entry_declares_credential(req.get("entry")))


def constants():
    return list(pool._DECLARED_CREDENTIAL_KEYS)


def expected(req):
    func = req.get("func")
    if func == "normalize_api_key":
        return normalize_api_key(req)
    if func == "identity":
        return identity(req)
    if func == "fast_provider":
        return fast_provider(req)
    if func == "declares_credential":
        return declares_credential(req)
    if func == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
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
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
