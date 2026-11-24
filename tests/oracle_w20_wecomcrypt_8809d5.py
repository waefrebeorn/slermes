#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/wecom/wecom_crypto.py
(w20_wecomcrypt_8809d5).

The live module runs for real: AES-256-CBC via `cryptography`, hashlib SHA-1,
base64 and the XML builder. Only gateway.platforms._shared is stubbed -- with
the same secrets_match logic and a timestamp_fresh pinned to the fixture's
`now`, so the 300s replay window is testable deterministically.

For `encrypt` the ciphertext is random by construction (os.urandom prefix), so
the compared value is the projection {signature, timestamp, nonce, xml_len} and
the harness's ciphertext is round-tripped through the live decrypt; that
decrypted plaintext must equal the fixture's.
"""
import hashlib
import hmac
import importlib.util
import json
import os
import sys
import types
import xml.etree.ElementTree as ET

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NOW = [0.0]


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _secrets_match(provided, expected):
    if not isinstance(provided, str) or not isinstance(expected, str) or not expected:
        return False
    return hmac.compare_digest(provided.encode("utf-8"), expected.encode("utf-8"))


def _timestamp_fresh(raw, window=300.0):
    try:
        return abs(NOW[0] - int(raw)) <= window
    except (TypeError, ValueError, OverflowError):
        return False



# cryptography loads for real from the user-writable sitecopy (the same wheels
# inside the WindowsApps payload are refused there: "DLL load failed while
# importing _rust: Access is denied").
_mk("gateway")
_mk("gateway.platforms")
_mk("gateway.platforms._shared", secrets_match=_secrets_match,
    timestamp_fresh=_timestamp_fresh)

NAME = "w20_wecomcrypt_8809d5_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "platforms", "wecom", "wecom_crypto.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def _raised(exc):
    return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}


def _ctx(spec):
    return live.WXBizMsgCrypt(spec.get("token"), spec.get("key"), spec.get("receive_id"))


def expected(r):
    f = r.get("func")
    spec = r.get("spec") if isinstance(r.get("spec"), dict) else {}
    try:
        if f == "init":
            c = _ctx(spec)
            return {"status": "ok", "value": {"key_len": len(c.key), "iv_len": len(c.iv)}}
        if f == "sha1":
            return {"status": "ok",
                    "value": live._sha1_signature(spec.get("token", ""),
                                                  spec.get("timestamp", ""),
                                                  spec.get("nonce", ""),
                                                  spec.get("encrypt", ""))}
        if f == "pkcs7_encode":
            raw = bytes.fromhex(spec.get("text") or "")
            out = live.PKCS7Encoder.encode(raw)
            return {"status": "ok", "value": {"hex": out.hex(), "len": len(out)}}
        if f == "pkcs7_decode":
            raw = bytes.fromhex(spec.get("hex") or "")
            out = live.PKCS7Encoder.decode(raw)
            return {"status": "ok", "value": {"hex": out.hex(), "len": len(out)}}
        if f == "decrypt":
            NOW[0] = float(spec.get("now") or 0.0)
            c = _ctx(spec)
            xml = c.decrypt(spec.get("msg_signature", ""), spec.get("timestamp", ""),
                            spec.get("nonce", ""), spec.get("encrypt", ""))
            return {"status": "ok", "value": xml.decode("utf-8")}
        if f == "encrypt":
            c = _ctx(spec)
            xml = c.encrypt(spec.get("plaintext", ""), nonce=spec.get("nonce"),
                            timestamp=spec.get("timestamp"))
            root = ET.fromstring(xml)
            return {"status": "ok",
                    "value": {"signature": root.find("MsgSignature").text,
                              "timestamp": root.find("TimeStamp").text,
                              "nonce": root.find("Nonce").text,
                              "xml_len": len(xml)}}
    except Exception as exc:  # noqa: BLE001 - the harness reports raised exceptions verbatim
        return _raised(exc)
    raise AssertionError("unknown func %r" % (f,))


# the ciphertext (and therefore its signature) is random by construction, so
# only these are comparable; the signature is re-derived from the harness output
ENCRYPT_KEYS = ("timestamp", "nonce", "xml_len")

cases = mism = mism_detail = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")

    if r.get("func") == "encrypt" and isinstance(got, dict) and exp.get("status") == "ok" \
            and got.get("status") == "ok":
        # the ciphertext is random: compare the deterministic projection and
        # round-trip the harness ciphertext through the live decrypt
        proj = {k: got["value"].get(k) for k in ENCRYPT_KEYS}
        want = {k: exp["value"].get(k) for k in ENCRYPT_KEYS}
        spec = r.get("spec") or {}
        try:
            NOW[0] = float(spec.get("now") or 0.0)
            c = live.WXBizMsgCrypt(spec.get("token"), spec.get("key"), spec.get("receive_id"))
            round_trip = c.decrypt(got["value"].get("signature", ""),
                                   got["value"].get("timestamp", ""),
                                   got["value"].get("nonce", ""),
                                   got["value"].get("encrypt", "")).decode("utf-8")
        except Exception as exc:  # noqa: BLE001
            round_trip = "ROUNDTRIP-ERROR: %r" % (exc,)
        expect_sig = live._sha1_signature(spec.get("token", ""), spec.get("timestamp", ""),
                                          spec.get("nonce", ""), got["value"].get("encrypt", ""))
        if proj != want:
            mism += 1
            print("MISMATCH encrypt case=%s py=%r c=%r" % (r.get("case"), want, proj),
                  file=sys.stderr)
        elif got["value"].get("signature") != expect_sig:
            mism += 1
            print("MISMATCH encrypt case=%s signature=%r want=%r"
                  % (r.get("case"), got["value"].get("signature"), expect_sig), file=sys.stderr)
        elif round_trip != spec.get("plaintext", ""):
            mism += 1
            print("MISMATCH encrypt case=%s roundtrip=%r want=%r"
                  % (r.get("case"), round_trip, spec.get("plaintext", "")), file=sys.stderr)
        continue

    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_wecomcrypt_8809d5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
