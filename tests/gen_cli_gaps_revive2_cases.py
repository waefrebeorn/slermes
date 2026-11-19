#!/usr/bin/env python3
"""gen_cli_gaps_revive2_cases.py — regenerate tests/fixtures/cli_gaps_revive2.jsonl.

Branch-covering cases (>=5 per ported function) for the cli_gaps group-2
revival ports. Scripted seam results mirror exactly what the C harness and
the oracle's recording stubs consume. Run from the repo root:
  python tests/gen_cli_gaps_revive2_cases.py
"""
import hashlib
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "cli_gaps_revive2.jsonl"
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_type=None, message=""):
    s = {"__script__": True}
    if raise_type:
        s["raise"] = {"type": raise_type, "message": message}
    else:
        s["ret"] = ret
    return s


def sha(s):
    return hashlib.sha256(s.encode()).hexdigest()


# ── _is_stale ──
for gen, selfgen, slot in [(1, 1, "same"), (2, 1, "same"), (1, 1, "other"),
                           (1, 1, "absent"), (0, 0, "absent"), (5, 5, "same"),
                           (-1, 0, "same")]:
    add("_is_stale", generation=gen, self_generation=selfgen, slot=slot, io={})

# ── last_error ──
def le_case(path, entries, resolved):
    order = [resolved[path]]
    for e in entries:
        order.append(resolved[e["path"]])
        if resolved[e["path"]] == resolved[path]:
            break
    add("last_error", path=path, unavailable=entries, resolved=resolved,
        io={"resolve": order})

le_case("a", [{"path": "a", "error": "E1"}, {"path": "b", "error": "E2"}],
        {"a": "/r/a", "b": "/r/b"})
le_case("b", [{"path": "a", "error": "E1"}, {"path": "b", "error": "E2"}],
        {"a": "/r/a", "b": "/r/b"})
le_case("c", [{"path": "a", "error": "E1"}, {"path": "b", "error": "E2"}],
        {"a": "/r/a", "b": "/r/b", "c": "/r/c"})
le_case("a", [], {"a": "/r/a"})
le_case("a", [{"path": "x", "error": "EX"}, {"path": "y", "error": "EY"}],
        {"a": "/r/shared", "x": "/r/shared", "y": "/r/shared"})
le_case("raw/../a", [{"path": "a", "error": "E1"}], {"raw/../a": "/r/a", "a": "/r/a"})

# ── publish_health ──
add("publish_health", source="s1", path="p1", state="ok", states=[],
    io={"publish_runtime_status": None})
add("publish_health", source="s2", path="p2", state="ok",
    states=[{"source": "s1", "entries": [{"path": "p1", "state": "retrying"}]}],
    io={"publish_runtime_status": None})
add("publish_health", source="s1", path="p1", state="ok",
    states=[{"source": "s1", "entries": [{"path": "p1", "state": "unavailable"}]}],
    io={"publish_runtime_status": None})
add("publish_health", source="s1", path="p1", state="unavailable",
    states=[{"source": "s1", "entries": [{"path": "p1", "state": "ok"}]},
            {"source": "s2", "entries": [{"path": "p2", "state": "retrying"}]}],
    io={"publish_runtime_status": None})
add("publish_health", source="s1", path="p1", state="ok", states=[],
    io={"publish_runtime_status": script(raise_type="RuntimeError", message="boom")})
add("publish_health", source="s1", path="p1", state="ok",
    states=[{"source": "s1", "entries": [{"path": "p1", "state": "unavailable"}]},
            {"source": "s2", "entries": [{"path": "p2", "state": "retrying"}]}],
    io={"publish_runtime_status": None})
add("publish_health", source="s3", path="p3", state="unavailable",
    states=[{"source": "s1", "entries": [{"path": "p1", "state": "ok"}]}],
    io={"publish_runtime_status": None})

# ── lock_for ──
add("lock_for", path="/h/logs/a.log", locks={}, io={"Lock": "L1"})
add("lock_for", path="/h/logs/a.log", locks={"/h/logs/a.log": "L0"}, io={"Lock": "L1"})
add("lock_for", path="/h/logs/b.log", locks={"/h/logs/a.log": "L0"}, io={"Lock": ["L1", "L2"]})
add("lock_for", path="", locks={}, io={"Lock": "L1"})
add("lock_for", path="/x", locks={"/x": "LX", "/y": "LY"}, io={"Lock": "L9"})
add("lock_for", path="/new", locks={}, io={"Lock": script(raise_type="RuntimeError", message="no locks")})

# ── _turns ──
add("_turns", io={})
add("_turns", producer_turns=None, io={})
add("_turns", producer_turns={}, io={})
add("_turns", producer_turns={"a": None}, io={})
add("_turns", producer_turns={"a": {"bracket": 1}, "b": None}, io={})

# ── take ──
def take_case(bound, event_source, store, with_source=True):
    inp = {"bound": bound, "store": store, "io": {"_bound.get": bound}}
    if with_source:
        inp["event_source"] = event_source
    add("take", **inp)

take_case(None, "s1", "st1")
take_case({"taken": True, "source": "s1", "store": "st1"}, "s1", "st1")
take_case({"taken": False, "source": "s1", "store": "st1"}, "s2", "st1")
take_case({"taken": False, "source": "s1", "store": "st1"}, "s1", "st2")
take_case({"taken": False, "source": "s1", "store": "st1"}, "s1", "st1")
take_case({"taken": False, "source": None, "store": "st1"}, None, "st1", with_source=False)
take_case({"taken": False, "source": None, "store": None}, None, None)
take_case({"taken": False, "source": "s1", "store": "st1"}, None, "st1")

# ── filter_and_accumulate ──
def filt(text, buffer="", in_block=False, accumulated=""):
    add("filter_and_accumulate", text=text,
        think={"buffer": buffer, "in_block": in_block, "accumulated": accumulated},
        io={"append_accumulated": None})

filt("hello world")
filt("<think>secret</think>tail")
filt("before\n<think>x</think>after")
filt("text<thi")
filt("more</think>rest", in_block=True)
filt("no close tag here at all", in_block=True)
filt("a</think>b")
filt("<THINK>x</THINK>y")
filt("say <think> now")
filt("<思考>秘密</思考>ok")
filt("abc</thi", in_block=True)
filt("<thinking>x</thinking>z")
filt("ink>hide</think>out", buffer="<th")
filt("")
filt("  <think>x</think>y")
filt("line1\n  <reasoning>r</reasoning>line2", accumulated="prev\n")
filt("<think>x</think>y", accumulated="prev")
filt("a\nb <think>x</think>c")

# ── flush_think_buffer ──
def flush(buffer, in_block=False, accumulated=""):
    add("flush_think_buffer",
        think={"buffer": buffer, "in_block": in_block, "accumulated": accumulated},
        io={"append_accumulated": None})

flush("tail")
flush("")
flush("x", in_block=True)
flush("a</think>b")
flush("<think")
flush("  </thinking>  ", accumulated="acc")

# ── watchdog_interval_seconds ──
def wd(af_unix, socket, usec):
    reads = []
    if af_unix:
        reads.append(socket)
        if socket.strip():
            reads.append(usec)
    add("watchdog_interval_seconds", af_unix=af_unix,
        io={"environ.get": reads})

wd(True, "", "1000000")
wd(True, "/run/systemd/notify", "2500000")
wd(True, "  @abstract  ", "1000000")
wd(True, "/run/s", "abc")
wd(True, "/run/s", "")
wd(True, "/run/s", "0")
wd(True, "/run/s", "-5")
wd(True, "/run/s", "inf")
wd(True, "/run/s", "nan")
wd(True, "/run/s", "1_000_000")
wd(False, "/run/s", "1000000")
wd(True, "/run/s", " 500000 ")
wd(True, "/run/s", "1e6")
wd(True, "   ", "1000000")

# ── lag_tolerance ──
def lt(interval, field):
    add("lag_tolerance", self={"interval_seconds": interval, "lag_tolerance_seconds": field}, io={})

lt(2.0, {"kind": "null"})
lt(None, {"kind": "null"})
lt(0.2, {"kind": "null"})
lt(2.0, {"kind": "str", "value": "0.75"})
lt(2.0, {"kind": "str", "value": "-3"})
lt(2.0, {"kind": "str", "value": "abc"})
lt(2.0, {"kind": "str", "value": "inf"})
lt(2.0, {"kind": "str", "value": "nan"})
lt(2.0, {"kind": "bool", "value": True})
lt(2.0, {"kind": "num", "value": 0.05})
lt(0.0, {"kind": "null"})

# ── record_tick ──
def rt(config_enabled, interval, field, stopping, unhealthy, sched, now, notify=None):
    add("record_tick",
        self={"config_enabled": config_enabled, "interval_seconds": interval,
              "lag_tolerance_seconds": field, "stopping": stopping, "unhealthy": unhealthy},
        scheduled_at=sched, now=now, io={"notify": notify})

N = {"kind": "num"}
rt(True, 2.0, {"kind": "null"}, False, False, {"kind": "num", "value": 100}, {"kind": "num", "value": 100.1})
rt(True, 2.0, {"kind": "null"}, False, False, {"kind": "num", "value": 100}, {"kind": "num", "value": 101})
rt(False, 2.0, {"kind": "null"}, False, False, {"kind": "num", "value": 100}, {"kind": "num", "value": 100.1})
rt(True, None, {"kind": "null"}, False, False, {"kind": "num", "value": 100}, {"kind": "num", "value": 100.1})
rt(True, 2.0, {"kind": "null"}, True, False, {"kind": "num", "value": 100}, {"kind": "num", "value": 100.1})
rt(True, 2.0, {"kind": "null"}, False, True, {"kind": "num", "value": 100}, {"kind": "num", "value": 100.1})
rt(True, 2.0, {"kind": "null"}, False, False, {"kind": "num", "value": 100}, {"kind": "str", "value": "bad"})
rt(True, 2.0, {"kind": "null"}, False, False, {"kind": "num", "value": 100}, {"kind": "null"})
rt(True, 2.0, {"kind": "null"}, False, False, {"kind": "num", "value": 100}, {"kind": "num", "value": 99})
rt(True, 2.0, {"kind": "null"}, False, False, {"kind": "num", "value": 100}, {"kind": "num", "value": 100.5})
rt(True, 2.0, {"kind": "str", "value": "0.01"}, False, False, {"kind": "num", "value": 100}, {"kind": "num", "value": 100.02})
rt(True, 2.0, {"kind": "null"}, False, False, {"kind": "str", "value": "bad"}, {"kind": "num", "value": 100})

# ── evict_idle ──
def lease(sid, holder=None, locked=False, pending=0, last_used=0.0, acquired_at=0.0):
    return {"sid": sid, "id": "L-" + sid, "holder": holder, "locked": locked,
            "pending": pending, "last_used": last_used, "acquired_at": acquired_at}

def ev(leases, max_entries):
    add("evict_idle", leases=leases, max_entries=max_entries, io={"time.time": 0})

ev([lease("a", last_used=1), lease("b", last_used=2)], 3)
ev([lease("a", last_used=1), lease("b", last_used=2), lease("c", last_used=3)], 2)
ev([lease("a", holder="t1", last_used=1), lease("b", last_used=2), lease("c", last_used=3)], 2)
ev([lease("a", locked=True, last_used=1), lease("b", last_used=2)], 1)
ev([lease("a", pending=2, last_used=1), lease("b", last_used=2)], 1)
ev([lease("a", last_used=5), lease("b", last_used=5), lease("c", last_used=5)], 2)
ev([lease("a", holder="t1", last_used=1), lease("b", last_used=2), lease("c", last_used=3)], 1)
ev([], 1)

# ── rebind ──
def tok(sid="s1", released=False, lease_id="L-s1", tid="t1", owner="k1", gen=1):
    return {"id": tid, "released": released, "session_id": sid, "owner_key": owner,
            "generation": gen, "lease": lease_id}

def rb(token, leases, new_sid, now=1000.0, other_tokens=None):
    add("rebind", token=token, leases=leases, new_session_id=new_sid, now=now,
        other_tokens=other_tokens or [],
        io={"time.time": now, "logger.warning": None})

rb(None, [lease("s1", holder="t1")], "s2")
rb(tok(released=True), [lease("s1", holder="t1")], "s2")
rb(tok(), [lease("s1", holder="t1")], "")
rb(tok(), [lease("s1", holder="t1")], "s1")
rb(tok(), [lease("s1", holder="t2")], "s2",
   other_tokens=[{"id": "t2", "owner_key": "k2", "generation": 2}])
rb(tok(), [lease("s1", holder="t1"), lease("s2", holder="t2", last_used=9)], "s2",
   other_tokens=[{"id": "t2", "owner_key": "k2", "generation": 2}])
rb(tok(), [lease("s1", holder="t1"), lease("s2", last_used=9)], "s2")
rb(tok(), [lease("s1", holder="t1")], "s2")
rb(tok(lease_id="L-s2"), [lease("s1", holder="t1"), lease("s2", holder="t1")], "s2")

# ── release ──
def rel(token, leases, now=1000.0, other_tokens=None):
    add("release", token=token, leases=leases, now=now,
        other_tokens=other_tokens or [],
        io={"time.time": now, "logger.debug": None, "lock.release": None})

rel(None, [lease("s1", holder="t1", locked=True)])
rel(tok(released=True), [lease("s1", holder="t1", locked=True)])
rel(tok(), [lease("s1", holder="t2", locked=True)],
    other_tokens=[{"id": "t2", "owner_key": "k2", "generation": 2}])
rel(tok(), [lease("s1", holder="t1", locked=True, acquired_at=500)])
rel(tok(), [lease("s1", holder="t1", locked=False, acquired_at=500)])
rel(tok(), [lease("s1", holder=None, locked=True)])

# ── probe_single_zai_endpoint ──
EP = ["coding-global", "https://api.z.ai/api/coding/paas/v4",
      ["glm-5.3", "glm-5.3-flash", "glm-4.7"], "Global (Coding Plan)"]

def probe(posts, models=None, key="sk-test", timeout=8.0):
    ep = list(EP)
    if models is not None:
        ep[2] = models
    add("probe_single_zai_endpoint", api_key=key, endpoint=ep, timeout=timeout,
        io={"httpx.post": posts, "logger.debug": None})

probe([{"status": 200}])
probe([{"status": 401}, {"status": 200}])
probe([{"status": 403}, {"status": 429}, {"status": 500}])
probe([script(raise_type="ConnectError", message="refused"), {"status": 200}])
probe([script(raise_type="TimeoutException", message="t/o"),
       script(raise_type="ConnectError", message="refused"),
       script(raise_type="ReadTimeout", message="slow")])
probe([], models=[])

# ── active_count ──
for n in [0, 1, 7, -1, 1000000]:
    add("active_count", active=n, io={})

# ── prepare ──
def prep(fence, now, idle_proof, token="tok-abc"):
    add("prepare", fence=fence, now=now,
        io={"idle_proof": idle_proof, "secrets.token_urlsafe": token,
            "logging.warning": None})

def fence(active=0, preparing=False, token=None, committed=False, expires_at=0.0):
    return {"active": active, "preparing": preparing, "token": token,
            "committed": committed, "expires_at": expires_at}

prep(fence(token="t", committed=True), 100, {"idle": True})
prep(fence(preparing=True), 100, {"idle": True})
prep(fence(token="t", expires_at=200), 100, {"idle": True})
prep(fence(active=2), 100, {"idle": True})
prep(fence(), 100, {"idle": True})
prep(fence(), 100, {"idle": False})
prep(fence(), 100, script(raise_type="RuntimeError", message="probe down"))
prep(fence(), 100, {"idle": 1})
prep(fence(), 100, {"idle": "yes"})
prep(fence(token="old", expires_at=50), 100, {"idle": True})
prep(fence(), 100, {})
prep(fence(token="t", committed=True, expires_at=10), 100, {"idle": True})
prep(fence(), 100, None)

# ── get_custom_provider_entries ──
def cpe(entries, load=None):
    add("get_custom_provider_entries",
        io={"load_config": load if load is not None else {},
            "get_compatible_custom_providers": entries})

cpe([{"name": "My Provider", "provider_key": "mp", "base_url": "http://x"}])
cpe([{"name": "   ", "provider_key": "x"}])
cpe(["not-a-dict", 42, None])
cpe([{"name": 123}])
cpe([{"name": "A", "provider_key": None}])
cpe([{"name": "A", "provider_key": 42}])
cpe([], load=script(raise_type="RuntimeError", message="config broken"))
cpe([])
cpe([{"name": "  Foo Bar  ", "provider_key": "  pk  "}, {"name": "B"}])

# ── is_current ──
def ic(recorded, token):
    add("is_current", recorded=recorded, io={"current": token})

ic(sha("cred-1"), "cred-1")
ic("wrong", "cred-1")
ic(None, "cred-1")
ic(sha(""), "")
ic(sha("思考-token"), "思考-token")
ic("", "cred-1")

# ── rotate ──
def rot(credential, revokers, script_vals):
    add("rotate", credential=credential, revokers=revokers,
        io={"set_session_token": None, "revoker": script_vals})

rot("new-cred", {}, [])
rot("new-cred", {"a": 3}, [3])
rot("new-cred", {"a": 1, "b": 0}, [1, 0])
rot("new-cred", {"a": 1}, [script(raise_type="RuntimeError", message="revoker down")])
rot("c2", {"x": 2, "y": 5, "z": 0}, [2, 5, 0])

# ── _fail ──
for msg in ["plain failure", "[bold red]x[/]", "", "unicode — 思考", "line\nbreak"]:
    add("_fail", message=msg, io={"print": None})

# ── _confirm ──
def conf(inp):
    add("_confirm", prompt="Proceed?", io={"input": inp, "print": None})

conf(script(ret="y"))
conf(script(ret="YES"))
conf(script(ret=" Yes "))
conf(script(ret="n"))
conf(script(ret=""))
conf(script(ret="yeah"))
conf(script(raise_type="EOFError"))
conf(script(raise_type="KeyboardInterrupt"))

# ── process_rows ──
def proc(pid="p1", command="echo hi", exit_code=None, started_at=100.0,
         exited_at=0.0, output_buffer=""):
    return {"id": pid, "command": command, "exit_code": exit_code,
            "started_at": started_at, "exited_at": exited_at,
            "output_buffer": output_buffer}

def entry(sid="p1", status="running", reason=None, exit_code=None):
    return {"session_id": sid, "status": status, "completion_reason": reason,
            "exit_code": exit_code}

def rows(entries, procs, now=150.0, label="goal"):
    io = {"registry.list_sessions": entries, "registry.get": procs, "t": label}
    if now is None:
        io["time.time"] = 150.0
    add("process_rows", now=now, io=io)

rows([entry()], [proc(started_at=100, output_buffer="out line")])
rows([entry(status="exited", exit_code=0)], [proc(exit_code=0, exited_at=140)])
rows([entry(status="exited", exit_code=0)], [proc(exit_code=0, exited_at=80)])
rows([entry()], [None])
rows([entry(status="exited", reason="killed", exit_code=0)],
     [proc(exit_code=0, exited_at=140)])
rows([entry(status="exited", reason="lost", exit_code=0)],
     [proc(exit_code=0, exited_at=140)])
rows([entry(status="exited", exit_code=2)], [proc(exit_code=2, exited_at=140)])
rows([entry("a"), entry("b"), entry("c", status="exited", exit_code=0)],
     [proc("a", started_at=140), proc("b", started_at=100),
      proc("c", exit_code=0, exited_at=145)])
rows([entry()], [proc(command="   ")])
rows([entry()], [proc()], now=None)
rows([entry(status="exited", exit_code=0)], [proc(exit_code=0, exited_at=0)])
rows([entry()], [proc(output_buffer="first\n\n  last   line  \n\n")])
rows([entry()], [proc(started_at=200)])
rows([], [])
rows([entry(status="exited", reason="other", exit_code=0)],
     [proc(exit_code=0, exited_at=149)])
rows([entry()], [proc(started_at=100, exited_at=999)])

# ── process_activity ──
def act(row, t_scripts):
    add("process_activity", row=row, io={"t": t_scripts})

act({"status": "running", "elapsed": 5, "detail": "out", "exit_code": None, "since_exit": 0},
    ["last output: out"])
act({"status": "running", "elapsed": 0, "detail": "", "exit_code": None, "since_exit": 0},
    ["starting…"])
act({"status": "done", "elapsed": 10, "detail": "", "exit_code": 0, "since_exit": 10},
    ["exit 0", "exit 0 · 10s ago"])
act({"status": "failed", "elapsed": 10, "detail": "", "exit_code": 1, "since_exit": 3},
    ["exit 1", "exit 1 · 3s ago"])
act({"status": "killed", "elapsed": 10, "detail": "", "exit_code": 0, "since_exit": 7},
    ["killed", "killed · 7s ago"])
act({"status": "lost", "elapsed": 10, "detail": "", "exit_code": None, "since_exit": 2},
    ["lost", "lost · 2s ago"])
act({"status": "bogus", "elapsed": 1, "detail": "", "exit_code": 0, "since_exit": 0}, [])
act({"status": "running", "elapsed": 12, "detail": "x", "exit_code": None, "since_exit": 0},
    ["last output: x"])

# ── process_tail ──
def tail(pid, proc_script, t="no output"):
    add("process_tail", process_id=pid, io={"registry.get": proc_script, "t": t})

tail("p1", {"output_buffer": "hello\nworld"})
tail("p1", {"output_buffer": "a\x01b\x07c\nd\te"})
tail("p1", {"output_buffer": ""}, t="no output yet")
tail("p1", {"output_buffer": None}, t="no output yet")
tail("p1", None, t="process gone")
tail("p1", {"output_buffer": "x" * 40000 + "TAIL"})
tail("p1", {"output_buffer": "思考\u2028ok\x7f"})

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
assert all(v >= 5 for v in counts.values()), counts
print(f"wrote {len(CASES)} cases across {len(counts)} functions to {OUT}")