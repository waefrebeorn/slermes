#!/usr/bin/env python3
"""Generate tests/fixtures/route_identity_flow.jsonl for the
route_identity_flow FLOW + leaf port (src/cli/route_identity_flow.c).

Ten live targets: hermes_cli/route_identity.py (normalize_route_base_url,
should_clear_context_pin, should_clear_context_pin_async) + utils/urls.py
(_parse_base_url, _hostname_of, base_url_hostname, base_url_path,
base_url_origin, same_base_url_origin, base_url_host_matches). >=5 cases
per func; deterministic (no RNG).

Domain invariants enforced here (documented in the port header):
  - ASCII only, no NUL bytes (libjson C-string storage truncates at NUL).
  - No quote/backslash characters anywhere (bracketed-host / port ValueError
    messages embed repr(); the domain keeps repr == "'<x>'").
  - Integral numbers only (integral floats == ints; str()-positions render
    them as decimal ints).
  - str()-positions (normalize.base_url, pin model args) carry
    str/null/bool/int; utils strip()-positions carry any JSON scalar.
  - _hostname_of.parsed is null or a 6-string ParseResult projection.
  - async.args is an array; import_ok defaults true.
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "route_identity_flow.jsonl"

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def seam(val):
    return {"context_route_mismatch": val}


def raise_spec(kind, msg):
    return {"__raise__": kind, "message": msg}


# ---- normalize_route_base_url -------------------------------------------
N = "normalize_route_base_url"
for v in [None, "", 0, False, True, 5, -5]:
    add(N, base_url=v)
for u in [
    "http://H:80/", "https://H:443/x/", "http://H:8080/x/",
    "HTTP://U:***@Example.COM:80/a/?q#f",
    "http://h/", "http://h/?", "http://h?x", "http://h/#f",
    "http://h/path/", "http://h/path/?q",
    "http://[FE80::1%tESt]:1234/keys/", "http://[::1]:80/",
    "https://[::1]:443/", "http://[::1%z]:80/", "http://[V1.X]/",
    "http://[v1.x]:80/", "http://[::1]:8080", "http://[fe80::1%25eth0]/",
    "http://h:bad/", "http://[foo]/", "http://[1.2.3.4]/", "http://[::1/",
    "http://h:65536/", "http://h:8_0/", "http://h:99999/", "http://h:+80/",
    "http://h/a b", " http://h/", "http://h/\t", "http://ho\tst/",
    "http://h\x7f/", "\x7fhttp://h/", "http://\x7fh/", "http://h/x\x7f",
    "http://u@p@h:8080/", "http://h%41/", "http://h./", "http://h:0/",
    "http://h:00080/", "http://h:/", "http://h?", "http://h?#f",
    "http://h#f?x", "http://h/a?b#c/", "http://h//", "http://h///",
    "//h/x", "http:/h", "mailto:a@b", "http://h/a;b/", "http://h/a;b/?q",
    "http://h:80?x", "http://h:443/", "https://h:80/", "http://h:80#f",
    "http://h:000/", "http://@h/", "http://h@/", "http://[:***@h/",
    "http://h/[::1]/", "http://h%:80/", "https://h:0/", "http://h/a%3Fb/",
    "http://h/?q#f/", "http://h:80./", "http://[::1]:99999/",
    "http://h:0000000000000000000080/", "http://h:0000000000000000000099999/",
]:
    add(N, base_url=u)

# ---- should_clear_context_pin --------------------------------------------
S = "should_clear_context_pin"


def pin(cm, am, script=None, import_ok=True, cb="b1", ab="b2", cp="p1", ap="p2",
        io_extra=None):
    inp = {"configured_model": cm, "active_model": am,
           "configured_base_url": cb, "active_base_url": ab,
           "configured_provider": cp, "active_provider": ap}
    if script is not None:
        inp["io"] = seam(script)
    elif io_extra is not None:
        inp["io"] = io_extra
    if not import_ok:
        inp["import_ok"] = False
    add(S, **inp)


pin("m1", "m2")                                   # model mismatch -> True, no seam
pin("m", "m", False)                              # seam False forwarded
pin("m", "m", True)
pin("", "m2", False)                              # empty configured -> seam
pin(" m ", "m", False)                            # strip equality
pin(None, None, False)
pin(5, 5, False)                                  # str(5) == str(5)
pin("m", " m ", False)
pin("m1", None)                                   # "m1" != "" -> True
pin("m", "m", "RET")                              # raw string result
pin("m", "m", None)                               # None result
pin("m", "m", {"x": 1})                           # dict result
pin("m", "m", raise_spec("ValueError", "boom"))   # swallowed -> True
pin("m", "m", raise_spec("RuntimeError", "nope"))
pin("m", "m", raise_spec("SystemExit", "bye"))    # passthrough
pin("m", "m", raise_spec("KeyboardInterrupt", "ki"))
pin("m", "m", raise_spec("GeneratorExit", "ge"))
pin("m", "m", raise_spec("CancelledError", "ce"))
pin("m", "m", raise_spec("CustomKind", "ck"))     # Exception-derived -> True
pin("m", "m", import_ok=False)                    # halted import -> True
pin("m", "m", io_extra={})                        # fail-closed script -> True
pin("m", "m", [False])                            # queue semantics
pin("m", "m", False, cb=None, ab=None, cp=None, ap=None)  # null args drop
pin("m", "m", False, cb={"u": "x"}, ab=[1, 2], cp=7, ap=True)  # verbatim fwd
pin(True, "True", False)                          # str(True) == "True"
pin("m", "m", [True, False])                      # one call consumes first

# ---- should_clear_context_pin_async --------------------------------------
A = "should_clear_context_pin_async"
FULL = ["m", "m", "b1", "b2", "p1", "p2"]
add(A, args=FULL, io=seam(False))
add(A, args=["m1", "m2", "b1", "b2", "p1", "p2"])          # mismatch -> True
add(A, args=FULL, io=seam(raise_spec("ValueError", "boom")))
add(A, args=FULL, io=seam(raise_spec("KeyboardInterrupt", "ki")))
add(A, args=FULL, io=seam(raise_spec("CancelledError", "ce")))
add(A, args=FULL, io=seam(None))
add(A, args=FULL, io=seam("RET"))
add(A, args=FULL, import_ok=False)
add(A, args=FULL, io={})                                    # fail-closed
for k in range(6):                                        # missing 6..1
    add(A, args=FULL[:k])
add(A, args=FULL + ["extra"])                             # takes 6 but 7
add(A, args=FULL + ["e1", "e2"])                          # takes 6 but 8
add(A, args=[None, None, None, None, None, None], io=seam(False))
add(A, args=[5, 5, "b1", "b2", "p1", "p2"], io=seam(False))

# ---- _parse_base_url ------------------------------------------------------
P = "_parse_base_url"
for v in [None, "", "   ", False, 0]:
    add(P, base_url=v)
for u in ["api.openai.com", "api.openai.com:8080", "https://H:443/v1/",
          "http://h/a;b", "https://h/a;b/c", "h/path://x", "http://[foo]",
          "http://ho\tst/", "\x01http://h/", "https://h/x;y/z",
          "hdl://h/p;q", "https://h/;x", "//h/x", "http://h?x#f",
          "https://u:***@h:443/", "http://h:80/p"]:
    add(P, base_url=u)
add(P, base_url=5)                                        # AttributeError int
add(P, base_url=True)                                     # AttributeError bool

# ---- _hostname_of ----------------------------------------------------------
H = "_hostname_of"
add(H, parsed=None)
for arr in [
    ["", "", "", "", "", ""],
    ["https", "H:443", "/v1", "", "", ""],
    ["https", "H.", "/", "", "", ""],
    ["https", "H..", "/", "", "", ""],
    ["https", "[FE80::1%tESt]:1234", "/", "", "", ""],
    ["https", "u@p@H", "/", "", "", ""],
    ["", "h:80", "", "", "", ""],
    ["https", ".h", "/", "", "", ""],
    ["https", "[::1]", "/", "", "", ""],
    ["https", "h%41", "/", "", "", ""],
]:
    add(H, parsed=arr)

# ---- base_url_hostname -----------------------------------------------------
BH = "base_url_hostname"
for v in [None, "", "  ", False, 0]:
    add(BH, base_url=v)
for u in ["api.openai.com", "https://H:443/v1/", "https://h.", "http://h:bad",
          "http://[foo]", "http://ho\tst/", "  https://h/x  ",
          "https://[FE80::1%tESt]:1234/keys/", "\x1chttps://h/",
          "http://h\x0b:80/", "https://u@p@h/", "https://h..",
          "http://h\n:80/", "https://h:99999/"]:
    add(BH, base_url=u)
add(BH, base_url=5)
add(BH, base_url=True)

# ---- base_url_path ----------------------------------------------------------
BP = "base_url_path"
for v in [None, ""]:
    add(BP, base_url=v)
for u in ["https://H:443/v1/", "http://h/a;b", "https://h/a;b/c",
          "https://h/;x", "https://h/x;", "HTTPS://H/P;Q", "api.openai.com",
          "http://[foo]", "https://h/A/B/", "hdl://h/p;q",
          "https://h/a;b?x;y#f;g", "https://h/x;y/z", "https://h/",
          "https://h///"]:
    add(BP, base_url=u)
add(BP, base_url=5)

# ---- base_url_origin --------------------------------------------------------
BO = "base_url_origin"
for v in [None, "", False]:
    add(BO, base_url=v)
for u in ["api.openai.com", "api.openai.com:8080", "https://H:443/v1/",
          "https://h", "http://h", "http://h:bad", "https://h:99999/",
          "http://[foo]", "ftp://h/p", "socks5://h:1080", "https://h:0",
          "h:80", "https://[::1]:443/", "https://h:080", "http://h:8080",
          "https://h:65535", "https://h:65536",
          "https://h:00000000000000000000443"]:
    add(BO, base_url=u)
add(BO, base_url=5)
add(BO, base_url=True)

# ---- same_base_url_origin ----------------------------------------------------
SO = "same_base_url_origin"
for a, b in [("https://h", "https://h:443"), ("https://h", "http://h"),
             ("https://h:8443", "https://h:8443/x"), ("https://h", "https://g"),
             ("", "https://h"), ("https://h", ""),
             ("https://h:bad", "https://h"), ("https://h", "https://h:bad"),
             ("http://h", "http://h:80"), ("socks5://h", "socks5://h"),
             ("https://h", "http://[foo]"), ("", "http://[foo]"),
             ("http://[foo]", "https://h"), ("https://h:443", "https://h"),
             ("http://h:8080", "http://h:8081")]:
    add(SO, a=a, b=b)
add(SO, a=5, b="https://h")
add(SO, a=None, b=None)

# ---- base_url_host_matches ----------------------------------------------------
BM = "base_url_host_matches"
for u, d in [("https://api.openai.com/v1", "openai.com"),
             ("https://x.openai.com", "openai.com"),
             ("https://openai.com.evil.com", "openai.com"),
             ("https://openai.com", "openai.com."),
             ("https://openai.com", " OPENAI.COM "),
             ("https://openai.com", ""), ("https://openai.com", None),
             ("", "openai.com"), ("https://sub.h.com", "h.com"),
             ("https://h.com", "xh.com"), ("https://h:bad", "h"),
             ("https://h", 5), ("http://[foo]", 5),
             ("https://h.com", "h.com"), ("https://h", True),
             ("https://h.com", "COM."), ("https://h.com", ".com"),
             ("https://h", "h.")]:
    add(BM, base_url=u, domain=d)
add(BM, base_url=None, domain=None)
add(BM, base_url=5, domain="h")


def validate():
    funcs = {}
    for rec in CASES:
        f = rec["func"]
        funcs[f] = funcs.get(f, 0) + 1
        stack = [rec["in"]]
        while stack:
            v = stack.pop()
            if isinstance(v, dict):
                stack.extend(v.values())
            elif isinstance(v, list):
                stack.extend(v)
            elif isinstance(v, str):
                for c in v:
                    assert ord(c) < 128, "non-ASCII in %r" % v
                    assert c != "\x00", "NUL outside domain: %r" % v
                    assert c not in "'\"\\", "quote/backslash outside domain: %r" % v
            elif isinstance(v, float):
                assert v == int(v), "non-integral float outside domain"
    for f, n in funcs.items():
        assert n >= 5, "func %s has %d cases (<5)" % (f, n)
    return funcs


def main():
    funcs = validate()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=True) + "\n")
    print("wrote %s: %d cases" % (OUT, len(CASES)))
    for f in sorted(funcs):
        print("  %-32s %d" % (f, funcs[f]))


if __name__ == "__main__":
    main()