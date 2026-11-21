#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW12 remainder port (C sibling:
src/gateway/platforms/discord_adapter_flow12.c).

Reads the harness JSONL on stdin, computes the Python reference with SCRIPTED
seams and compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state. Every seamed call is mirrored as ctx.call in both the C
port (da12_call) and the models below; the SAME scripted io drives both, so the
decision sequences line up exactly. Fail-closed: missing defs, seam-set drift
or 0 cases => exit 2. Per-model raises round-trip their spec; BaseException
family propagates verbatim.

Summary: 'ORACLE discord_adapter_flow12: N cases, M mismatches'.
"""
import ast
import json
import os
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
Q = os.path.expanduser("~/hermes-agent")
if not os.path.exists(os.path.join(Q, "plugins", "platforms", "discord", "adapter.py")):
    for _cand in (str(_REPO.parent / "hermes-agent"), "C:/Users/eman5/hermes-agent"):
        if os.path.exists(os.path.join(_cand, "plugins", "platforms", "discord", "adapter.py")):
            Q = _cand
            break
A_SRC = os.path.join(Q, "plugins", "platforms", "discord", "adapter.py")

PORTED = [
    "_is_forum_parent", "_iter_channel_and_thread_messages",
    "_iter_missed_message_backfill_candidates", "_lead_silence_bytes",
    "_liveness_loop", "_load", "_load_voice_fx_config",
    "_looks_like_nonconversational_history_message",
    "_message_has_non_down_bot_response", "_missed_message_backfill_channels",
    "_native_slash_commands", "_notify_liveness_fatal_error",
    "_notify_unauthorized_slash", "_is_length_overflow_error",
    "_normalize_permissions", "_message_reference_from_ids",
]

# ── canonical seam set (SEAM-NAME SET EQUALITY with the C port) ──
SEAMS = [
    'BYTES_PER_MS',
    'ForumChannel',
    'MessageReference',
    'abort_transport',
    'add_done_callback',
    'archived_call',
    'archived_threads',
    'author',
    'author_id',
    'await_close',
    'backfill_config',
    'backfill_limit',
    'backfill_window_seconds',
    'bot_id',
    'bot_user',
    'cancel',
    'channel_id',
    'channel_type',
    'client',
    'close',
    'closing_task',
    'content',
    'create_task',
    'created_at',
    'current_task',
    'decode_json_list_literal',
    'deliver_alert',
    'disconnecting',
    'discord_object',
    'exists',
    'fetch_channel',
    'free_response_channels',
    'gate_csv_set',
    'gate_env',
    'gateway_runner',
    'get_allowed_channels',
    'get_channel',
    'guild',
    'guild_id',
    'guilds',
    'health',
    'history',
    'history_call',
    'history_patterns',
    'interval',
    'is_down_notice',
    'isinstance_forum',
    'iter_messages',
    'logger.debug',
    'logger.error',
    'logger.info',
    'logger.warning',
    'message_channel',
    'message_id',
    'notif_task',
    'notify_fatal_error',
    'now',
    'obfuscated',
    'owner_profile',
    'pattern_match',
    'profile_dir',
    'profile_scope',
    'read_raw_config',
    'read_text',
    'recovery_cursor',
    'ref_message_id',
    'reference',
    'running',
    'set_closing_task',
    'set_disconnecting',
    'set_fatal_error',
    'set_notif_task',
    'sleep',
    'snowflake_time',
    'specs',
    'state_path',
    't_discord',
    'terminal_reasons',
    'text_channels',
    'text_limit',
    'thread',
    'thread_id',
    'threads',
    'threshold',
    'type_value',
    'voice_fx_cfg',
    'voice_mixer_module',
    'wait',
    'ws',
]

def _verify_upstream_defs():
    """Fail-closed: each PoP target must exist in the live quarry source."""
    try:
        tree = ast.parse(open(A_SRC, encoding="utf-8").read())
    except OSError:
        print("FATAL oracle_discord_adapter_flow12: adapter.py unavailable")
        sys.exit(2)
    names = set()
    for n in ast.walk(tree):
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)):
            names.add(n.name)
    missing = [k for k in PORTED if k not in names]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow12: missing defs: {missing}")
        sys.exit(2)

_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "discord_adapter_flow12.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'da12_call\(\s*ctx,\s*"([^"]+)"', _CSRC))
ARGMAP = {s: (lambda *a, **k: {}) for s in SEAMS}


def canon(v):
    def conv(x):
        if isinstance(x, BaseException):
            return str(x)
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


EXC_CLASSES = {
    "Exception": Exception, "BaseException": BaseException,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "IOError": OSError,
    "LookupError": LookupError, "StopIteration": StopIteration, "IndexError": IndexError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "NotImplementedError": NotImplementedError, "SyntaxError": SyntaxError,
    "TimeoutError": TimeoutError, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": type("CancelledError", (BaseException,), {}),
    "asyncio.CancelledError": type("CancelledError", (BaseException,), {}),
}
BASE_EXC = {"BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
            "CancelledError", "asyncio.CancelledError"}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind) or type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _raise(self, name, args, spec, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": spec})
        self.scripted.append((exc, spec))
        raise exc

    def call(self, name, args=None, force_args=None):
        args = force_args if force_args is not None else (args or {})
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            msg = f"no script for {name}"
            self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                msg = f"script exhausted for {name}"
                self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                rs = used["raise"]
                spec = {"__raise__": rs.get("type", "RuntimeError"), "message": rs.get("message", "")}
                self._raise(name, args, spec, make_exc(rs))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


_MISSING = object()


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


def _c(ctx, name, fa):
    try:
        return "ok", ctx.call(name, force_args=fa)
    except BaseException as e:
        return "raise", exc_spec(e, ctx)


def _truthy(v):
    if v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return v != ""
    if isinstance(v, (list, dict, tuple, set)):
        return len(v) > 0
    return False


def _base(spec):
    return isinstance(spec, dict) and spec.get("__raise__") in BASE_EXC


def _israise(v):
    return isinstance(v, dict) and "__raise__" in v


def _pystr(v):
    if isinstance(v, str):
        return v
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, int):
        return str(v)
    if isinstance(v, float):
        return str(int(v)) if v.is_integer() else repr(v)
    if v is None:
        return "None"
    return ""


def _num(v, d=0):
    if isinstance(v, bool):
        return 1 if v else 0
    if isinstance(v, (int, float)):
        return v
    return d


def _toint(v):
    if isinstance(v, bool):
        return (1 if v else 0), True
    if isinstance(v, int):
        return v, True
    if isinstance(v, float):
        return int(v), True
    if isinstance(v, str):
        try:
            return int(v.strip()), True
        except ValueError:
            return 0, False
    return 0, False


def _eq(a, b):
    an = isinstance(a, (int, float))
    bn = isinstance(b, (int, float))
    if an and bn:
        return _num(a) == _num(b)
    if isinstance(a, str) and isinstance(b, str):
        return a == b
    if a is None:
        return b is None
    return False


def _aslist(v):
    return v if isinstance(v, list) else []


def _asset(v):
    return set(_pystr(x) for x in (v if isinstance(v, list) else []))


def _log(fmt):
    return {"fmt": fmt, "args": []}


MODELS = {}


def model(fn):
    MODELS[fn.__name__[1:]] = fn
    return fn


# ───────────────────────────── models ─────────────────────────────

@model
def m_is_forum_parent(ctx, args):
    ch = args.get("channel")
    if ch is None:
        return False
    st, fc = _c(ctx, "ForumChannel", {})
    if st == "raise":
        return fc
    st, iv = _c(ctx, "isinstance_forum", {"channel": ch, "cls": fc})
    if st == "raise":
        return iv
    if _truthy(iv):
        return True
    st, ct = _c(ctx, "channel_type", {})
    if st == "raise":
        return ct
    if ct is not None:
        st, tv = _c(ctx, "type_value", {"channel_type": ct})
        if st == "raise":
            return tv
        if isinstance(tv, (int, float)) and not isinstance(tv, bool) and tv == 15:
            return True
    return False


@model
def m_load(ctx, args):
    st, p = _c(ctx, "state_path", {})
    if st == "raise":
        return p
    st, ex = _c(ctx, "exists", {"path": p})
    if st == "raise":
        return ex
    if not _truthy(ex):
        return []
    st, txt = _c(ctx, "read_text", {"encoding": "utf-8-sig"})
    if st == "raise":
        if _base(txt):
            return txt
        st, lg = _c(ctx, "logger.debug", _log("[%s] Failed to load non-conversational Discord IDs"))
        return lg if st == "raise" else []
    try:
        data = json.loads(txt)
    except Exception:
        st, lg = _c(ctx, "logger.debug", _log("[%s] Failed to load non-conversational Discord IDs"))
        return lg if st == "raise" else []
    out = []
    if isinstance(data, list):
        for mid in data:
            s = _pystr(mid).strip()
            if s:
                out.append(s)
    return out


@model
def m_load_voice_fx_config(ctx, args):
    d = {"enabled": False, "ambient_enabled": True, "ambient_path": "",
         "ambient_gain": 0.18, "duck_gain": 0.06, "speech_gain": 1.0,
         "lead_silence_ms": 200, "ack_enabled": True, "ack_phrases": None}
    st, cfg = _c(ctx, "read_raw_config", {})
    if st == "raise":
        if _base(cfg):
            return cfg
        st, lg = _c(ctx, "logger.debug", _log("Could not load discord.voice_fx config: %s"))
        return lg if st == "raise" else d
    disc = cfg.get("discord") if isinstance(cfg, dict) else None
    fx = (disc or {}).get("voice_fx") if isinstance(disc, dict) else None
    if isinstance(fx, dict):
        for k, v in fx.items():
            if k in d and v is not None:
                d[k] = v
    return d


@model
def m_looks_like_nonconversational_history_message(ctx, args):
    cv = args.get("content")
    text = cv if isinstance(cv, str) else ""
    st, pats = _c(ctx, "history_patterns", {})
    if st == "raise":
        return pats
    any_ = False
    if isinstance(pats, list):
        for p in pats:
            st, m = _c(ctx, "pattern_match", {"pattern": p if isinstance(p, str) else "", "text": text})
            if st == "raise":
                return m
            if _truthy(m):
                any_ = True
                break
    return any_


@model
def m_is_length_overflow_error(ctx, args):
    text = _pystr(args.get("err")).lower()
    return "error code: 50035" in text and ("2000 or fewer" in text or "fewer in length" in text)


@model
def m_normalize_permissions(ctx, args):
    v = args.get("value")
    if v is None:
        return None
    return _pystr(v)


@model
def m_message_reference_from_ids(ctx, args):
    ch = args.get("channel")
    st, cid = _c(ctx, "channel_id", {"channel": ch})
    if st == "raise":
        return cid
    st, gld = _c(ctx, "guild", {"channel": ch})
    if st == "raise":
        return gld
    st, gid = _c(ctx, "guild_id", {"guild": gld})
    if st == "raise":
        return gid
    ci, _ = _toint(args.get("message_id"))
    st, ref = _c(ctx, "MessageReference", {"message_id": ci, "channel_id": cid,
                                           "guild_id": gid, "fail_if_not_exists": False})
    return ref


@model
def m_missed_message_backfill_channels(ctx, args):
    st, cfg = _c(ctx, "backfill_config", {})
    if st == "raise":
        return cfg
    if isinstance(cfg, dict) and "channels" in cfg:
        raw = cfg.get("channels")
        st, chans = _c(ctx, "gate_csv_set", {"raw": raw})
        if st == "raise":
            return chans
        if not (isinstance(chans, list) and len(chans) > 0):
            st, dec = _c(ctx, "decode_json_list_literal", {"raw": raw})
            if st == "raise":
                return dec
            if not isinstance(dec, list):
                chans = None
        if chans is not None:
            return _asset(chans)
    st, raw = _c(ctx, "gate_env", {"key": "DISCORD_MISSED_MESSAGE_BACKFILL_CHANNELS"})
    if st == "raise":
        return raw
    if not (isinstance(raw, str) and raw.strip()):
        st, allowed = _c(ctx, "get_allowed_channels", {})
        if st == "raise":
            return allowed
        st, free = _c(ctx, "free_response_channels", {})
        if st == "raise":
            return free
        return _asset(allowed) | _asset(free)
    st, chans = _c(ctx, "gate_csv_set", {"raw": raw})
    return chans


@model
def m_notify_unauthorized_slash(ctx, args):
    st, runner = _c(ctx, "gateway_runner", {})
    if st == "raise":
        return runner
    if not _truthy(runner):
        return None
    st, prof = _c(ctx, "owner_profile", {})
    if st == "raise":
        return prof
    da = {"runner": runner, "profile": prof,
          "user_name": args.get("user_name"), "user_id": args.get("user_id"),
          "chan_id": args.get("chan_id"), "guild_id": args.get("guild_id"),
          "command_text": args.get("command_text"), "reason": args.get("reason")}
    if _truthy(prof):
        st, pdir = _c(ctx, "profile_dir", {"profile": prof})
        if st == "raise":
            st2, result = "raise", pdir
        else:
            st, scope = _c(ctx, "profile_scope", {"profile_dir": pdir})
            if st == "raise":
                st2, result = "raise", scope
            else:
                st2, result = _c(ctx, "deliver_alert", da)
    else:
        st2, result = _c(ctx, "deliver_alert", da)
    if st2 == "raise":
        if _base(result):
            return result
        la = {"fmt": "[Discord] Admin notify: profile %r scope failed: %s",
              "args": [prof, result.get("message", "") if isinstance(result, dict) else ""]}
        st, lg = _c(ctx, "logger.debug", la)
        return lg if st == "raise" else None
    return None


@model
def m_lead_silence_bytes(ctx, args):
    st, cfg = _c(ctx, "voice_fx_cfg", {})
    if st == "raise":
        return cfg
    out = {"zeros": 0}
    lmv = cfg.get("lead_silence_ms") if isinstance(cfg, dict) else None
    if not _truthy(lmv):
        return out
    lead, valid = _toint(lmv)
    if not valid or lead <= 0:
        return out
    st, mod = _c(ctx, "voice_mixer_module", {})
    if st == "raise":
        return mod
    st, bps = _c(ctx, "BYTES_PER_MS", {"module": mod})
    if st == "raise":
        return bps
    b = bps if isinstance(bps, (int, float)) and not isinstance(bps, bool) else 0
    if b < 0:
        b = 0
    out["zeros"] = b * lead
    return out


@model
def m_native_slash_commands(ctx, args):
    st, lim = _c(ctx, "text_limit", {})
    if st == "raise":
        return lim
    limit = _num(lim)
    st, specs = _c(ctx, "specs", {})
    if st == "raise":
        return specs
    out = []
    if isinstance(specs, list):
        for sp in specs:
            if not isinstance(sp, list) or len(sp) < 5:
                continue
            name, desc_key, sargs, tmpl, follow = sp[0], sp[1], sp[2], sp[3], sp[4]
            st, desc = _c(ctx, "t_discord", {"key": desc_key, "limit": limit})
            if st == "raise":
                return desc
            largs = []
            if isinstance(sargs, list):
                for ar in sargs:
                    if not isinstance(ar, list) or len(ar) < 5:
                        continue
                    an, at, dv, dkey, choices = ar[0], ar[1], ar[2], ar[3], ar[4]
                    st, atext = _c(ctx, "t_discord", {"key": dkey, "limit": limit})
                    if st == "raise":
                        return atext
                    if isinstance(choices, list):
                        chlist = []
                        for cr in choices:
                            if not isinstance(cr, list) or len(cr) < 2:
                                continue
                            lbl, val = cr[0], cr[1]
                            ls = lbl if isinstance(lbl, str) else ""
                            if "." in ls:
                                st, tr = _c(ctx, "t_discord", {"key": ls, "limit": limit})
                                if st == "raise":
                                    return tr
                                chosen = tr
                            else:
                                chosen = lbl
                            chlist.append([chosen, val])
                    else:
                        chlist = None
                    largs.append([an, at, dv, atext, chlist])
            out.append([name, desc, largs, tmpl, follow])
    return out


@model
def m_iter_channel_and_thread_messages(ctx, args):
    channel = args.get("channel")
    limit = args.get("limit", 0)
    after = args.get("after")
    seen = set(_pystr(x) for x in _aslist(args.get("seen_channels")))
    out = []

    def rec(ch, depth):
        if depth > 40:
            return None
        st, v = _c(ctx, "channel_id", {"channel": ch})
        if st == "raise":
            return v
        key = _pystr(v)
        if not key or key in seen:
            return None
        seen.add(key)
        channel_after = after
        st, cur = _c(ctx, "recovery_cursor", {"channel_key": key})
        if st == "raise":
            return cur
        if _truthy(cur):
            cid, valid = _toint(cur)
            if valid:
                set_after = False
                if not (isinstance(after, dict) and "datetime" in after):
                    set_after = True
                else:
                    st, ts = _c(ctx, "snowflake_time", {"cursor_id": cid})
                    if st == "raise":
                        return ts
                    ats = after.get("datetime", 0)
                    if isinstance(ts, (int, float)) and not isinstance(ts, bool) and \
                            isinstance(ats, (int, float)) and ts > ats:
                        set_after = True
                if set_after:
                    st, obj = _c(ctx, "discord_object", {"id": cid})
                    if st == "raise":
                        return obj
                    channel_after = obj
        st, hc = _c(ctx, "history", {"channel": ch})
        if st == "raise":
            return hc
        if _truthy(hc):
            st, msgs = _c(ctx, "history_call", {"limit": limit, "after": channel_after,
                                                "oldest_first": False})
            if st == "raise":
                if _base(msgs):
                    return msgs
                st, lg = _c(ctx, "logger.debug", _log("[%s] Cannot read history for %s: %s"))
                if st == "raise":
                    return lg
            else:
                if isinstance(msgs, list):
                    for m in reversed(msgs):
                        out.append(m)
        st, ths = _c(ctx, "threads", {"channel": ch})
        if st == "raise":
            return ths
        children = list(ths) if isinstance(ths, list) else []
        st, arch = _c(ctx, "archived_threads", {"channel": ch})
        if st == "raise":
            return arch
        if _truthy(arch):
            st, al = _c(ctx, "archived_call", {"limit": limit})
            if st == "raise":
                if _base(al):
                    return al
                st, lg = _c(ctx, "logger.debug", _log("[%s] Cannot list archived threads for %s: %s"))
                if st == "raise":
                    return lg
            else:
                if isinstance(al, list):
                    children.extend(al)
        for th in children:
            st, tv = _c(ctx, "thread_id", {"thread": th})
            if st == "raise":
                return tv
            tkey = _pystr(tv)
            if not tkey or tkey in seen:
                continue
            r = rec(th, depth + 1)
            if r is not None:
                return r
        return None

    bad = rec(channel, 0)
    if bad is not None:
        return bad
    return out


@model
def m_iter_missed_message_backfill_candidates(ctx, args):
    st, cli = _c(ctx, "client", {})
    if st == "raise":
        return cli
    if not _truthy(cli):
        return []
    st, na = _c(ctx, "now", {})
    if st == "raise":
        return na
    st, wa = _c(ctx, "backfill_window_seconds", {})
    if st == "raise":
        return wa
    after_ts = _num(na) - _num(wa)
    st, lim = _c(ctx, "backfill_limit", {})
    if st == "raise":
        return lim
    limit = _num(lim)
    ids = args.get("channel_ids")
    cands = []
    if isinstance(ids, list) and any(_pystr(x) == "*" for x in ids):
        st, guilds = _c(ctx, "guilds", {"client": cli})
        if st == "raise":
            return guilds
        if isinstance(guilds, list):
            for g in guilds:
                st, tcs = _c(ctx, "text_channels", {"guild": g})
                if st == "raise":
                    return tcs
                if isinstance(tcs, list):
                    cands.extend(tcs)
    else:
        for sid in sorted((_pystr(x) for x in (ids if isinstance(ids, list) else []))):
            ci, _ = _toint(sid)
            st, ch = _c(ctx, "get_channel", {"channel_id": ci})
            if st == "raise":
                if _base(ch):
                    return ch
                ch = None
            if ch is None:
                st, fch = _c(ctx, "fetch_channel", {"channel_id": ci})
                if st == "raise":
                    if _base(fch):
                        return fch
                    st, lg = _c(ctx, "logger.debug", _log("[%s] Cannot fetch backfill channel %s: %s"))
                    if st == "raise":
                        return lg
                    continue
                ch = fch
            cands.append(ch)
    filtered = []
    for ch in cands:
        st, ob = _c(ctx, "obfuscated", {"channel": ch})
        if st == "raise":
            return ob
        if not _truthy(ob):
            filtered.append(ch)
    iters = []
    for ch in filtered:
        st, msgs = _c(ctx, "iter_messages", {"channel": ch, "limit": limit, "after": after_ts})
        if st == "raise":
            return msgs
        iters.append([list(msgs) if isinstance(msgs, list) else [], 0])
    out = []
    yielded = 0
    while iters and yielded < limit:
        nxt = []
        for it in iters:
            if it[1] < len(it[0]):
                out.append(it[0][it[1]])
                it[1] += 1
                yielded += 1
                nxt.append(it)
                if yielded >= limit:
                    break
        iters = nxt
    return out


def _scan(ctx, message, channel, bid):
    st, h = _c(ctx, "history", {"channel": channel})
    if st == "raise":
        return h if _base(h) else False
    if not _truthy(h):
        return False
    st, created = _c(ctx, "created_at", {"message": message})
    if st == "raise":
        return created if _base(created) else False
    st, cands = _c(ctx, "history_call", {"limit": 25, "after": created, "oldest_first": True})
    if st == "raise":
        return cands if _base(cands) else False
    if not isinstance(cands, list):
        return False
    for cand in cands:
        st, author = _c(ctx, "author", {"candidate": cand})
        if st == "raise":
            return author if _base(author) else False
        st, aid = _c(ctx, "author_id", {"author": author})
        if st == "raise":
            return aid if _base(aid) else False
        if not _eq(aid, bid):
            continue
        st, content = _c(ctx, "content", {"candidate": cand})
        if st == "raise":
            return content if _base(content) else False
        st, down = _c(ctx, "is_down_notice", {"content": content})
        if st == "raise":
            return down if _base(down) else False
        if _truthy(down):
            continue
        st, ref = _c(ctx, "reference", {"candidate": cand})
        if st == "raise":
            return ref if _base(ref) else False
        st, rmi = _c(ctx, "ref_message_id", {"reference": ref})
        if st == "raise":
            return rmi if _base(rmi) else False
        refbuf = _pystr(rmi) if _truthy(rmi) else ""
        st, mid = _c(ctx, "message_id", {"message": message})
        if st == "raise":
            return mid if _base(mid) else False
        if refbuf == _pystr(mid):
            return True
    return False


@model
def m_message_has_non_down_bot_response(ctx, args):
    message = args.get("message")
    st, cli = _c(ctx, "client", {})
    if st == "raise":
        return cli
    if not _truthy(cli):
        return False
    st, bu = _c(ctx, "bot_user", {"client": cli})
    if st == "raise":
        return bu
    st, bid = _c(ctx, "bot_id", {"bot_user": bu})
    if st == "raise":
        return bid
    if bid is None:
        return False
    st, chan = _c(ctx, "message_channel", {"message": message})
    if st == "raise":
        return chan
    r = _scan(ctx, message, chan, bid)
    if _israise(r):
        return r
    if r:
        return True
    st, thread = _c(ctx, "thread", {"message": message})
    if st == "raise":
        return thread
    if thread is not None:
        r = _scan(ctx, message, thread, bid)
        if _israise(r):
            return r
        return bool(r)
    return False


@model
def m_liveness_loop(ctx, args):
    st, iv = _c(ctx, "interval", {})
    if st == "raise":
        return iv
    interval = _num(iv)
    st, th = _c(ctx, "threshold", {})
    if st == "raise":
        return th
    threshold = _num(th)
    failures = 0
    for _ in range(256):
        st, rv = _c(ctx, "running", {})
        if st == "raise":
            return rv
        if not _truthy(rv):
            return None
        st, sl = _c(ctx, "sleep", {"interval": interval})
        if st == "raise":
            return sl
        st, cli = _c(ctx, "client", {})
        if st == "raise":
            return cli
        st, rv2 = _c(ctx, "running", {})
        if st == "raise":
            return rv2
        st, dv = _c(ctx, "disconnecting", {})
        if st == "raise":
            return dv
        if not _truthy(rv2) or cli is None or _truthy(dv):
            st, lg = _c(ctx, "logger.info", _log(
                "[%s] Discord liveness probe exiting (running=%s, client=%s, disconnecting=%s)"))
            if st == "raise":
                return lg
            return None
        st, h = _c(ctx, "health", {"client": cli})
        healthy = False
        reason = ""
        if st == "raise":
            if _base(h):
                return h
            reason = "health_check_error"
        else:
            if isinstance(h, list) and len(h) >= 2:
                healthy = _truthy(h[0])
                reason = h[1] if isinstance(h[1], str) else ""
        if healthy:
            if failures > 0:
                st, lg = _c(ctx, "logger.info", _log(
                    "[%s] Discord Gateway WebSocket healthy again after %d unhealthy sample(s)"))
                if st == "raise":
                    return lg
            failures = 0
            continue
        failures += 1
        st, lg = _c(ctx, "logger.warning", {"reason": reason, "failures": failures,
                                            "threshold": threshold})
        if st == "raise":
            return lg
        st, treasons = _c(ctx, "terminal_reasons", {})
        if st == "raise":
            return treasons
        terminal = any(_pystr(x) == reason for x in _aslist(treasons))
        if failures < threshold and not terminal:
            continue
        st, x = _c(ctx, "set_disconnecting", {"value": True})
        if st == "raise":
            return x
        st, x = _c(ctx, "logger.error", _log(
            "[%s] Discord Gateway WebSocket %s (%s, %d/%d); forcing reconnect"))
        if st == "raise":
            return x
        st, x = _c(ctx, "set_fatal_error", {
            "kind": "discord_websocket_health_stale",
            "message": "Discord Gateway WebSocket health check failed: " + reason,
            "retryable": True})
        if st == "raise":
            return x
        st, x = _c(ctx, "create_task", {"coro": "_notify_liveness_fatal_error"})
        if st == "raise":
            return x
        return None
    return None


@model
def m_notify_liveness_fatal_error(ctx, args):
    client = args.get("client")
    st, ws = _c(ctx, "ws", {"client": client})
    if st == "raise":
        return ws
    st, close = _c(ctx, "close", {"client": client})
    if st == "raise":
        if _base(close):
            return close
        st, lg = _c(ctx, "logger.debug", _log("[%s] Error closing unhealthy Discord client"))
        return lg if st == "raise" else None
    state = 0
    st, w = _c(ctx, "wait", {"task": close, "timeout": 1.0})
    if st == "raise":
        if _base(w):
            return w
        st, lg = _c(ctx, "logger.debug", _log("[%s] Error closing unhealthy Discord client"))
        if st == "raise":
            return lg
        state = 2
    else:
        done = _truthy(w.get("done")) if isinstance(w, dict) else False
        if not done:
            state = 1
        else:
            st, aw = _c(ctx, "await_close", {"task": close})
            if st == "raise":
                if _base(aw):
                    return aw
                st, lg = _c(ctx, "logger.debug", _log("[%s] Error closing unhealthy Discord client"))
                if st == "raise":
                    return lg
                state = 2
    if state == 1:
        st, lg = _c(ctx, "logger.warning", _log("[%s] Timed out closing unhealthy Discord client"))
        if st == "raise":
            return lg
        st, x = _c(ctx, "cancel", {"task": close})
        if st == "raise":
            return x
        st, x = _c(ctx, "add_done_callback", {"task": close})
        if st == "raise":
            return x
        st, ct = _c(ctx, "closing_task", {"client": client})
        if st == "raise":
            return ct
        if isinstance(ct, dict):
            st, x = _c(ctx, "cancel", {"task": ct})
            if st == "raise":
                return x
            st, x = _c(ctx, "add_done_callback", {"task": ct})
            if st == "raise":
                return x
            st, x = _c(ctx, "set_closing_task", {"client": client, "value": None})
            if st == "raise":
                return x
        st, ab = _c(ctx, "abort_transport", {"ws": ws})
        if st == "raise":
            if _base(ab):
                return ab
            st, lg = _c(ctx, "logger.debug", _log(
                "[%s] Error aborting unhealthy Discord WebSocket transport"))
            if st == "raise":
                return lg
        elif _truthy(ab):
            st, lg = _c(ctx, "logger.warning", _log(
                "[%s] Aborted unresponsive Discord WebSocket transport"))
            if st == "raise":
                return lg
    st, nt = _c(ctx, "notif_task", {})
    if st == "raise":
        return nt
    st, ct2 = _c(ctx, "current_task", {})
    if st == "raise":
        return ct2
    if _eq(nt, ct2):
        st, x = _c(ctx, "set_notif_task", {"value": None})
        if st == "raise":
            return x
    st, nf = _c(ctx, "notify_fatal_error", {})
    if st == "raise":
        if _base(nf):
            return nf
        st, lg = _c(ctx, "logger.debug", _log("[%s] Fatal-error handler raised"))
        if st == "raise":
            return lg
    return None


# ───────────────────────────── driver ─────────────────────────────

def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    try:
        ret = MODELS[func](ctx, args)
    except BaseException as exc:
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {}}


def cross_check():
    if set(SEAMS) != SEAMED:
        print(f"FATAL oracle_discord_adapter_flow12: SEAMS/SEAMED drift: "
              f"missing={sorted(SEAMED - set(SEAMS))} extra={sorted(set(SEAMS) - SEAMED)}")
        sys.exit(2)
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_discord_adapter_flow12: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_discord_adapter_flow12: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow12.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(str(fix), encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_discord_adapter_flow12: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(PORTED) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_discord_adapter_flow12: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(PORTED))
        if unknown_funcs:
            print(f"FATAL oracle_discord_adapter_flow12: fixture funcs not in targets: {unknown_funcs}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got):
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:800]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:800]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:800]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:800]}")


def main():
    _verify_upstream_defs()
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            if shown < 200:
                report_mismatch(func, want, g)
                shown += 1
    print(f"ORACLE discord_adapter_flow12: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow12.jsonl"
    n = bad = 0
    counts = {}
    for line in open(str(fix), encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if case.get("_note") == "fail-closed":
            continue
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    print(f"SELFTEST discord_adapter_flow12: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
