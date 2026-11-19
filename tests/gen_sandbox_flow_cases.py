#!/usr/bin/env python3
"""Generate tests/fixtures/sandbox_flow.jsonl for the sandbox_flow oracle.

Each case is {"func", "in"}; "in" carries the flow params, the JSON record
model (scope/identity/env/pipe_state/stream/method/provider — see the
src/tools/sandbox_flow.c header) and the "io" seam scripts (single value
reused, or non-empty array consumed per call with the last value repeating;
a MISSING key or EMPTY array is fail-closed on both sides). >=5 cases per
ported function, branch-covering: happy paths, refusal paths, attribute
errors, scripted raises (incl. the BaseException trio + StopIteration),
fail-closed seams, and the documented domain edges. ASCII only; bytes are
modeled as str; integral numbers are ints.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "sandbox_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def rais(kind, msg):
    return {"__raise__": kind, "msg": msg}


def scope(t="acme", a="a1"):
    return {"tenant": t, "agent": a}


A = scope()
B = scope("acme", "a2")
LOC = scope("local", "/h/p")
KEY = "sandbox:acme/a1"
KEY_B = "sandbox:acme/a2"


def ident(t="acme", a="a1"):
    return {"tenant": t, "agent": a}


def P(name, kind="pos_or_kw"):
    return {"name": name, "kind": kind}


def cdesc(*params):
    return {"callable": True, "params": list(params)}


def adesc(v):
    return {"callable": False, "value": v}


SCOPE_METHOD = cdesc(P("scope", "kw_only"))
NOSCOPE_METHOD = cdesc(P("x"))
KW_METHOD = cdesc(P("kw", "var_kw"))


def inner_full():
    return {
        "execute": cdesc(P("command"), P("scope", "kw_only")),
        "cleanup": SCOPE_METHOD,
        "start_process": cdesc(P("name"), P("argv"), P("child_env", "kw_only"), P("scope", "kw_only")),
        "attach_process": cdesc(P("name"), P("scope", "kw_only")),
        "stop_process": cdesc(P("name"), P("scope", "kw_only")),
        "stop_idle": SCOPE_METHOD,
        "fetch_file": SCOPE_METHOD,
        "fetch_realpath": SCOPE_METHOD,
        "get_temp_dir": SCOPE_METHOD,
        "cwd": adesc("/w"),
    }


def env(inner=None, sandbox=None, owner=None, inner_repr="namespace()"):
    return {"__scoped_env__": True, "inner": inner if inner is not None else {"cwd": adesc("/w")},
            "inner_repr": inner_repr, "sandbox": sandbox if sandbox is not None else ident(),
            "owner": owner}


def pdesc(methods=("read", "readline", "write", "flush", "close"), closed=None, has_closed=True):
    d = {"methods": list(methods)}
    if has_closed:
        d["closed"] = closed
    return d


def pst(pipe=None, identity=None):
    return {"identity": identity if identity is not None else ident(),
            "pipe": pipe if pipe is not None else pdesc()}


def stream(stdin=None, stdout=None, poll=True, has=("stdin", "stdout", "poll")):
    d = {}
    if "stdin" in has:
        d["stdin"] = stdin if stdin is not None else pdesc()
    if "stdout" in has:
        d["stdout"] = stdout if stdout is not None else pdesc()
    if "poll" in has:
        d["poll"] = poll
    return d


def pref(sandbox=KEY, name="proc"):
    return {"__process_ref__": True, "sandbox": sandbox, "name": name}


HS = {"current_scope": A}          # hosted scope bound
NOS = {"current_scope": None}      # no scope bound
LOCALS = {"current_scope": LOC}    # local install

# ── hosted_scope ──
add("hosted_scope", io=NOS)
add("hosted_scope", io=LOCALS)
add("hosted_scope", io=HS)
add("hosted_scope", io={"current_scope": scope("a c", "x/y")})
add("hosted_scope", io={})
add("hosted_scope", io={"current_scope": rais("SystemExit", "bye")})

# ── sandbox_identity ──
add("sandbox_identity", scope=A)
add("sandbox_identity", scope=LOC)
add("sandbox_identity", scope=None)
add("sandbox_identity", scope=5)
add("sandbox_identity", scope={"agent": "a"})
add("sandbox_identity", scope={"tenant": "t"})

# ── identity_key ──
add("identity_key", tenant="acme", agent="a1")
add("identity_key", tenant="a c", agent="x/y")
add("identity_key", tenant="", agent="")
add("identity_key", tenant="~_.-", agent="ABC123")
add("identity_key", tenant="a%b", agent="+")
add("identity_key", tenant="t", agent="a'b")

# ── current_sandbox_key ──
add("current_sandbox_key", io=HS)
add("current_sandbox_key", io=NOS)
add("current_sandbox_key", io=LOCALS)
add("current_sandbox_key", io={"current_scope": scope("a c", "x/y")})
add("current_sandbox_key", io={})
add("current_sandbox_key", io={"current_scope": rais("KeyboardInterrupt", "")})

# ── sandbox_visible ──
add("sandbox_visible", sandbox_key=KEY, io=HS)
add("sandbox_visible", sandbox_key=KEY_B, io=HS)
add("sandbox_visible", sandbox_key="", io=NOS)
add("sandbox_visible", sandbox_key="", io=HS)
add("sandbox_visible", sandbox_key=5, io=NOS)
add("sandbox_visible", sandbox_key=None, io=NOS)
add("sandbox_visible", sandbox_key=KEY, io={})

# ── sandbox_lexical_path ──
add("sandbox_lexical_path", path="a//b/./c", io=HS)
add("sandbox_lexical_path", path="/a/../b", io=HS)
add("sandbox_lexical_path", path="", io=HS)
add("sandbox_lexical_path", path={"__fspath__": "/x//y"}, io=HS)
add("sandbox_lexical_path", path="a", io=NOS)
add("sandbox_lexical_path", path=None, io=HS)
add("sandbox_lexical_path", path=5, io=HS)
add("sandbox_lexical_path", path="//a", io=HS)
add("sandbox_lexical_path", path="a", io={})

# ── producer_fields ──
add("producer_fields", sandbox_key="k")
add("producer_fields", sandbox_key="")
add("producer_fields", sandbox_key=None)
add("producer_fields", sandbox_key=0)
add("producer_fields", sandbox_key=False)
add("producer_fields", sandbox_key=[])
add("producer_fields", sandbox_key={"a": 1})
add("producer_fields", sandbox_key=5)

# ── event_visible ──
add("event_visible", event={"sandbox_key": KEY}, io=HS)
add("event_visible", event={"sandbox_key": "other"}, io=HS)
add("event_visible", event={}, io=HS)
add("event_visible", event={}, io=NOS)
add("event_visible", event={"sandbox_key": "x"}, io=NOS)
add("event_visible", event={"sandbox_key": 0}, io=HS)
add("event_visible", event={"sandbox_key": True}, io=HS)
add("event_visible", event=None, io=HS)
add("event_visible", event=5, io=HS)
add("event_visible", event=[], io=HS)

# ── require_identity ──
add("require_identity", identity=ident(), what="execute", io=HS)
add("require_identity", identity=ident("acme", "a2"), what="execute", io=HS)
add("require_identity", identity=ident(), what="cleanup", io=NOS)
add("require_identity", identity=ident(), what="cleanup", io=LOCALS)
add("require_identity", identity=None, what="poll", io=HS)
add("require_identity", identity=ident(), what="execute", io={"current_scope": rais("SystemExit", "x")})
add("require_identity", identity=ident(), what="execute", io={})

# ── admit_env ──
add("admit_env", env=None, io=HS)
add("admit_env", env=None, io=NOS)
add("admit_env", env=env(), io=HS)
add("admit_env", env=env(sandbox=ident("acme", "a2")), io=HS)
add("admit_env", env={"plain": 1}, io=HS)
add("admit_env", env={"plain": 1}, io=NOS)
add("admit_env", env=env(), io=NOS)
add("admit_env", env=env(), io={"current_scope": rais("GeneratorExit", "")})

# ── create_hosted_environment ──
CREATED = {"inner": inner_full(), "inner_repr": "namespace(cwd='/w')"}
add("create_hosted_environment", env_type="docker", provider={"serves_hosted_scopes": True},
    kwargs={"cwd": "/w", "timeout": 30}, io={"current_scope": A, "provider.create_environment": CREATED})
add("create_hosted_environment", env_type="docker", provider={"serves_hosted_scopes": True},
    kwargs={}, io=NOS)
add("create_hosted_environment", env_type="docker", provider=None, kwargs={}, io=HS)
add("create_hosted_environment", env_type="docker", provider={"serves_hosted_scopes": False}, kwargs={}, io=HS)
add("create_hosted_environment", env_type="docker", provider={"serves_hosted_scopes": 1}, kwargs={}, io=HS)
add("create_hosted_environment", env_type="docker", provider={}, kwargs={}, io=HS)
add("create_hosted_environment", env_type="docker", provider={"serves_hosted_scopes": True},
    kwargs={}, io={"current_scope": A, "provider.create_environment": {"inner": {"execute": SCOPE_METHOD}, "inner_repr": "namespace()"}})
add("create_hosted_environment", env_type="docker", provider={"serves_hosted_scopes": True},
    kwargs={}, io={"current_scope": A, "provider.create_environment": {"inner": dict(inner_full(), execute=NOSCOPE_METHOD), "inner_repr": "namespace()"}})
add("create_hosted_environment", env_type="docker", provider={"serves_hosted_scopes": True},
    kwargs={"scope": "x"}, io=HS)
add("create_hosted_environment", env_type="docker", provider={"serves_hosted_scopes": True},
    kwargs={"task_id": "caller-held"}, io={"current_scope": A, "provider.create_environment": CREATED})
add("create_hosted_environment", env_type="docker", provider={"serves_hosted_scopes": True},
    kwargs={}, io={"current_scope": A, "provider.create_environment": rais("RuntimeError", "boom")})
add("create_hosted_environment", env_type="docker", provider={"serves_hosted_scopes": True},
    kwargs={}, io={"current_scope": A, "provider.create_environment": None})

# ── accepts_scope ──
add("accepts_scope", method=cdesc())
add("accepts_scope", method=SCOPE_METHOD)
add("accepts_scope", method=cdesc(P("scope")))
add("accepts_scope", method=cdesc(P("scope", "pos_only")))
add("accepts_scope", method=KW_METHOD)
add("accepts_scope", method=cdesc(P("a", "var_pos")))
add("accepts_scope", method={"signature_error": True})
add("accepts_scope", method=None)
add("accepts_scope", method=5)
add("accepts_scope", method=cdesc(P("x", "pos_only"), P("scope", "kw_only")))

# ── require_hosted_interface ──
add("require_hosted_interface", inner=inner_full(), env_type="docker")
add("require_hosted_interface", inner=dict(inner_full(), cleanup=adesc(5)), env_type="docker")
add("require_hosted_interface", inner=None, env_type="docker")
add("require_hosted_interface", inner=dict(inner_full(), execute=NOSCOPE_METHOD), env_type="docker")
add("require_hosted_interface", inner=dict(inner_full(), fetch_file=NOSCOPE_METHOD), env_type="docker")
add("require_hosted_interface", inner=dict(inner_full(), cleanup=adesc(5), execute=NOSCOPE_METHOD), env_type="docker")
add("require_hosted_interface", inner=inner_full(), env_type="docker's")
add("require_hosted_interface", inner=5, env_type="local")

# ── scoped_call ──
add("scoped_call", identity=ident(), what="poll", args=[], kwargs={}, io={"current_scope": A, "fn": 0})
add("scoped_call", identity=ident("acme", "a2"), what="poll", args=[], kwargs={}, io=HS)
add("scoped_call", identity=ident(), what="poll", args=[], kwargs={}, io=NOS)
add("scoped_call", identity=ident(), what="poll", args=[1], kwargs={"k": "v"},
    io={"current_scope": A, "fn": rais("ValueError", "bad")})
add("scoped_call", identity=ident(), what="poll", args=["x", 2], kwargs={"a": None},
    io={"current_scope": A, "fn": "ok"})
add("scoped_call", identity=ident(), what="poll", args=[], kwargs={}, io={"current_scope": A})
add("scoped_call", identity=ident(), what="poll", args=[], kwargs={},
    io={"current_scope": rais("SystemExit", "stop")})

# ── env_init ──
add("env_init", inner={"cwd": adesc("/w")}, inner_repr="namespace()", identity=ident(), owner=None)
add("env_init", inner={}, inner_repr="namespace()", identity=ident(), owner=A)
add("env_init", inner={}, inner_repr="namespace()", identity=ident(), owner=B)
add("env_init", inner={}, inner_repr="namespace()", identity=None, owner=A)
add("env_init", inner=inner_full(), inner_repr="namespace(cwd='/w')", identity=ident(), owner=None)
add("env_init", inner={}, inner_repr="namespace()", identity=None, owner=None)

# ── env_getattr ──
add("env_getattr", env=env(), name="cwd")
add("env_getattr", env=env(inner_full()), name="execute")
add("env_getattr", env=env(inner_full()), name="cleanup")
add("env_getattr", env=env(inner_full()), name="stop_idle")
add("env_getattr", env=env(), name="nope")
add("env_getattr", env=env({"x": adesc(None)}), name="x")
add("env_getattr", env=env({"x": adesc({"a": 1})}), name="x")

# ── env_setattr ──
add("env_setattr", env=env(), name="cwd", value="/w2")
add("env_setattr", env=env({"cwd": adesc("/w")}), name="cwd", value=5)
add("env_setattr", env=env(), name="new", value=None)
add("env_setattr", env=env(), name="d", value={"a": 1})
add("env_setattr", env=env({}), name="x", value="y")
add("env_setattr", env=env(), name="q'uote", value=True)

# ── env_guarded ──
GE = {"current_scope": A, "inner.execute": "ran"}
add("env_guarded", env=env(), name="execute", method=cdesc(P("command"), P("scope", "kw_only")),
    args=["ls"], kwargs={}, io=GE)
add("env_guarded", env=env(), name="execute", method=NOSCOPE_METHOD, args=["v"], kwargs={}, io=GE)
add("env_guarded", env=env(), name="execute", method=KW_METHOD, args=[], kwargs={"a": 1}, io=GE)
add("env_guarded", env=env(), name="execute", method=SCOPE_METHOD, args=[], kwargs={"scope": "old"}, io=GE)
add("env_guarded", env=env(sandbox=ident("acme", "a2")), name="execute", method=SCOPE_METHOD,
    args=[], kwargs={}, io=HS)
add("env_guarded", env=env(), name="execute", method=SCOPE_METHOD, args=[], kwargs={}, io=NOS)
add("env_guarded", env=env(), name="execute", method=SCOPE_METHOD, args=[], kwargs={},
    io={"current_scope": A, "inner.execute": rais("RuntimeError", "inner boom")})
add("env_guarded", env=env(), name="execute", method=SCOPE_METHOD, args=[], kwargs={}, io={"current_scope": A})
add("env_guarded", env=env(), name="execute", method=cdesc(P("a", "var_pos")), args=[1, 2], kwargs={}, io=GE)
add("env_guarded", env=env(), name="execute", method=cdesc(P("scope", "pos_only")), args=["s"], kwargs={}, io=GE)

# ── env_lifecycle ──
add("env_lifecycle", env=env(), name="cleanup", method=SCOPE_METHOD, args=[], kwargs={},
    io={"current_scope": A, "inner.cleanup": None})
add("env_lifecycle", env=env(owner=A), name="cleanup", method=SCOPE_METHOD, args=[], kwargs={},
    io={"current_scope": [None, A], "bind_state": True, "inner.cleanup": None})
add("env_lifecycle", env=env(owner=None), name="cleanup", method=SCOPE_METHOD, args=[], kwargs={}, io=NOS)
add("env_lifecycle", env=env(owner=A), name="stop_idle", method=SCOPE_METHOD, args=[], kwargs={},
    io={"current_scope": [None, A], "bind_state": rais("RuntimeError", "bind boom")})
add("env_lifecycle", env=env(owner=A), name="cleanup", method=SCOPE_METHOD, args=[], kwargs={},
    io={"current_scope": [None, None], "bind_state": True})
add("env_lifecycle", env=env(sandbox=ident("acme", "a2")), name="cleanup", method=SCOPE_METHOD,
    args=[], kwargs={}, io=HS)
add("env_lifecycle", env=env(), name="cleanup", method=SCOPE_METHOD, args=[], kwargs={},
    io={"current_scope": rais("SystemExit", "halt")})
add("env_lifecycle", env=env(owner=B), name="cleanup", method=SCOPE_METHOD, args=[], kwargs={},
    io={"current_scope": [None, B], "bind_state": True, "inner.cleanup": "done"})

# ── env_start_process ──
SP = {"current_scope": A, "inner.start_process": stream()}
add("env_start_process", env=env(inner_full()), name="proc", argv=["sleep", "1"], child_env=None, io=SP)
add("env_start_process", env=env(inner_full()), name="proc", argv=["sh"], child_env={"K": "V"}, io=SP)
add("env_start_process", env=env(inner_full()), name="proc", argv=[], child_env=None,
    io={"current_scope": A, "inner.start_process": None})
add("env_start_process", env=env({}), name="proc", argv=[], child_env=None, io=HS)
add("env_start_process", env=env(inner_full(), sandbox=ident("acme", "a2")), name="proc", argv=[], child_env=None, io=HS)
add("env_start_process", env=env(inner_full()), name="proc", argv=[], child_env=None,
    io={"current_scope": A, "inner.start_process": rais("OSError", "spawn fail")})
add("env_start_process", env=env({"start_process": adesc(5)}), name="proc", argv=[], child_env=None, io=HS)
add("env_start_process", env=env(inner_full()), name="proc", argv=[], child_env=None,
    io={"current_scope": A, "inner.start_process": stream(has=("stdin", "stdout"))})

# ── env_attach_process ──
AT = {"current_scope": A, "inner.attach_process": stream()}
add("env_attach_process", env=env(inner_full()), ref="proc", io=AT)
add("env_attach_process", env=env(inner_full()), ref=pref(), io=AT)
add("env_attach_process", env=env(inner_full()), ref=pref(sandbox=KEY_B), io={})
add("env_attach_process", env=env(inner_full()), ref="proc",
    io={"current_scope": A, "inner.attach_process": None})
add("env_attach_process", env=env({}), ref="proc", io={})
add("env_attach_process", env=env(inner_full(), sandbox=ident("acme", "a2")), ref="proc", io=HS)
add("env_attach_process", env=env(inner_full()), ref="proc",
    io={"current_scope": A, "inner.attach_process": rais("KeyError", "proc")})

# ── env_stop_process ──
ST = {"current_scope": A, "inner.stop_process": True}
add("env_stop_process", env=env(inner_full()), ref="proc", io=ST)
add("env_stop_process", env=env(inner_full()), ref="proc",
    io={"current_scope": A, "inner.stop_process": False})
add("env_stop_process", env=env(inner_full()), ref=pref(), io=ST)
add("env_stop_process", env=env(inner_full()), ref=pref(sandbox=KEY_B), io={})
add("env_stop_process", env=env({}), ref="proc", io={})
add("env_stop_process", env=env(inner_full()), ref="proc",
    io={"current_scope": A, "inner.stop_process": rais("RuntimeError", "stop fail")})

# ── env_guard_stream ──
add("env_guard_stream", env=env(), stream=stream(), name="proc")
add("env_guard_stream", env=env(), stream=None, name="proc")
add("env_guard_stream", env=env(), stream=5, name="proc")
add("env_guard_stream", env=env(), stream=stream(has=("stdout", "poll")), name="proc")
add("env_guard_stream", env=env(), stream=stream(has=("stdin", "poll")), name="proc")
add("env_guard_stream", env=env(), stream=stream(has=("stdin", "stdout")), name="proc")
add("env_guard_stream", env=env(), stream=stream(), name=None)

# ── env_own_name ──
add("env_own_name", env=env(), ref="proc", what="attach_process")
add("env_own_name", env=env(), ref=pref(), what="attach_process")
add("env_own_name", env=env(), ref=pref(sandbox=KEY_B), what="stop_process")
add("env_own_name", env=env(), ref=None, what="attach_process")
add("env_own_name", env=env(), ref=5, what="attach_process")
add("env_own_name", env=env(), ref={"plain": 1}, what="attach_process")

# ── env_repr ──
add("env_repr", env=env())
add("env_repr", env=env(sandbox=ident("a c", "x/y")))
add("env_repr", env=env(inner_repr="namespace(cwd='/w')"))
add("env_repr", env=env(inner_repr=""))
add("env_repr", env=env(sandbox=ident("local", "/h/p")))

# ── pipe_op ──
add("pipe_op", pipe_state=pst(), what="read", io=HS)
add("pipe_op", pipe_state=pst(identity=ident("acme", "a2")), what="read", io=HS)
add("pipe_op", pipe_state=pst(), what="read", io=NOS)
add("pipe_op", pipe_state=pst(), what="iteration", io=HS)
add("pipe_op", pipe_state=pst(), what="read", io={"current_scope": rais("SystemExit", "x")})
add("pipe_op", pipe_state=pst(), what="read", io={})

# ── pipe_read ──
PR = {"current_scope": A, "pipe.read": "data"}
add("pipe_read", pipe_state=pst(), io=PR)
add("pipe_read", pipe_state=pst(), size=5, io=PR)
add("pipe_read", pipe_state=pst(), io={"current_scope": A, "pipe.read": rais("OSError", "read fail")})
add("pipe_read", pipe_state=pst(pdesc(methods=("write",))), io=HS)
add("pipe_read", pipe_state=pst(identity=ident("acme", "a2")), io=HS)
add("pipe_read", pipe_state=pst(), io={"current_scope": A, "pipe.read": None})
add("pipe_read", pipe_state=pst(), io={"current_scope": A})

# ── pipe_read1 ──
add("pipe_read1", pipe_state=pst(pdesc(methods=("read", "read1"))), io={"current_scope": A, "pipe.read1": "d1"})
add("pipe_read1", pipe_state=pst(pdesc(methods=("read",))), io=PR)
add("pipe_read1", pipe_state=pst(pdesc(methods=("write",))), io=HS)
add("pipe_read1", pipe_state=pst(pdesc(methods=("read", "read1"))), size=3,
    io={"current_scope": A, "pipe.read1": "d3"})
add("pipe_read1", pipe_state=pst(identity=ident("acme", "a2")), io=HS)
add("pipe_read1", pipe_state=pst(pdesc(methods=("read1",))), io={"current_scope": A, "pipe.read1": rais("ValueError", "x")})

# ── pipe_readline ──
add("pipe_readline", pipe_state=pst(), io={"current_scope": A, "pipe.readline": "line\n"})
add("pipe_readline", pipe_state=pst(), size=2, io={"current_scope": A, "pipe.readline": "li"})
add("pipe_readline", pipe_state=pst(pdesc(methods=("read",))), io=HS)
add("pipe_readline", pipe_state=pst(identity=ident("acme", "a2")), io=HS)
add("pipe_readline", pipe_state=pst(), io={"current_scope": A, "pipe.readline": rais("RuntimeError", "rl")})

# ── pipe_readlines ──
add("pipe_readlines", pipe_state=pst(pdesc(methods=("readlines",))), io={"current_scope": A, "pipe.readlines": ["a", "b"]})
add("pipe_readlines", pipe_state=pst(pdesc(methods=("readlines",))), hint=10,
    io={"current_scope": A, "pipe.readlines": ["a"]})
add("pipe_readlines", pipe_state=pst(pdesc(methods=("read",))), io=HS)
add("pipe_readlines", pipe_state=pst(pdesc(methods=("readlines",)), identity=ident("acme", "a2")), io=HS)
add("pipe_readlines", pipe_state=pst(pdesc(methods=("readlines",))), io={"current_scope": A, "pipe.readlines": []})

# ── pipe_readinto ──
add("pipe_readinto", pipe_state=pst(pdesc(methods=("readinto",))), buffer=[0, 0, 0],
    io={"current_scope": A, "pipe.readinto": 3})
add("pipe_readinto", pipe_state=pst(pdesc(methods=("readinto",))), buffer=None,
    io={"current_scope": A, "pipe.readinto": 0})
add("pipe_readinto", pipe_state=pst(pdesc(methods=("read",))), buffer=[0], io=HS)
add("pipe_readinto", pipe_state=pst(pdesc(methods=("readinto",)), identity=ident("acme", "a2")), buffer=[0], io=HS)
add("pipe_readinto", pipe_state=pst(pdesc(methods=("readinto",))), buffer=[0],
    io={"current_scope": A, "pipe.readinto": rais("OSError", "ri")})

# ── pipe_write ──
PW = {"current_scope": A, "pipe.write": 4}
add("pipe_write", pipe_state=pst(), data="data", io=PW)
add("pipe_write", pipe_state=pst(), data=None, io={"current_scope": A, "pipe.write": 0})
add("pipe_write", pipe_state=pst(pdesc(methods=("read",))), data="x", io=HS)
add("pipe_write", pipe_state=pst(identity=ident("acme", "a2")), data="x", io=HS)
add("pipe_write", pipe_state=pst(), data="x", io={"current_scope": A, "pipe.write": rais("OSError", "wr")})
add("pipe_write", pipe_state=pst(), data="x", io={"current_scope": A})

# ── pipe_writelines ──
add("pipe_writelines", pipe_state=pst(pdesc(methods=("writelines",))), lines=["a", "b"],
    io={"current_scope": A, "pipe.writelines": None})
add("pipe_writelines", pipe_state=pst(pdesc(methods=("writelines",))), lines=[],
    io={"current_scope": A, "pipe.writelines": None})
add("pipe_writelines", pipe_state=pst(pdesc(methods=("read",))), lines=["a"], io=HS)
add("pipe_writelines", pipe_state=pst(pdesc(methods=("writelines",)), identity=ident("acme", "a2")), lines=["a"], io=HS)
add("pipe_writelines", pipe_state=pst(pdesc(methods=("writelines",))), lines=["a"],
    io={"current_scope": A, "pipe.writelines": rais("ValueError", "wl")})

# ── pipe_flush ──
add("pipe_flush", pipe_state=pst(), io={"current_scope": A, "pipe.flush": None})
add("pipe_flush", pipe_state=pst(pdesc(methods=("read",))), io=HS)
add("pipe_flush", pipe_state=pst(identity=ident("acme", "a2")), io=HS)
add("pipe_flush", pipe_state=pst(), io={"current_scope": A, "pipe.flush": rais("OSError", "fl")})
add("pipe_flush", pipe_state=pst(), io={"current_scope": A})

# ── pipe_close ──
add("pipe_close", pipe_state=pst(), io={"current_scope": A, "pipe.close": None})
add("pipe_close", pipe_state=pst(pdesc(methods=("read",))), io=HS)
add("pipe_close", pipe_state=pst(identity=ident("acme", "a2")), io=HS)
add("pipe_close", pipe_state=pst(), io={"current_scope": A, "pipe.close": rais("OSError", "cl")})
add("pipe_close", pipe_state=pst(), io={"current_scope": rais("KeyboardInterrupt", "")})

# ── pipe_closed ──
add("pipe_closed", pipe_state=pst(pdesc(closed=True)), io=HS)
add("pipe_closed", pipe_state=pst(pdesc(closed=False)), io=HS)
add("pipe_closed", pipe_state=pst(pdesc(closed=0)), io=HS)
add("pipe_closed", pipe_state=pst(pdesc(closed="x")), io=HS)
add("pipe_closed", pipe_state=pst(pdesc(closed=None)), io=HS)
add("pipe_closed", pipe_state=pst(pdesc(has_closed=False)), io=HS)
add("pipe_closed", pipe_state=pst(identity=ident("acme", "a2")), io=HS)

# ── pipe_readable ──
add("pipe_readable", pipe_state=pst(pdesc(methods=("readable",))), io={"current_scope": A, "pipe.readable": True})
add("pipe_readable", pipe_state=pst(pdesc(methods=("readable",))), io={"current_scope": A, "pipe.readable": 0})
add("pipe_readable", pipe_state=pst(pdesc(methods=("readable",))), io={"current_scope": A, "pipe.readable": "x"})
add("pipe_readable", pipe_state=pst(pdesc(methods=("readable",))), io={"current_scope": A, "pipe.readable": None})
add("pipe_readable", pipe_state=pst(pdesc(methods=("read",))), io=HS)
add("pipe_readable", pipe_state=pst(pdesc(methods=("readable",)), identity=ident("acme", "a2")), io=HS)

# ── pipe_writable ──
add("pipe_writable", pipe_state=pst(pdesc(methods=("writable",))), io={"current_scope": A, "pipe.writable": True})
add("pipe_writable", pipe_state=pst(pdesc(methods=("writable",))), io={"current_scope": A, "pipe.writable": False})
add("pipe_writable", pipe_state=pst(pdesc(methods=("writable",))), io={"current_scope": A, "pipe.writable": 1})
add("pipe_writable", pipe_state=pst(pdesc(methods=("read",))), io=HS)
add("pipe_writable", pipe_state=pst(pdesc(methods=("writable",)), identity=ident("acme", "a2")), io=HS)

# ── pipe_setattr ──
add("pipe_setattr", pipe_state=pst(), name="x", value=1)
add("pipe_setattr", pipe_state=pst(), name="q'uote", value=None)
add("pipe_setattr", pipe_state=pst(), name='d"quote', value=None)
add("pipe_setattr", pipe_state=pst(), name="", value=None)
add("pipe_setattr", pipe_state=pst(), name="closed", value=True)

# ── pipe_iter ──
add("pipe_iter", pipe_state=pst(), io=HS)
add("pipe_iter", pipe_state=pst(identity=ident("acme", "a2")), io=HS)
add("pipe_iter", pipe_state=pst(), io=NOS)
add("pipe_iter", pipe_state=pst(), io={"current_scope": rais("SystemExit", "it")})
add("pipe_iter", pipe_state=pst(), io={})

# ── pipe_next ──
add("pipe_next", pipe_state=pst(), io={"current_scope": A, "pipe.readline": "abc"})
add("pipe_next", pipe_state=pst(), io={"current_scope": A, "pipe.readline": ""})
add("pipe_next", pipe_state=pst(), io={"current_scope": A, "pipe.readline": None})
add("pipe_next", pipe_state=pst(pdesc(methods=("read",))), io=HS)
add("pipe_next", pipe_state=pst(identity=ident("acme", "a2")), io=HS)
add("pipe_next", pipe_state=pst(), io={"current_scope": A, "pipe.readline": rais("OSError", "nx")})

# ── pipe_enter ──
add("pipe_enter", pipe_state=pst(), io=HS)
add("pipe_enter", pipe_state=pst(identity=ident("acme", "a2")), io=HS)
add("pipe_enter", pipe_state=pst(), io=NOS)
add("pipe_enter", pipe_state=pst(), io={"current_scope": rais("GeneratorExit", "")})
add("pipe_enter", pipe_state=pst(), io={})

# ── pipe_exit ──
add("pipe_exit", pipe_state=pst(), io={"current_scope": A, "pipe.close": None})
add("pipe_exit", pipe_state=pst(), io={"current_scope": A, "pipe.close": rais("OSError", "ex")})
add("pipe_exit", pipe_state=pst(pdesc(methods=("read",))), io=HS)
add("pipe_exit", pipe_state=pst(identity=ident("acme", "a2")), io=HS)
add("pipe_exit", pipe_state=pst(), io={"current_scope": A})

# ── pipe_repr ──
add("pipe_repr", pipe_state=pst())
add("pipe_repr", pipe_state=pst(identity=ident("a c", "x/y")))
add("pipe_repr", pipe_state=pst(identity=ident("", "")))
add("pipe_repr", pipe_state=pst(identity=ident("local", "/h/p")))
add("pipe_repr", pipe_state=pst(identity=ident("t", "a'b")))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {f: n for f, n in counts.items() if n < 5}
    if thin:
        raise SystemExit("fewer than 5 cases for: %r" % thin)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print("wrote %s: %d cases, %d funcs (min %d per func)"
          % (OUT, len(CASES), len(counts), min(counts.values())))


if __name__ == "__main__":
    main()
