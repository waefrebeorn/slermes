#!/usr/bin/env python3
"""Differential oracle for plugins/config_backends/remote/values.py
(gate w24_values_5f77ea).

The live module is imported inside its package (its relative ``.paths`` import
needs it) and its pure rules are called on the JSON the harness read:
``to_wire`` and ``_wire_key`` with ``ConfigValueError`` compared by message,
``is_secret_path`` on decoded segments, and ``secret_literal_path`` on a decoded
value.  The vendored config-secret-keys.json is read by the live module; the C
side carries the same tables as literals, so the ``constants`` case proves the
two agree.  The plane's own re-check on write is the seam and is not compared.

``{"$map": [[k, v], ...]}`` in the fixture decodes to a Python dict with those
exact keys (a JSON object cannot carry an int or bool key).
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_values_5f77ea"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.config_backends.remote import values as live  # noqa: E402
from hermes_cli.config_backend import ConfigValueError  # noqa: E402

WANTED = {"to_wire", "wire_key", "is_secret_path", "secret_literal_path", "constants"}


def decode(value):
    """Fixture -> Python value, restoring mappings with non-string keys."""
    if isinstance(value, dict):
        if set(value) == {"$map"} and isinstance(value["$map"], list):
            out = {}
            for pair in value["$map"]:
                if not isinstance(pair, list) or len(pair) != 2:
                    continue
                out[pair[0]] = decode(pair[1])
            return out
        return {k: decode(v) for k, v in value.items()}
    if isinstance(value, list):
        return [decode(v) for v in value]
    return value


def _path(r):
    raw = r.get("path")
    return tuple(raw) if isinstance(raw, list) else ()


def to_wire(r):
    try:
        return {"ok": True, "value": live.to_wire(decode(r.get("value")), _path(r))}
    except ConfigValueError as exc:
        return {"ok": False, "error": str(exc)}


def wire_key(r):
    try:
        return {"ok": True, "value": live._wire_key(decode(r.get("key")), _path(r))}
    except ConfigValueError as exc:
        return {"ok": False, "error": str(exc)}


def is_secret_path(r):
    segs = r.get("segments")
    return live.is_secret_path(tuple(segs) if isinstance(segs, list) else ())


def secret_literal_path(r):
    hit = live.secret_literal_path(_path(r), decode(r.get("value")))
    return None if hit is None else list(hit)


def constants(r):
    rules = live._secret_rules()
    return {
        "max_safe_integer": live.MAX_SAFE_INTEGER,
        "secret_keys": sorted(rules["secret_keys"]),
        "secret_key_suffixes": list(rules["secret_suffixes"]),
        "non_secret_key_suffixes": list(rules["non_secret_suffixes"]),
        "env_config_keys": sorted(rules["env_keys"]),
        "env_config_key_suffixes": list(rules["env_suffixes"]),
        "env_config_key_prefixes": list(rules["env_prefixes"]),
        "non_secret_paths": sorted(list(p) for p in rules["non_secret_paths"]),
        "placeholder_pattern": rules["placeholder"].pattern,
    }


DISPATCH = {
    "to_wire": to_wire,
    "wire_key": wire_key,
    "is_secret_path": is_secret_path,
    "secret_literal_path": secret_literal_path,
    "constants": constants,
}

SET_LIKE = ("secret_keys", "env_config_keys")


def normalise(func, value):
    """Compare the constant tables set-wise, as Python would."""
    if func == "constants" and isinstance(value, dict):
        value = dict(value)
        for key in SET_LIKE:
            if isinstance(value.get(key), list):
                value[key] = sorted(value[key])
        if isinstance(value.get("non_secret_paths"), list):
            value["non_secret_paths"] = sorted(value["non_secret_paths"])
    return value


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
            py = DISPATCH[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = normalise(r["func"], r.get("out"))
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
