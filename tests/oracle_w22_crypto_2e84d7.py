#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/qqbot/crypto.py
(gate w22_crypto_2e84d7).

Nothing is stubbed: the live generate_bind_key()/decrypt_secret() run, and the
result is compared as data plus the Python exception type they raise.

  bind_key       -- the live key's shape (byte length, char count, padding,
                    alphabet) since the key itself is random.
  b64_encode_hex -- base64.b64encode(bytes.fromhex(hex)); ValueError on a bad
                    hex string.
  b64_decode_len -- len(base64.b64decode(text)); binascii.Error with its two
                    details ("padding", "length_mod1").
  decrypt        -- the live plaintext, or the exception type plus the detail
                    the C side names: key_length / nonce_length / tag / utf8.
"""
import base64
import binascii
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "qqbot" / "crypto.py"
GATE = "w22_crypto_2e84d7"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("qqbot_crypto_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["qqbot_crypto_live"] = live
_spec.loader.exec_module(live)

WANTED = {"bind_key", "b64_encode_hex", "b64_decode_len", "decrypt"}

ALPHABET = set("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")


def bind_key(_r):
    key = live.generate_bind_key()
    raw = base64.b64decode(key)
    return {
        "bytes": len(raw),
        "chars": len(key),
        "padding": len(key) - len(key.rstrip("=")),
        "alphabet": all(c in ALPHABET or c == "=" for c in key),
    }


def b64_encode_hex(r):
    try:
        raw = bytes.fromhex(r.get("hex") or "")
    except ValueError:
        return {"ok": False, "error": "ValueError", "why": "hex"}
    return {"ok": True, "value": base64.b64encode(raw).decode("ascii")}


def b64_decode_len(r):
    try:
        raw = base64.b64decode(r.get("text") or "")
    except binascii.Error as exc:
        why = "padding" if "padding" in str(exc) else "length_mod1"
        return {"ok": False, "error": "binascii.Error", "why": why}
    return {"ok": True, "len": len(raw)}


def decrypt(r):
    try:
        value = live.decrypt_secret(r.get("enc") or "", r.get("key") or "")
    except binascii.Error as exc:
        why = "padding" if "padding" in str(exc) else "length_mod1"
        return {"ok": False, "error": "binascii.Error", "why": why}
    except UnicodeDecodeError:
        # UnicodeDecodeError is a ValueError subclass: catch it first.
        return {"ok": False, "error": "UnicodeDecodeError", "why": "utf8"}
    except ValueError as exc:
        text = str(exc)
        why = "nonce_length" if text.startswith("Nonce must be") else "key_length"
        return {"ok": False, "error": "ValueError", "why": why}
    except Exception as exc:  # noqa: BLE001  (cryptography.exceptions.InvalidTag)
        return {"ok": False, "error": type(exc).__name__, "why": "tag"}
    return {"ok": True, "value": value}


def expected(r):
    f = r.get("func")
    if f == "bind_key":
        return bind_key(r)
    if f == "b64_encode_hex":
        return b64_encode_hex(r)
    if f == "b64_decode_len":
        return b64_decode_len(r)
    if f == "decrypt":
        return decrypt(r)
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
