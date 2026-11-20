#!/usr/bin/env python3
"""Generate tests/fixtures/gw_conn_normalize_flow2.jsonl (>=5 cases / func).

Each line is {"func": <name>, "in": {...}}. Pure data, no quarry import.
in.state is the caller-held carrier for the _Normalizer methods and the
*clean helpers; "clean" holds the dict those helpers mutate. All strings are
ASCII, numbers integral (or simple non-integral decimals), URLs are
scheme://[userinfo@]host[:port][/path].
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gw_conn_normalize_flow2.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def reg(connections=None, primary="local", lastUsed="local", launchMode="primary", quarantined=None):
    d = {"connections": connections or [], "primary": primary,
         "lastUsed": lastUsed, "launchMode": launchMode}
    if quarantined is not None:
        d["quarantined"] = quarantined
    return d


def conn(cid, kind, label, **kw):
    d = {"id": cid, "kind": kind, "label": label}
    d.update(kw)
    return d


# ── js_string / js_falsy / js_str ────────────────────────────────────────
for v in [None, True, False, 0, 3, "x", "", [1, None, "a"], {"a": 1}, 3.0, 2.5]:
    case("js_string", value=v)
for v in [None, False, True, 0, 5, "", "x", [], {}, 2.5]:
    case("js_falsy", value=v)
for v in [None, False, 0, "", "abc", [], {}, 7]:
    case("js_str", value=v)

# ── label_key / unique_label / label_slug / connection_id_for_label ────
for v in ["  Hello World  ", "MiXeD  Case", "", 5, None, "a-b_C"]:
    case("label_key", value=v)
for cand, taken in [("Work", []), ("work", ["work"]), ("Home", ["home", "home 2"]),
                    ("", []), ("A", ["a", "a 2", "a 3"]), ("X", ["x 2"])]:
    case("unique_label", candidate=cand, taken=taken)
for v in ["Hello, World!", "foo_bar", "!!!", "a" * 70, "A B", "  --x--  "]:
    case("label_slug", label=v)
for lab, taken in [("Home", []), ("Home", ["home", "home-2"]), ("local", []),
                   ("Work Box", ["work-box"]), ("!!!", ["connection"]), ("x", ["local"])]:
    case("connection_id_for_label", label=lab, taken=taken)

# ── norm_auth_mode / stored_auth_mode ───────────────────────────────────
for m in ["oauth", "token", None, 5, "OAUTH"]:
    case("norm_auth_mode", mode=m)
for kind, am, tok in [("cloud", "token", None), ("cloud", "oauth", None),
                      ("cloud", "token", "t"), ("remote", "oauth", "t"),
                      ("ssh", "token", None)]:
    case("stored_auth_mode", kind=kind, auth_mode=am, token=tok)

# ── _url_host ────────────────────────────────────────────────────────────
for u in ["http://Host:123/x", "http://[::1]/x", "https://example.com",
          "ftp://Foo", "http://user@HOST/p", "host"]:
    case("_url_host", url=u)

# ── normalize_remote_base_url ───────────────────────────────────────────
for raw in ["example.com/path", "http://host", "https://host:443/a/b/",
            "http://host:8080/a/b/", "https://[::1]:8443/x", "   ",
            "ftp://x/y", "http://host:abc/x", "http://host:99999/x",
            "http://user:pass@Host:1234/p?q#f"]:
    case("normalize_remote_base_url", raw=raw)

# ── host_label_from_base_url ────────────────────────────────────────────
for b in ["http://host:8080/x", "http://host/x", "ws://host:1234/x",
          "https://[::1]/x", "", "ftp://x/y", "http://host:abc/x", "host:9000"]:
    case("host_label_from_base_url", base_url=b)

# ── sanitize_header_value / normalize_remote_headers ───────────────────
for v in ["a\r\nb", "  x  ", None, 5, "clean", True]:
    case("sanitize_header_value", value=v)
for raw in [
    {"X-Test": "abc", "Authorization": "secret", "bad name": "v", "Host": "h"},
    {"X-Ok": {"encoding": "plain", "value": "v"}, "X-Safe": {"encoding": "safeStorage", "value": "s"}},
    {"X-Empty": "", "X-BadEnc": {"encoding": "bogus", "value": "v"}},
    "not-a-dict", {}, {"A-B": "yes", "te": "no", "x-hermes-session-token": "no"},
    {"UPGRADE": "v", "X_C": "ok", "dot.name": "ok"},
]:
    case("normalize_remote_headers", raw=raw)

# ── _split_ssh_host / _explicit_port ────────────────────────────────────
for h in ["user@host:2222", "[::1]:2200", "host", "a@b@c:22", "host:abc", ""]:
    case("_split_ssh_host", host=h)
for raw in [None, 22, "2222", "abc", 0, "007"]:
    case("_explicit_port", raw=raw)

# ── normalize_ssh_config ────────────────────────────────────────────────
for e in [
    {"mode": "ssh", "host": "user@myhost:2222"},
    {"mode": "ssh", "host": "SSH  myhost", "user": "bob", "keyPath": "/k"},
    {"mode": "remote", "host": "x"},
    {"mode": "ssh", "host": ""},
    {"mode": "ssh", "host": "[::1]:2200", "port": 3000, "remoteProfile": "dev-1"},
    {"mode": "ssh", "host": "h", "port": 22, "remoteProfile": "root"},
]:
    case("normalize_ssh_config", entry=e)

# ── _ssh_fields / _local_entry ──────────────────────────────────────────
for s in [{"mode": "ssh", "host": "h", "user": "u"}, {"mode": "ssh"}, {},
          {"mode": "ssh", "host": "h", "port": 22, "user": "u", "keyPath": "/k"},
          {"mode": "remote", "host": "h"}]:
    case("_ssh_fields", ssh=s)
for lab in [None, "My Device", "", "A", 5]:
    case("_local_entry", label=lab)

# ── _remote_like_clean / _ssh_clean ─────────────────────────────────────
for entry, kind in [
    ({"url": "https://h/x", "authMode": "oauth", "token": "t"}, "remote"),
    ({"url": "", "name": "N"}, "cloud"),
    ({"url": "h/x", "headers": {"X-A": "v"}, "name": "C", "org": "O"}, "cloud"),
    ({"url": "h", "authMode": "token"}, "remote"),
    ({"url": "https://h", "token": None}, "remote"),
]:
    case("_remote_like_clean", state={"clean": {"id": "c", "kind": kind, "label": "L"}},
         entry=entry, kind=kind)
for entry in [
    {"mode": "ssh", "host": "h", "user": "u"},
    {"host": "myhost:2222", "keyPath": "/k"},
    {"host": ""},
    {"mode": "ssh", "host": "h", "token": "t"},
    {"mode": "remote", "host": "h"},
]:
    case("_ssh_clean", state={"clean": {"id": "c", "kind": "ssh", "label": "L"}}, entry=entry)

# ── _Normalizer.quarantine / add / _add_entry ───────────────────────────
def nstate():
    return {"labels": [], "ids": [], "connections": [], "quarantined": []}
for reason, entry in [("why", {"a": 1}), ("r2", [1, 2]), ("r3", 5),
                      ("r4", None), ("r5", "text")]:
    case("quarantine", state=nstate(), reason=reason, entry=entry)
for item in [
    {"kind": "local", "label": "Here"},
    {"kind": "remote", "url": "h/x", "label": "R"},
    {"kind": "weird"}, [1, 2], 5, None,
    {"kind": "ssh", "host": "u@h:2222", "label": "S"},
    {"kind": "cloud", "url": "https://h", "label": "C"},
]:
    case("add", state=nstate(), item=item)
for entry in [
    {"kind": "local", "label": "Here"},
    {"kind": "local", "label": "Here"},  # duplicate id local -> collision path
    {"kind": "remote", "url": "h/x", "label": "R"},
    {"kind": "ssh", "host": "h:22", "label": "S"},
    {"kind": "unknown", "label": "U"},
]:
    case("_add_entry", state=nstate(), entry=entry)

# ── normalize_registry ──────────────────────────────────────────────────
for raw in [
    {}, "not-dict",
    {"connections": [{"kind": "local"}, {"kind": "remote", "url": "example.com:8080/x"}]},
    {"connections": [{"kind": "remote", "url": "h/x", "label": "A"},
                     {"kind": "remote", "url": "h/y", "label": "B"}],
     "primary": "r", "lastUsed": "local", "launchMode": "last-used"},
    {"connections": [{"kind": "ssh", "host": "u@h", "label": "S"}], "primary": "s"},
    {"connections": [{"kind": "weird"}, [1], 7]},
    {"connections": [{"kind": "local", "label": "L"}],
     "quarantined": [{"reason": "old", "entry": {"x": 1}}]},
    {"connections": [{"kind": "cloud", "url": "https://h", "label": "C", "name": "N"}]},
]:
    case("normalize_registry", raw=raw)

# ── _url_key / _ssh_key ─────────────────────────────────────────────────
for v in ["  HTTPS://H/x/  ", "http://a/b///", "", "X/Y", "  ", "A/B/"]:
    case("_url_key", value=v)
for c in [
    {"user": "U@H", "host": "H", "port": 2222, "remoteProfile": "dev"},
    {"host": "h"}, {"user": "u", "host": "h", "port": 22},
    {"user": None, "host": None, "remoteProfile": " x "},
    {"user": "u", "host": "h", "port": 2200, "remoteProfile": ""},
]:
    case("_ssh_key", conn=c)

# ── _check_label ────────────────────────────────────────────────────────
r1 = reg([conn("a", "remote", "Home", url="http://h")])
for label, iid, registry in [
    ("Work", "b", r1), ("home", "a", r1), ("home", "b", r1),
    ("", "b", r1), ("x" * 65, "b", r1), ("WORK", None, r1),
    (" Unique ", "z", reg([conn("a", "remote", "other")])),
]:
    case("_check_label", label=label, input_id=iid, registry=registry)

# ── _ssh_input / _remote_input ──────────────────────────────────────────
rssh = reg([conn("s1", "ssh", "Old", host="h1", user="u1", port=2200)])
for data, cid, registry in [
    ({"_label": "New", "host": "u@h2:2222"}, "s2", rssh),
    ({"_label": "Dup", "host": "u1@h1:2200"}, "s2", rssh),
    ({"_label": "Bad", "host": ""}, "s2", rssh),
    ({"_label": "Tok", "host": "h3", "token": "t"}, "s3", reg([])),
    ({"_label": "P", "host": "h4", "keyPath": "/k", "remoteProfile": "dev"}, "s4", reg([])),
]:
    case("_ssh_input", data=data, cid=cid, registry=registry)
rrem = reg([conn("r1", "remote", "Old", url="http://h1/x")])
for data, cid, kind, registry in [
    ({"_label": "New", "url": "http://h2/x"}, "r2", "remote", rrem),
    ({"_label": "Dup", "url": "http://h1/x/"}, "r2", "remote", rrem),
    ({"_label": "C", "url": "https://h3", "name": "N", "org": "O"}, "c2", "cloud", reg([])),
    ({"_label": "T", "url": "http://h4", "authMode": "oauth", "token": "t"}, "r3", "remote", reg([])),
    ({"_label": "H", "url": "h5", "headers": {"X-A": "v"}}, "r4", "remote", reg([])),
]:
    case("_remote_input", data=data, cid=cid, kind=kind, registry=registry)

# ── normalize_connection_input ──────────────────────────────────────────
rnc = reg([conn("r1", "remote", "Home", url="http://h1/x")])
for data in [
    {"label": "Local", "kind": "local"},
    {"label": "New", "kind": "remote", "url": "http://h2/x"},
    {"label": "Dup", "kind": "remote", "url": "http://h1/x"},
    {"label": "S", "kind": "ssh", "host": "u@h:2222"},
    {"label": "C", "kind": "cloud", "url": "https://h3", "name": "N"},
    {"label": "", "kind": "remote", "url": "h"},
    {"label": "X", "kind": "local", "id": "local"},
    {"label": "U", "kind": "weird"},
    {"label": "WithId", "kind": "remote", "url": "http://h9", "id": "custom"},
]:
    case("normalize_connection_input", data=data, registry=rnc)

# ── merge_connection_input / _same_url ──────────────────────────────────
for data, existing in [
    ({"kind": "remote", "url": "http://h"}, {"kind": "remote", "authMode": "oauth", "org": "O"}),
    ({"kind": "ssh", "host": "h"}, {"kind": "ssh", "user": "u", "port": 22, "host": "old"}),
    ({"kind": "remote", "url": "http://h"}, None),
    ({"kind": "remote", "url": "http://h"}, {"kind": "cloud", "org": "O"}),
    ({"kind": "cloud", "url": "http://h/"}, {"kind": "cloud", "url": "http://h", "name": "N"}),
    ({"kind": "cloud", "url": "http://other"}, {"kind": "cloud", "url": "http://h", "name": "N"}),
    ({"kind": "ssh", "host": ""}, {"kind": "ssh", "user": "u", "port": 2200}),
]:
    case("merge_connection_input", data=data, existing=existing)
for a, b in [("http://h/x/", "http://h/x"), ("http://a", "http://b"),
             ("https://h:443", "https://h"), ("bad://x", "http://y"), ("", "")]:
    case("_same_url", a=a, b=b)

# ── connection_dial_fields_changed / upsert / remove / require / setters ─
base = conn("r1", "remote", "Home", url="http://h", authMode="token")
for before, after in [
    (base, base), (base, dict(base, url="http://h2")),
    (base, dict(base, kind="ssh")), (base, dict(base, token="t")),
    (base, dict(base, headers={"X": "1"})),
    (conn("s", "ssh", "S", host="h"), conn("s", "ssh", "S", host="h", port=2200)),
]:
    case("connection_dial_fields_changed", before=before, after=after)
rr = reg([conn("l", "local", "Local"), conn("r1", "remote", "R1", url="http://h")])
for entry in [conn("r1", "remote", "R1-new", url="http://h2"),
              conn("r2", "remote", "R2", url="http://h3"), conn("l", "local", "Local"),
              conn("r3", "remote", "R3", url="http://h4"), conn("r1", "remote", "R1")]:
    case("upsert_connection", registry=rr, entry=entry)
for cid in ["r1", "l", "missing", "r1", "l"]:
    case("remove_connection", registry=reg(
        [conn("l", "local", "Local"), conn("r1", "remote", "R1", url="http://h")],
        primary="r1", lastUsed="r1"), cid=cid)
for cid in ["r1", "missing", "l", "r1", "missing"]:
    case("_require", registry=reg([conn("l", "local", "Local"), conn("r1", "remote", "R1")]), cid=cid)
for cid in ["r1", "missing", "l", "r1", "missing"]:
    case("set_primary_connection", registry=reg([conn("r1", "remote", "R1")]), cid=cid)
for cid in ["r1", "missing", "l", "r1", "missing"]:
    case("set_last_used_connection", registry=reg([conn("r1", "remote", "R1")]), cid=cid)
for mode in ["last-used", "primary", "bogus", "last-used", "primary"]:
    case("set_connection_launch_mode", registry=reg([]), mode=mode)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    assert all(v >= 5 for v in counts.values()), {k: v for k, v in counts.items() if v < 5}
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    print(json.dumps(counts, sort_keys=True))


if __name__ == "__main__":
    main()
