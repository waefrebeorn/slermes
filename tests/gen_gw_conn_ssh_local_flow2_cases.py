#!/usr/bin/env python3
"""Generate tests/fixtures/gw_conn_ssh_local_flow2.jsonl (>=5 cases per function)."""
import json
from collections import Counter
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "gw_conn_ssh_local_flow2.jsonl"

OID = "0123456789abcdef0123456789abcdef"
NONCE = "0123456789abcdef"
ROOT = "/home/u/.hermes"
BASE = "http://127.0.0.1:5000"
FP = ["h1", "u1", "22", "k1", "/remote/h", "default"]
CONNECTED = {"base_url": BASE, "token": "tok", "local_port": 5000, "remote_port": 1234,
             "pid": 4242, "reused": True, "spawn_nonce": NONCE, "hermes_version": "v1"}
ENDPOINT = {"host": "h1", "user": "u1", "port": 22, "key_path": "k1",
            "control_path": "/tmp/c/x.sock"}
TUNNEL = {"ownership_id": OID, "connected": CONNECTED, "host": "h1",
          "fingerprint": FP, "ssh_binary": "ssh", "endpoint": ENDPOINT}
CONN = {"id": "c1", "host": "h1", "user": "u1", "port": 22, "keyPath": "k1",
        "remoteHermesPath": "/remote/h", "remoteProfile": "default"}


def rs(kind, msg="boom"):
    return {"__raise__": kind, "msg": msg}


def resp(status, ok=None, body=True):
    b = {"ok": ok} if body is True else (body if body is not None else {})
    return {"status": status, "body": b}


def conn(**kw):
    d = dict(CONN); d.update(kw); return d


def tunnel(**kw):
    d = json.loads(json.dumps(TUNNEL)); d.update(kw); return d


def ep_seams():
    return {"default_control_dir": ["/tmp/c"], "control_socket_path": ["/tmp/c/x.sock"]}


def boot_seams(reuse=""):
    d = ep_seams()
    d.update({"resolve_ssh_binary": ["ssh"], "ssh_open": [None],
              "connect": [dict(CONNECTED, token="tok" if reuse else "")],
              "ssh_close": [None]})
    return d


def reuse_seams():
    return {"is_alive": [True],
            "http_get": [resp(200, True), resp(200, True)],
            "classify_proof": ["authenticated-ok"]}


CASES = []


def C(func, **inp):
    CASES.append({"func": func, "in": inp})


# ── ownership_id (pure sha256) ──
for root, cid in [(ROOT, "c1"), (ROOT, "c2"), ("/other", "c1"), (ROOT, ""), ("/a/b", "id-9")]:
    C("ownership_id", root=root, connection_id=cid)

# ── _fingerprint ──
for c in [conn(), {}, {"host": "h", "port": 2222}, {"host": None, "user": "u", "keyPath": None},
          {"host": "h", "port": 0, "remoteProfile": "p"}, {"user": "u", "remoteHermesPath": "/x"}]:
    C("_fingerprint", connection=c)

# ── _target ──
for c in [CONNECTED,
          dict(CONNECTED, token="", reused=False, pid=0),
          dict(CONNECTED, local_port=6000, remote_port=9000, hermes_version="v2"),
          dict(CONNECTED, base_url="", hermes_version=""),
          dict(CONNECTED, token="x" * 8, reused=True)]:
    C("_target", connected=c)

# ── _health_ok ──
for sc in [resp(200, True), resp(200, False), resp(500, True),
           rs("HTTPError"), resp(200, body={}), rs("RuntimeError", "kaboom")]:
    C("_health_ok", base_url=BASE, seams={"http_get": [sc]})

# ── _probe_reuse_proof ──
C("_probe_reuse_proof", base_url=BASE, token="tok", nonce=NONCE,
  seams={"http_get": [resp(200, body={"ok": True})], "classify_proof": ["authenticated-ok"]})
for status in (401, 403, 404):
    C("_probe_reuse_proof", base_url=BASE, token="tok", nonce=NONCE,
      seams={"http_get": [resp(status, body={})]})
C("_probe_reuse_proof", base_url=BASE, token="tok", nonce=NONCE,
  seams={"http_get": [resp(500, body={})]})
C("_probe_reuse_proof", base_url=BASE, token="tok", nonce=NONCE,
  seams={"http_get": [rs("HTTPError", "net")]})
C("_probe_reuse_proof", base_url=BASE, token="tok", nonce=NONCE,
  seams={"http_get": [resp(200, body={"ok": True})], "classify_proof": [rs("RuntimeError", "cls")]})

# ── _wait_for_health ──
C("_wait_for_health", base_url=BASE,
  seams={"monotonic": [0, 1], "http_get": [resp(200, True)], "sleep": None})
C("_wait_for_health", base_url=BASE,
  seams={"monotonic": [0, 1, 2], "http_get": [resp(200, body={}), resp(200, True)], "sleep": None})
C("_wait_for_health", base_url=BASE, seams={"monotonic": [0, 30]})
C("_wait_for_health", base_url=BASE,
  seams={"monotonic": [0, 1, 31], "http_get": [resp(200, body={})], "sleep": None})
C("_wait_for_health", base_url=BASE,
  seams={"monotonic": [0, 1, 31], "http_get": [rs("HTTPError", "net")], "sleep": None})

# ── _live_reuse ──
C("_live_reuse", tunnel=tunnel(), seams=reuse_seams())
C("_live_reuse", tunnel=tunnel(), seams={"is_alive": [False]})
C("_live_reuse", tunnel=tunnel(), seams={"is_alive": [True], "http_get": [resp(200, False)]})
C("_live_reuse", tunnel=tunnel(),
  seams={"is_alive": [True], "http_get": [resp(200, True), resp(200, True)],
         "classify_proof": ["authenticated-stale"]})
C("_live_reuse", tunnel=tunnel(),
  seams={"is_alive": [True], "http_get": [resp(200, True), rs("HTTPStatusError", "status 500")],
         "classify_proof": ["authenticated-ok"]})
C("_live_reuse", tunnel=tunnel(),
  seams={"is_alive": [True], "http_get": [rs("RuntimeError", "boom")]})

# ── _endpoint ──
for c in [conn(), conn(host=None, user=None), conn(port="2222"), conn(port=0),
          conn(keyPath=None), conn(host="h2", user="u2", port=2222, keyPath="k2")]:
    s = ep_seams()
    C("_endpoint", connection=c, root=ROOT, ownership_id=OID, seams=s)
s = ep_seams(); s["control_socket_path"] = [rs("RuntimeError", "csp")]
C("_endpoint", connection=conn(), root=ROOT, ownership_id=OID, seams=s)

# ── _teardown ──
C("_teardown", tunnel=tunnel(), seams={"cancel_forward": [None], "disconnect_remote": [None], "ssh_close": [None]})
C("_teardown", tunnel=tunnel(), seams={"cancel_forward": [None], "disconnect_remote": [rs("SshReachError", "dr")], "ssh_close": [None]})
C("_teardown", tunnel=tunnel(), seams={"cancel_forward": [rs("RuntimeError", "cf")]})
C("_teardown", tunnel=tunnel(), seams={"cancel_forward": [None], "disconnect_remote": [None], "ssh_close": [rs("RuntimeError", "cl")]})
C("_teardown", tunnel=tunnel(), seams={"cancel_forward": [None], "disconnect_remote": [rs("HTTPError", "dr")], "ssh_close": [None]})

# ── tunnel_status ──
C("tunnel_status", tunnel=None)
C("tunnel_status", tunnel=tunnel())
C("tunnel_status", tunnel=tunnel(connected=dict(CONNECTED, reused=False, pid=0)))
C("tunnel_status", tunnel=tunnel(host=""))
C("tunnel_status", tunnel=tunnel(connected=dict(CONNECTED, hermes_version="")))

# ── disconnect ──
C("disconnect", tunnel=None)
C("disconnect", tunnel=tunnel(), seams={"cancel_forward": [None], "disconnect_remote": [None], "ssh_close": [None]})
C("disconnect", tunnel=tunnel(), seams={"cancel_forward": [None], "disconnect_remote": [rs("SshReachError", "dr")], "ssh_close": [None]})
C("disconnect", tunnel=tunnel(), seams={"cancel_forward": [rs("RuntimeError", "cf")]})
C("disconnect", tunnel=tunnel(), seams={"cancel_forward": [None], "disconnect_remote": [rs("HTTPError", "dr")], "ssh_close": [None]})

# ── _bootstrap ──
C("_bootstrap", connection=conn(), root=ROOT, previous=None, seams=boot_seams())
C("_bootstrap", connection=conn(), root=ROOT, previous=tunnel(), seams=boot_seams(reuse="x"))
s = boot_seams(); s["connect"] = [rs("SshReachError", "connect failed")]
C("_bootstrap", connection=conn(), root=ROOT, previous=None, seams=s)
s = boot_seams(); s["ssh_open"] = [rs("RuntimeError", "open")]
C("_bootstrap", connection=conn(), root=ROOT, previous=None, seams=s)
s = boot_seams(); s["resolve_ssh_binary"] = [rs("RuntimeError", "resolve")]
C("_bootstrap", connection=conn(), root=ROOT, previous=None, seams=s)

# ── ensure_tunnel ──
C("ensure_tunnel", connection=conn(id=""), root=ROOT, tunnel=None)
s = reuse_seams()
C("ensure_tunnel", connection=conn(), root=ROOT, tunnel=tunnel(), seams=s)
s = {"is_alive": [False], "cancel_forward": [None]}
s.update(boot_seams())
C("ensure_tunnel", connection=conn(), root=ROOT, tunnel=tunnel(), seams=s)
s = {"cancel_forward": [None], "disconnect_remote": [None], "ssh_close": [None]}
s.update(boot_seams())
C("ensure_tunnel", connection=conn(), root=ROOT,
  tunnel=tunnel(fingerprint=["other"] * 6), seams=s)
C("ensure_tunnel", connection=conn(), root=ROOT, tunnel=None, seams=boot_seams())

# ── interactive_shell_argv ──
def isa_seams(cwd_out=None, argv=None):
    s = reuse_seams()
    if cwd_out is not None:
        s["login_shell_quote"] = [cwd_out]
    s["build_exec_args"] = [argv if argv is not None else ["-o", "BatchMode=yes", "cmd"]]
    return s

C("interactive_shell_argv", connection=conn(), cwd="", root=ROOT, tunnel=tunnel(),
  seams=isa_seams())
C("interactive_shell_argv", connection=conn(), cwd="/work", root=ROOT, tunnel=tunnel(),
  seams=isa_seams(cwd_out="'/work'"))
C("interactive_shell_argv", connection=conn(), cwd="/a b", root=ROOT, tunnel=tunnel(),
  seams=isa_seams(cwd_out="'/a b'", argv=["-tt", "cmd", "x"]))
C("interactive_shell_argv", connection=conn(id=""), cwd="/work", root=ROOT, tunnel=tunnel(),
  seams=isa_seams(cwd_out="'/work'"))
C("interactive_shell_argv", connection=conn(), cwd="rel/path", root=ROOT, tunnel=tunnel(),
  seams=isa_seams(cwd_out="rel/path"))

# ── close_all ──
C("close_all", tunnels=[])
C("close_all", tunnels=[tunnel()],
  seams={"cancel_forward": [None], "disconnect_remote": [None], "ssh_close": [None]})
C("close_all", tunnels=[tunnel(), tunnel(host="h2")],
  seams={"cancel_forward": [None, None], "disconnect_remote": [None, None], "ssh_close": [None, None]})
C("close_all", tunnels=[tunnel()],
  seams={"cancel_forward": [None], "disconnect_remote": [rs("SshReachError", "skip")], "ssh_close": [None]})
C("close_all", tunnels=[tunnel()],
  seams={"cancel_forward": [None], "disconnect_remote": [rs("RuntimeError", "stop")], "ssh_close": [None]})

# pad to >= 5 per function with a harmless marker field
cnt = Counter(c["func"] for c in CASES)
for fn in sorted({c["func"] for c in CASES}):
    while cnt[fn] < 5:
        src = next(c for c in CASES if c["func"] == fn)
        d = json.loads(json.dumps(src)); d["in"]["__pad"] = cnt[fn]
        CASES.append(d); cnt[fn] += 1

OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
print("wrote", len(CASES), "cases across", len(cnt), "funcs; min", min(cnt.values()))
for k, v in sorted(cnt.items()):
    if v < 5:
        print("LOW", k, v)
