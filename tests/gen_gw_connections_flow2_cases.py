#!/usr/bin/env python3
"""Generate tests/fixtures/gw_connections_flow2.jsonl (>=5 cases / func).

Each line is {"func": <name>, "in": {...}}. Pure data, no quarry import.
"norm" is the injected normalize_remote_base_url result for _entry_url/_bound_url:
a string value, or {"__raise__":{"type","message"}} scripting an exception.
All strings ASCII; integral numbers have no decimal point.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gw_connections_flow2.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def rais(t, msg=""):
    return {"__raise__": {"type": t, "message": msg}}


def plain(v):
    return {"encoding": "plain", "value": v}


def safe(v):
    return {"encoding": "safeStorage", "value": v}


def reg(connections=None, quarantined=None, primary="local", lastUsed="local", launchMode="primary"):
    return {"version": 2, "primary": primary, "launchMode": launchMode,
            "lastUsed": lastUsed, "connections": connections or [],
            "quarantined": quarantined if quarantined is not None else []}


# ── _revision ─────────────────────────────────────────────────────────────
for doc in [{"revision": 0}, {"revision": 5}, {"revision": 3}, {},
            {"revision": "9"}, {"revision": True}, {"revision": None},
            {"revision": -2}, {"other": 1}]:
    case("revision", doc=doc)
case("revision", doc=None)

# ── _plain ────────────────────────────────────────────────────────────────
for s in ["raw-token", plain("v"), safe("blob"), "abc",
          {"encoding": "other", "value": "x"}, {"encoding": "plain"}, {}, 5, None]:
    case("plain", secret=s)

# ── _kept_envelope ────────────────────────────────────────────────────────
case("kept_envelope", token=safe("blob"), existing={"token": safe("blob")})
case("kept_envelope", token=safe("blob"), existing={"token": plain("x")})
case("kept_envelope", token=safe("blob"), existing=None)
case("kept_envelope", token=plain("x"), existing={"token": plain("x")})
case("kept_envelope", token=safe(""), existing={"token": safe("")})
case("kept_envelope", token="blob", existing={"token": "blob"})
case("kept_envelope", token=safe("blob"), existing={})

# ── _envelope ─────────────────────────────────────────────────────────────
for v in ["tok", "", "a b c", "123456789012", " leading "]:
    case("envelope", value=v)

# ── _token_preview ────────────────────────────────────────────────────────
for v in ["", "a", "12345678", "123456789", "abcdefghijklmnop", "  "]:
    case("token_preview", value=v)

# ── _public_connection ────────────────────────────────────────────────────
case("public_connection", entry={"id": "local", "kind": "local", "label": "Home"})
case("public_connection", entry={"id": "r1", "kind": "remote", "label": "Cloud",
                                 "token": plain("tok"), "headers": {"Authorization": "a", "X-A": "b"}})
case("public_connection", entry={"id": "r2", "label": "Safe", "token": safe("blob")})
case("public_connection", entry={"id": "r3", "label": "NoTok", "token": plain("abcdefghij")})
case("public_connection", entry={"id": "r4", "label": "HdrOnly", "headers": {"Z": "1", "A": "2"}})
case("public_connection", entry={"id": "r5", "label": "BadHdr", "headers": "not-a-dict", "token": 5})

# ── public_registry ───────────────────────────────────────────────────────
case("public_registry", reg=reg())
case("public_registry", reg=reg(connections=[
    {"id": "local", "kind": "local", "label": "Home", "token": plain("t")},
    {"id": "r1", "kind": "remote", "label": "Cloud", "token": plain("tok"),
     "headers": {"X-A": "v"}}]))
case("public_registry", reg=reg(quarantined=[
    {"reason": "dupe", "entry": {"label": "D"}},
    {"reason": None, "entry": {"label": "E"}},
    {"reason": 5, "entry": {"label": "F"}},
    {"entry": "not-a-dict"},
    {"reason": "x", "entry": {"label": 7}}]))
case("public_registry", reg=reg(primary="r1", lastUsed="r2", launchMode="ondemand"))
case("public_registry", reg=reg(connections=[{"id": "x", "label": "X", "token": safe("b")}],
                                quarantined=[]))

# ── _incoming_headers ─────────────────────────────────────────────────────
case("incoming_headers", raw={"X-Test": "abc", "Authorization": " secret ", "bad name": "v"},
     existing={})
case("incoming_headers", raw={"X-Keep": None, "X-Set": "new"},
     existing={"X-Keep": plain("old"), "X-Other": plain("o")})
case("incoming_headers", raw={"  Spaced  ": "  value  ", "": "drop", "X-Empty": "   "},
     existing={})
case("incoming_headers", raw={"X-Env": plain("verbatim"), "X-Obj": {"encoding": "safeStorage", "value": "s"}},
     existing={})
case("incoming_headers", raw={"X-Num": 5, "X-Bool": True, "X-Arr": [1], "X-None": None},
     existing={"X-None": plain("kept")})
case("incoming_headers", raw="not-a-dict", existing={"A": plain("a")})

# ── _signed_in_ids_gone ───────────────────────────────────────────────────
case("signed_in_ids_gone", before={"connections": []}, after={"connections": []})
case("signed_in_ids_gone",
     before={"connections": [{"id": "a", "url": "http://h", "authMode": "token"}]},
     after={"connections": [{"id": "a", "url": "http://h", "authMode": "token"}]})
case("signed_in_ids_gone",
     before={"connections": [{"id": "a", "url": "http://h"}, {"id": "b"}]},
     after={"connections": [{"id": "a", "url": "http://h"}]})
case("signed_in_ids_gone",
     before={"connections": [{"id": "a", "url": "http://one"}, {"id": "b", "url": "http://b"}]},
     after={"connections": [{"id": "a", "url": "http://two"}, {"id": "b", "url": "http://b"}]})
case("signed_in_ids_gone",
     before={"connections": [{"id": "a", "authMode": "token"}]},
     after={"connections": [{"id": "a", "authMode": "oauth"}]})
case("signed_in_ids_gone",
     before={"connections": [{"id": "a"}]},
     after={"connections": [{"id": "a"}]})

# ── _entry_url (seam + ConnectionStoreError catch) ───────────────────────
case("entry_url", entry={"url": "http://host/x"}, norm="http://host/x")
case("entry_url", entry={"url": "https://h/a"}, norm="https://h/a/")
case("entry_url", entry={"url": ""}, norm="http://ignored")
case("entry_url", entry={}, norm="http://ignored")
case("entry_url", entry={"url": "bad://x"}, norm=rais("ConnectionStoreError", "bad url"))
case("entry_url", entry={"url": "bad://x"}, norm=rais("DuplicateConnection", "dupe"))
case("entry_url", entry={"url": "x"}, norm=rais("ValueError", "boom"))
case("entry_url", entry={"url": "x"}, norm=rais("TypeError", "boom"))

# ── _bound_url (seam + ConnectionStoreError catch) ───────────────────────
case("bound_url", token_set={"baseUrl": "http://host/x"}, norm="http://host/x")
case("bound_url", token_set={"baseUrl": 5}, norm="http://ignored")
case("bound_url", token_set={}, norm="http://ignored")
case("bound_url", token_set={"baseUrl": "bad"}, norm=rais("ConnectionStoreError", "bad"))
case("bound_url", token_set={"baseUrl": "bad"}, norm=rais("NoSuchConnection", "gone"))
case("bound_url", token_set={"baseUrl": "x"}, norm=rais("RuntimeError", "boom"))
case("bound_url", token_set={"baseUrl": "x"}, norm=rais("ConnectionStoreBusy", "busy"))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    assert all(n >= 5 for n in counts.values()), counts
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()
