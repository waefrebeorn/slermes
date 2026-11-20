#!/usr/bin/env python3
"""Oracle for the tui_gateway/methods_session.py FLOW2 port (unclaimed remainder).

QUARRY MODULE NOTE: methods_session.py is SINGULAR (there is no
methods_sessions.py).  All PoP claims name the real defining module.

The harness (tests/t_port_tui_methods_session_flow2.c) emits one JSON object
per case carrying ONLY what the C port did: {"func","in","out"} where
out = {"ret","trace","state"}.  This oracle computes the Python reference for
the same case from in.{args,io,state} and diffs the two.  The reference is a
LITERAL shadow mirror of the C port (same helpers, same universal seam with
fail-closed scripts, null-arg drop and KeyError message repr, same full
builtin-exception classification), so the differential is exact over the code
domain the fixtures exercise.  The generator's live-drive shadow-diff (mirror
vs the real AST-extracted quarry functions) independently proves the mirror is
faithful to live behaviour (0 diffs).

Summary: 'ORACLE tui_methods_session_flow2: N cases, M mismatches'.
"""
import base64
import copy
import json
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

# ---------------------------------------------------------------------------
# Full builtin exception hierarchy (target interpreter py3.14 audit).
# Maps a class name to its Python parent; aliases fold to the canonical class.
# ---------------------------------------------------------------------------
EXC_PARENT = {
    "SystemExit": "BaseException", "KeyboardInterrupt": "BaseException",
    "GeneratorExit": "BaseException", "CancelledError": "BaseException",
    "Exception": "BaseException", "ExceptionGroup": "BaseException",
    "OverflowError": "ArithmeticError", "ZeroDivisionError": "ArithmeticError",
    "FloatingPointError": "ArithmeticError", "ArithmeticError": "Exception",
    "IndexError": "LookupError", "KeyError": "LookupError", "LookupError": "Exception",
    "ModuleNotFoundError": "ImportError", "ImportError": "Exception",
    "UnboundLocalError": "NameError", "NameError": "Exception",
    "NotImplementedError": "RuntimeError", "RecursionError": "RuntimeError",
    "RuntimeError": "Exception",
    "IndentationError": "SyntaxError", "TabError": "IndentationError",
    "SyntaxError": "Exception",
    "FileNotFoundError": "OSError", "FileExistsError": "OSError",
    "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "PermissionError": "OSError", "ProcessLookupError": "OSError",
    "TimeoutError": "OSError", "InterruptedError": "OSError",
    "BlockingIOError": "OSError", "ChildProcessError": "OSError",
    "BrokenPipeError": "OSError", "ConnectionAbortedError": "OSError",
    "ConnectionRefusedError": "OSError", "ConnectionResetError": "OSError",
    "EnvironmentError": "OSError", "IOError": "OSError", "WindowsError": "OSError",
    "ConnectionError": "OSError", "OSError": "Exception",
    "UnicodeDecodeError": "UnicodeError", "UnicodeEncodeError": "UnicodeError",
    "UnicodeTranslateError": "UnicodeError", "UnicodeError": "ValueError",
    "ValueError": "Exception",
    "BytesWarning": "Warning", "DeprecationWarning": "Warning",
    "EncodingWarning": "Warning", "FutureWarning": "Warning",
    "ImportWarning": "Warning", "PendingDeprecationWarning": "Warning",
    "ResourceWarning": "Warning", "RuntimeWarning": "Warning",
    "SyntaxWarning": "Warning", "UnicodeWarning": "Warning", "UserWarning": "Warning",
    "Warning": "Exception",
}
# Direct children of Exception (incl. the domain-specific custom kinds).
_EXC_DIRECT = {
    "StopIteration", "StopAsyncIteration", "AssertionError", "AttributeError",
    "BufferError", "EOFError", "MemoryError", "ReferenceError", "SystemError",
    "TypeError", "GenerationError", "ManifestError", "SessionNotOwned",
    "CompressionBusy", "CompressionLockHeld", "PetStoreError", "ExportTooLarge",
    "ResumeTooLarge", "BillingError", "JSONDecodeError",
}
for _k in _EXC_DIRECT:
    EXC_PARENT.setdefault(_k, "Exception")


def is_subclass(kind, target):
    if not kind or not target:
        return False
    cur = kind
    for _ in range(32):
        if cur == target:
            return True
        nxt = EXC_PARENT.get(cur)
        if nxt is None:
            # Unknown/custom kinds route through Exception.
            if cur not in EXC_PARENT and target == "Exception" and cur != "BaseException":
                return True
            return False
        cur = nxt
    return False


def catches(raise_spec, target):
    kind = raise_spec.get("__raise__") if isinstance(raise_spec, dict) else None
    if not kind:
        return False
    # BaseException-only kinds pass through every except-Exception site.
    if is_subclass(kind, "BaseException") and not is_subclass(kind, "Exception"):
        return target == "BaseException"
    return is_subclass(kind, target)


# ---------------------------------------------------------------------------
# Representation helpers (mirror the C helpers exactly).
# ---------------------------------------------------------------------------
def truthy(v):
    if v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def pystr(v):
    if v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, str):
        return v
    if isinstance(v, int):
        return str(v)
    if isinstance(v, float):
        return str(int(v)) if v == int(v) else repr(v)
    return json.dumps(v, ensure_ascii=False, separators=(",", ":"))


def strip_ws(s):
    return s.strip(" \t\n\r\f\v\x1c\x1d\x1e\x1f") if s else ""


def err_spec(ty, message):
    return {"__raise__": ty or "RuntimeError", "message": message or ""}


def is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


def kerr_repr(msg):
    q = '"' if ("'" in msg and '"' not in msg) else "'"
    body = msg.replace("\\", "\\\\").replace(q, "\\" + q)
    return q + body + q


def pyint(v):
    """Return (value, None) on success or (None, raise_spec) on failure."""
    if v is None:
        return None, err_spec("TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'")
    if isinstance(v, bool):
        return (1 if v else 0), None
    if isinstance(v, int):
        return v, None
    if isinstance(v, float):
        return int(v), None
    if isinstance(v, str):
        s = v.strip(" \t\n\v\f\r")
        if not s:
            return None, err_spec("ValueError", "invalid literal for int() with base 10: ''")
        try:
            return int(s), None
        except ValueError:
            return None, err_spec("ValueError", "invalid literal for int() with base 10: " + repr(s))
    tyname = "dict" if isinstance(v, dict) else ("list" if isinstance(v, list) else "object")
    return None, err_spec("TypeError", "int() argument must be a string, a bytes-like object or a real number, not '%s'" % tyname)


def str_param(params, key):
    v = params.get(key) if isinstance(params, dict) else None
    return strip_ws(pystr(v)) if truthy(v) else ""


def ok(rid, result):
    return {"jsonrpc": "2.0", "id": rid, "result": result if result is not None else None}


def err(rid, code, msg):
    return {"jsonrpc": "2.0", "id": rid, "error": {"code": code, "message": msg or ""}}


class Ctx:
    def __init__(self, io, state):
        self.io = io if isinstance(io, dict) else {}
        self.state = copy.deepcopy(state) if isinstance(state, dict) else {}
        self.cursors = {}
        self.trace = []

    def call(self, name, args):
        clean = {k: copy.deepcopy(v) for k, v in (args or {}).items() if v is not None}
        ev = {"name": name, "args": clean}
        if name not in self.io:
            ret = err_spec("RuntimeError", "no script for " + name)
        else:
            entry = self.io[name]
            used = entry
            ret = None
            if isinstance(entry, list):
                idx = int(self.cursors.get(name, 0))
                if idx >= len(entry):
                    ret = err_spec("RuntimeError", "script exhausted for " + name)
                else:
                    self.cursors[name] = idx + 1
                    used = entry[idx]
            if ret is None:
                if isinstance(used, dict) and used.get("__script__") is True:
                    if isinstance(used.get("raise"), dict):
                        spec = used["raise"]
                        ty = spec.get("type", "RuntimeError")
                        msg = spec.get("message", "")
                        ret = err_spec(ty, kerr_repr(msg) if ty == "KeyError" else msg)
                    else:
                        r = used.get("ret")
                        ret = copy.deepcopy(r) if r is not None else None
                else:
                    ret = copy.deepcopy(used)
        ev["ret"] = copy.deepcopy(ret)
        self.trace.append(ev)
        return ret


# ---------------------------------------------------------------------------
# Shadow-mirrored ported functions.
# ---------------------------------------------------------------------------
def f_b64(ctx, args):
    data = args.get("data")
    s = data if isinstance(data, str) else ""
    return base64.b64encode(s.encode("ascii", "replace")).decode("ascii")


def f_terminal_resize(ctx, args):
    rid = args.get("rid")
    params = args.get("params") if isinstance(args.get("params"), dict) else {}
    v = params.get("cols", 80)
    cols, bad = pyint(v)
    if bad is not None:
        return err_spec("ValueError", "invalid cols")
    ctx.state["cols"] = cols
    return ok(rid, {"cols": cols})


def f_delegation_status(ctx, args):
    rid = args.get("rid")
    p = {
        "active": ctx.call("list_active_subagents", {}),
        "paused": ctx.call("is_spawn_paused", {}),
        "max_spawn_depth": ctx.call("_get_max_spawn_depth", {}),
        "max_concurrent_children": ctx.call("_get_max_concurrent_children", {}),
    }
    return ok(rid, p)


def f_delegation_pause(ctx, args):
    rid = args.get("rid")
    params = args.get("params") if isinstance(args.get("params"), dict) else {}
    paused = truthy(params.get("paused", True))
    r = ctx.call("set_spawn_paused", {"paused": paused})
    return ok(rid, {"paused": r})


def f_events_stats(ctx, args):
    rid = args.get("rid")
    return ok(rid, ctx.call("replay_stats", {}))


def f_events_since(ctx, args):
    rid = args.get("rid")
    params = args.get("params") if isinstance(args.get("params"), dict) else {}
    sv = params.get("session_id")
    sid = pystr(sv) if truthy(sv) else ""
    if sid == "None":
        sid = ""
    ls = params.get("last_seen", 0)
    last_seen, bad = pyint(ls)
    if bad is not None:
        return err(rid, -32602, "invalid params: last_seen must be an integer")
    frames = ctx.call("events_since", {"sid": sid, "last_seen": last_seen})
    latest = ctx.call("latest_seq", {"sid": sid})
    trunc = ctx.call("is_truncated", {"sid": sid, "last_seen": last_seen})
    epoch = ctx.call("replay_epoch", {})
    openreq = ctx.call("open_requests", {"sid": sid})
    count = len(frames) if isinstance(frames, list) else 0
    p = {"events": frames, "latest_seq": latest, "truncated": trunc,
         "count": count, "epoch": epoch, "open_requests": openreq}
    return ok(rid, p)


def f_pet_disable(ctx, args):
    rid = args.get("rid")
    ctx.call("set_enabled", {"enabled": False})
    return ok(rid, {"ok": True})


def f_pet_scale(ctx, args):
    rid = args.get("rid")
    params = args.get("params") if isinstance(args.get("params"), dict) else {}
    r = ctx.call("set_pet_scale", {"scale": params.get("scale")})
    scale = r[0] if isinstance(r, list) and len(r) > 0 else None
    perr = r[1] if isinstance(r, list) and len(r) > 1 else None
    if truthy(perr):
        return err(rid, 4004, pystr(perr))
    return ok(rid, {"ok": True, "scale": scale})


def f_pet_cancel(ctx, args):
    rid = args.get("rid")
    params = args.get("params") if isinstance(args.get("params"), dict) else {}
    token = str_param(params, "token")
    if token:
        ctx.call("pet_cancel_request", {"token": token})
    return ok(rid, {"ok": True})


def f_pet_gen_status(ctx, args):
    rid = args.get("rid")
    available = True
    r1 = ctx.call("resolve_provider", {"require_references": True})
    if is_raise(r1) and catches(r1, "GenerationError"):
        available = False
    r2 = ctx.call("list_sprite_providers", {})
    providers = copy.deepcopy(r2) if (isinstance(r2, list) and not is_raise(r2)) else []
    return ok(rid, {"available": available, "providers": providers})


def f_session_close(ctx, args):
    rid = args.get("rid")
    params = args.get("params") if isinstance(args.get("params"), dict) else {}
    sid = params.get("session_id", "")
    session = ctx.call("pop_session_by_id", {"session_id": sid})
    closed = ctx.call("teardown_popped_session", {"session": copy.deepcopy(session), "end_reason": "tui_close"})
    msgs = []
    if isinstance(session, dict):
        em = session.get("_end_msgs")
        if isinstance(em, list):
            msgs = copy.deepcopy(em)
        elif isinstance(em, str):
            msgs = list(em)          # list("text") -> chars
        elif isinstance(em, dict):
            msgs = list(em.keys())   # list(dict) -> keys
    return ok(rid, {"closed": closed, "messages": msgs})


def f_session_branch(ctx, args):
    rid = args.get("rid")
    params = args.get("params")
    session = args.get("session")
    return ctx.call("branch_live", {"rid": rid, "params": copy.deepcopy(params),
                                    "session": copy.deepcopy(session), "omit_messages": False})


def f_session_branch_whole(ctx, args):
    rid = args.get("rid")
    params = args.get("params")
    session = args.get("session")
    return ctx.call("branch_live", {"rid": rid, "params": copy.deepcopy(params),
                                    "session": copy.deepcopy(session), "omit_messages": True})


def f_save_via_compute_host(ctx, args):
    rid = args.get("rid")
    params = args.get("params") if isinstance(args.get("params"), dict) else {}
    sv = params.get("session_id")
    sid = pystr(sv) if truthy(sv) else ""   # str(...) -- NO strip (live body)
    ack = ctx.call("send_compute_host_control", {"session_id": sid, "route_name": "session.save", "wait": True})
    if is_raise(ack):
        return err(rid, 5011, "compute-host session save failed: " + ack.get("message", ""))
    cv = ack.get("code") if isinstance(ack, dict) else None
    if truthy(cv) and isinstance(cv, (int, float)) and not isinstance(cv, bool):
        code = int(cv) if cv == int(cv) else cv
    elif truthy(cv) and isinstance(cv, str):
        try:
            code = float(cv)
            if code == int(code):
                code = int(code)
        except ValueError:
            code = 5011
    else:
        code = 5011
    resp = ctx.call("compute_host_ack_error", {"rid": rid, "ack": copy.deepcopy(ack),
                                               "code": code, "message": "compute-host session save failed"})
    if isinstance(resp, dict) and not is_raise(resp) and resp:
        if "error" in resp or "jsonrpc" in resp:
            return resp
    result = ack.get("result") if isinstance(ack, dict) else None
    if not isinstance(result, dict):
        return err(rid, 5011, "compute-host session save returned an invalid response")
    return ok(rid, copy.deepcopy(result))


def f_spawn_tree_load(ctx, args):
    rid = args.get("rid")
    params = args.get("params") if isinstance(args.get("params"), dict) else {}
    raw = str_param(params, "path")
    if not raw:
        return err(rid, 4000, "path required")
    resolved = ctx.call("path_resolve", {"path": raw})
    if is_raise(resolved):
        return err(rid, 4030, "path outside spawn-trees root: " + resolved.get("message", ""))
    rp = pystr(resolved)
    payload = ctx.call("path_read_json", {"path": rp})
    if is_raise(payload):
        return err(rid, 5000, "spawn_tree.load failed: " + payload.get("message", ""))
    if not isinstance(payload, dict):
        return err(rid, 5000, "spawn_tree.load failed: snapshot is not a JSON object")
    return ok(rid, copy.deepcopy(payload))


FN = {
    "_b64": f_b64,
    "terminal.resize": f_terminal_resize,
    "delegation.status": f_delegation_status,
    "delegation.pause": f_delegation_pause,
    "session.events.stats": f_events_stats,
    "session.events.since": f_events_since,
    "pet.disable": f_pet_disable,
    "pet.scale": f_pet_scale,
    "pet.cancel": f_pet_cancel,
    "pet.generate.status": f_pet_gen_status,
    "session.close": f_session_close,
    "session.branch": f_session_branch,
    "session.branch_whole": f_session_branch_whole,
    "_save_via_compute_host": f_save_via_compute_host,
    "spawn_tree.load": f_spawn_tree_load,
}


def canon(v):
    """Normalize integral floats to ints so C number serialization matches."""
    if isinstance(v, bool) or v is None:
        return v
    if isinstance(v, float):
        return int(v) if v == int(v) else v
    if isinstance(v, list):
        return [canon(x) for x in v]
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    return v


def reference(rec):
    inp = rec.get("in") if isinstance(rec.get("in"), dict) else {}
    args = inp.get("args") if isinstance(inp.get("args"), dict) else {}
    func = rec.get("func")
    fn = FN.get(func)
    if fn is None:
        return {"error": "unknown func"}
    ctx = Ctx(inp.get("io"), inp.get("state"))
    ret = fn(ctx, args)
    return {"ret": ret, "trace": ctx.trace, "state": ctx.state}


def main():
    total = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception:
            continue
        if not isinstance(rec, dict):
            continue
        expected = reference(rec)
        got = rec.get("out")
        total += 1
        if canon(expected) != canon(got):
            mism += 1
            if mism <= 12:
                print("MISMATCH func=%s" % rec.get("func"), file=sys.stderr)
                print("  expected:", json.dumps(canon(expected), ensure_ascii=False, sort_keys=True), file=sys.stderr)
                print("  got     :", json.dumps(canon(got), ensure_ascii=False, sort_keys=True), file=sys.stderr)
    print("ORACLE tui_methods_session_flow2: %d cases, %d mismatches" % (total, mism))
    return 0 if (total > 0 and mism == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
