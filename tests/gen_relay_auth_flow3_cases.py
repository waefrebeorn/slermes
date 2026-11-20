#!/usr/bin/env python3
"""Generate tests/fixtures/relay_auth_flow3.jsonl — scripted-seam FLOW cases for
the standalone relay_auth_flow3 port of the LIVE quarry gateway/relay/auth.py.

Every case is {"func": <harness name>, "in": {... inputs ..., "seams": {...}}}.
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises a REAL builtin of that class on
the live side (full py3.14 parent hierarchy, incl. the BaseException trio) and
is delivered as a raise dict to the C universal seam. A MISSING seam key
fail-closes with RuntimeError on BOTH sides; a present JSON null is a valid
None. An object {"__arr__": [...]} is an explicit array VALUE, not a queue.

Seam call order (transcribed from live):
  sign              : hmac_hexdigest
  make_token(ttl>0) : time_time, hmac_hexdigest, b64url_encode
  make_token(ttl<=0): hmac_hexdigest, b64url_encode
  make_upgrade_token: identical to make_token (default ttl=300)

Domain: ASCII strings, JSON integral numbers for ttl/time, hex strings for the
hmac result, base64 strings (with possible '=' padding) for b64url_encode.
Non-string secrets/payloads, non-numeric time and non-ASCII bytes are outside
the fixture domain (documented in the port header).

Run: python tests/gen_relay_auth_flow3_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "relay_auth_flow3.jsonl")

CASES = []


def add(func, payload, secret, seams, **extra):
    c = {"func": func, "in": {"payload": payload, "secret": secret, "seams": seams}}
    c["in"].update(extra)
    CASES.append(c)


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def base(hexd="ab12cd34", enc="QUJDRA==", tm=1700000000.9):
    return {"time_time": tm, "hmac_hexdigest": hexd, "b64url_encode": enc}


def with_seam(name, value, **kw):
    s = base(**kw)
    s[name] = value
    return s


# ══════════════ sign (hmac_hexdigest only) ══════════════
add("sign", "hello", "s3cret", {"hmac_hexdigest": "a" * 64})
add("sign", "", "s3cret", {"hmac_hexdigest": ""})
add("sign", "gw-1", "", {"hmac_hexdigest": "0123456789abcdef"})
add("sign", "p:a|y", "k", {"hmac_hexdigest": ["deadbeef"]})          # array queue
add("sign", "p", "k", {"hmac_hexdigest": rs("ValueError", "bad hex")})
add("sign", "p", "k", {"hmac_hexdigest": rs("TypeError", "t")})
add("sign", "p", "k", {"hmac_hexdigest": rs("RuntimeError", "r")})
add("sign", "p", "k", {"hmac_hexdigest": rs("KeyError", "k")})
add("sign", "p", "k", {"hmac_hexdigest": rs("FileNotFoundError", "f")})
add("sign", "p", "k", {"hmac_hexdigest": rs("ZeroDivisionError", "z")})
add("sign", "p", "k", {"hmac_hexdigest": rs("ModuleNotFoundError", "m")})
add("sign", "p", "k", {"hmac_hexdigest": rs("UnicodeEncodeError", "u")})
add("sign", "p", "k", {"hmac_hexdigest": rs("ExceptionGroup", "e")})
add("sign", "p", "k", {"hmac_hexdigest": rs("Warning", "w")})
add("sign", "p", "k", {"hmac_hexdigest": rs("Exception", "x")})
add("sign", "p", "k", {"hmac_hexdigest": rs("SystemExit", "bye")})
add("sign", "p", "k", {"hmac_hexdigest": rs("KeyboardInterrupt", "")})
add("sign", "p", "k", {"hmac_hexdigest": rs("GeneratorExit", "")})
add("sign", "p", "k", {"hmac_hexdigest": rs("BaseException", "root")})

# ══════════════ make_token ══════════════
# ttl<=0 path: NO time_time event; exp = 0.
add("make_token", "gw-1", "sec", base(), ttl_seconds=0)
add("make_token", "gw-1", "sec", base(), ttl_seconds=-5)
add("make_token", "gw-1", "sec", base())                              # default 0
add("make_token", "gw-1", "sec", with_seam("b64url_encode", "QUJDRA=="),
    ttl_seconds=0)                                                    # rstrip("=")
add("make_token", "gw-1", "sec", with_seam("hmac_hexdigest", "cafe"), ttl_seconds=0)
# ttl>0 path: time_time, then hmac, then b64.
add("make_token", "gw-1", "sec", base(), ttl_seconds=60)
add("make_token", "gw-1", "sec", base(tm=-0.5), ttl_seconds=1)       # int trunc
add("make_token", "gw-1", "sec", base(tm=1700000000.0), ttl_seconds=300)
add("make_token", "gw-1", "sec", with_seam("hmac_hexdigest", ["h1", "h2"]),
    ttl_seconds=7)
add("make_token", "gw-1", "sec", with_seam("time_time", rs("ValueError", "clock")),
    ttl_seconds=5)
add("make_token", "gw-1", "sec", with_seam("hmac_hexdigest", rs("TypeError", "hm")),
    ttl_seconds=5)
add("make_token", "gw-1", "sec", with_seam("b64url_encode", rs("RuntimeError", "b64")),
    ttl_seconds=5)
add("make_token", "gw-1", "sec", with_seam("b64url_encode", rs("FileNotFoundError", "nf")),
    ttl_seconds=5)
add("make_token", "gw-1", "sec", with_seam("b64url_encode", rs("UnicodeEncodeError", "ue")),
    ttl_seconds=5)
add("make_token", "gw-1", "sec", with_seam("b64url_encode", rs("ExceptionGroup", "eg")),
    ttl_seconds=5)
add("make_token", "gw-1", "sec", with_seam("b64url_encode", rs("SystemExit", "bye")),
    ttl_seconds=5)
add("make_token", "gw-1", "sec", with_seam("b64url_encode", rs("KeyboardInterrupt", "")),
    ttl_seconds=5)
add("make_token", "gw-1", "sec", with_seam("b64url_encode", rs("GeneratorExit", "")),
    ttl_seconds=5)
add("make_token", "gw-1", "sec", with_seam("b64url_encode", rs("BaseException", "root")),
    ttl_seconds=5)

# ══════════════ make_upgrade_token (composed; default ttl=300) ══════════════
add("make_upgrade_token", "gw-1", "sec", base())                       # default 300
add("make_upgrade_token", "gw-1", "sec", base(), ttl_seconds=0)
add("make_upgrade_token", "gw-1", "sec", base(), ttl_seconds=120)
add("make_upgrade_token", "gw-2", "sec", base(tm=1000.7), ttl_seconds=30)
add("make_upgrade_token", "gw-1", "sec", with_seam("b64url_encode", "QQ=="),
    ttl_seconds=60)                                                    # rstrip all '='
add("make_upgrade_token", "gw-1", "sec", with_seam("hmac_hexdigest", ["aa", "bb"]),
    ttl_seconds=1)
add("make_upgrade_token", "gw-1", "sec", with_seam("time_time", rs("ValueError", "clk")),
    ttl_seconds=9)
add("make_upgrade_token", "gw-1", "sec", with_seam("hmac_hexdigest", rs("TypeError", "hm")),
    ttl_seconds=9)
add("make_upgrade_token", "gw-1", "sec", with_seam("b64url_encode", rs("RuntimeError", "b64")),
    ttl_seconds=9)
add("make_upgrade_token", "gw-1", "sec", with_seam("b64url_encode", rs("SystemExit", "bye")),
    ttl_seconds=9)
add("make_upgrade_token", "gw-1", "sec", with_seam("b64url_encode", rs("KeyboardInterrupt", "")),
    ttl_seconds=9)
add("make_upgrade_token", "gw-1", "sec", with_seam("b64url_encode", rs("GeneratorExit", "")),
    ttl_seconds=9)
add("make_upgrade_token", "gw-1", "sec", with_seam("b64url_encode", rs("UnicodeEncodeError", "ue")),
    ttl_seconds=9)
add("make_upgrade_token", "gw-1", "sec", with_seam("b64url_encode", rs("ExceptionGroup", "eg")),
    ttl_seconds=9)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases to %s" % (len(CASES), OUT))


if __name__ == "__main__":
    main()
