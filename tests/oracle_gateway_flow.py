#!/usr/bin/env python3
"""Oracle for the gateway/run.py message-handling FLOW cores (run_flow.c).

Drives the LIVE functions with SCRIPTED I/O: the functions are AST-extracted
from the quarry (no heavy gateway import), their module constants are seeded,
and every store/rendezvous/profile/clock/i18n/log dependency is replaced by a
recording monkeypatch stub returning the same scripted results the C harness
feeds its seams. Compares the DECISION SEQUENCE (ordered trace of seam calls)
plus the return value / mutated state, not just a final scalar.

Input: stdin JSON lines from t_port_gateway_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...], ...}}
Summary: ORACLE gateway_flow: N cases, M mismatches. Exit 1 on any mismatch,
and fail-closed (exit 1) when zero cases were compared.
"""
import ast
import copy
import enum
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "run.py")

WANTED = {
    "_normalize_empty_agent_response", "_context_overflow_reply",
    "_build_replay_entry", "_has_replayable_sidecar",
    "_command_origin_for_source", "_cached_agent_for_hygiene",
    "_get_or_create_pool", "_stop_pool", "_resolve_session_key_or_none",
    "_cron_profile_gate", "_cron_tick_profile_homes", "_handoff_watch_scopes",
    "_claim_host_gateway_role", "_owner_is_standalone",
    "_refuse_second_host_gateway", "_refresh_host_gateway_record",
    "_migrate_command",
}

# Per-case state shared by every recording stub.
CUR = {"func": None, "in": {}, "trace": [], "attach_calls": 0}


def rec(ev):
    CUR["trace"].append(ev)


def cin(key, default=None):
    return CUR["in"].get(key, default)


class RecLogger:
    def _log(self, level, fmt, *args):
        rec({"ev": "log", "level": level, "msg": fmt % args if args else fmt})

    def debug(self, fmt, *args, **kw):
        self._log("debug", fmt, *args)

    def warning(self, fmt, *args, **kw):
        self._log("warning", fmt, *args)

    def error(self, fmt, *args, **kw):
        self._log("error", fmt, *args)


class HostLockOutcome(enum.Enum):
    ACQUIRED = "acquired"
    HELD_BY_OTHER = "held-by-other"
    COULD_NOT_OPEN = "could-not-open"


def ns_obj(d):
    """JSON object -> attribute-access double (live code reads .home/.pid/...)."""
    if d is None:
        return None
    return types.SimpleNamespace(**d)


def obj_json(o):
    """Attribute double -> plain dict for trace comparison."""
    if o is None:
        return None
    if isinstance(o, dict):
        return o
    return vars(o)


def pstr(p):
    """Path -> POSIX string (str(Path) flips slashes on Windows; the C seams
    carry the fixture's forward-slash strings verbatim)."""
    if isinstance(p, Path):
        return p.as_posix()
    return str(p)


def extract():
    src = open(SRC, encoding="utf-8").read()
    # Annotation names are evaluated at def-exec time (no PEP 563 in run.py):
    # seed typing + the modules the signatures reference.
    import concurrent.futures
    import threading
    from contextlib import suppress
    from typing import Any, Callable, Optional
    ns = {"__name__": "gateway_run_extracted", "Path": Path, "os": os,
          "logger": RecLogger(), "Any": Any, "Callable": Callable,
          "Optional": Optional, "concurrent": types.SimpleNamespace(futures=concurrent.futures),
          "threading": threading, "suppress": suppress}
    tree = ast.parse(src)
    # Module constants the targets read (_ASSISTANT_REPLAY_FIELDS is an
    # AnnAssign). Regex constants referencing unimported helpers fail and are
    # skipped — the predicates that use them are scripted seams here.
    for node in tree.body:
        if isinstance(node, ast.Assign):
            # Plain name constants only: subscript assigns (os.environ[...]=)
            # would mutate the real process environment during seeding.
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        if isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in WANTED:
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec"), ns)
    # Constants imported by run.py from sibling modules (seed from the LIVE
    # files so a rename upstream surfaces as a mismatch, not a silent drift).
    for rel, names in (
        ("agent/message_metadata.py", {"MESSAGE_UID", "ABSORBED_MESSAGE_UIDS"}),
        ("agent/conversation_compression_archive.py",
         {"MERGED_DURABLE_ROWS", "UNNAMED_DURABLE_ROWS", "RETIRED_DURABLE_ROWS"}),
        ("agent/turn_failure_copy.py", {"SITE_FAILURE_CODES"}),
    ):
        p = os.path.join(QUARRY, rel)
        t2 = ast.parse(open(p, encoding="utf-8").read())
        for node in t2.body:
            targets = []
            if isinstance(node, ast.Assign):
                targets = [t.id for t in node.targets if isinstance(t, ast.Name)]
            elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
                targets = [node.target.id]
            if any(t in names for t in targets):
                exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec"), ns)
    return ns


def install_stubs(ns):
    live = {k: ns[k] for k in WANTED if k in ns}

    # ── module-level names the targets resolve from their globals ──
    ns["logger"] = RecLogger()
    ns["os"] = types.SimpleNamespace(getpid=lambda: cin("self_pid", 0))

    def t_stub(key, **kwargs):
        rec({"ev": "t", "key": key, "kwargs": kwargs})
        tpl = cin("i18n", {}).get(key, "")
        return tpl.format(**kwargs)
    ns["t"] = t_stub

    def print_stub(text):
        rec({"ev": "print", "text": text})
    ns["print"] = print_stub

    def home_stub():
        if cin("home_raises", False):
            rec({"ev": "hermes_home", "raised": True})
            raise RuntimeError("hermes home unavailable")
        h = cin("hermes_home", "")
        rec({"ev": "hermes_home", "ret": h})
        return h
    ns["get_hermes_home"] = home_stub

    def overflow_stub(agent_result, history_len):
        r = cin("overflow_verdict", False)
        rec({"ev": "is_context_overflow", "history_len": history_len, "ret": r})
        return r
    ns["is_context_overflow_failure_result"] = overflow_stub

    def provider_stub(text):
        r = False
        for row in cin("provider_verdicts", []):
            if row[0] == text:
                r = bool(row[1])
                break
        rec({"ev": "looks_like_provider_error", "text": text, "ret": r})
        return r
    ns["_looks_like_gateway_provider_error"] = provider_stub

    def hidden_stub(agent_result):
        r = cin("hidden_verdict", False)
        rec({"ev": "hidden_reasoning_incomplete", "ret": r})
        return r
    ns["_is_gateway_hidden_reasoning_incomplete_turn"] = hidden_stub

    def metadata_stub(msg):
        rec({"ev": "replay_metadata_fields"})
        return copy.deepcopy(cin("metadata_fields", {}))
    ns["replay_metadata_fields"] = metadata_stub

    def identity_stub(src, dst):
        rec({"ev": "copy_identity_fields", "src": dict(src)})
        dst.update(cin("identity_fields", {}))
    ns["copy_identity_fields"] = identity_stub

    def multiplex_stub(config):
        func = CUR["func"]
        if func == "handoff_watch_scopes" and cin("homes_raise", False):
            rec({"ev": "multiplex_homes", "raised": True})
            raise RuntimeError("resolver failed")
        homes = cin("homes", [])
        rec({"ev": "multiplex_homes", "ret": homes})
        pairs = []
        for name, home in homes:
            pairs.append((name, Path(home) if home is not None else None))
        return pairs
    ns["_multiplex_profile_homes"] = multiplex_stub

    # claim composes these module functions; script them on the claim path,
    # run the LIVE extracted defs when they are the case target.
    def owner_wrapper():
        if CUR["func"] == "claim_host_gateway_role":
            r = cin("owner_standalone", False)
            rec({"ev": "owner_is_standalone", "ret": r})
            return r
        return live["_owner_is_standalone"]()
    ns["_owner_is_standalone"] = owner_wrapper

    def refuse_wrapper(owner):
        if CUR["func"] == "claim_host_gateway_role":
            rec({"ev": "refuse_second_host_gateway", "owner": obj_json(owner)})
            raise SystemExit(75)
        return live["_refuse_second_host_gateway"](owner)
    ns["_refuse_second_host_gateway"] = refuse_wrapper

    def migrate_wrapper():
        if CUR["func"] in ("claim_host_gateway_role", "refuse_second_host_gateway"):
            m = cin("migrate_command", "hermes gateway migrate --multiplex")
            rec({"ev": "migrate_command", "ret": m})
            return m
        return live["_migrate_command"]()
    ns["_migrate_command"] = migrate_wrapper

    # ── fake modules for the targets' function-local imports ──
    def claim_lock_stub(role):
        if cin("claim_raises", False):
            rec({"ev": "claim_host_lock", "role": role, "raised": True})
            raise RuntimeError("rendezvous failed")
        oc = cin("claim_outcome", "held-by-other")
        err = cin("claim_error")
        rec({"ev": "claim_host_lock", "role": role, "outcome": oc, "error": err})
        return (HostLockOutcome(oc), err)

    def publish_stub(role, profiles=(), home=""):
        rec({"ev": "publish_record", "role": role, "profiles": list(profiles), "home": home})

    def cleanup_stub(role):
        rec({"ev": "cleanup_on_exit", "role": role})

    def read_record_stub(role, include_stale=False):
        if cin("read_record_raises", False):
            rec({"ev": "read_record", "role": role, "include_stale": include_stale, "raised": True})
            raise RuntimeError("record unreadable")
        owner = cin("read_record")
        rec({"ev": "read_record", "role": role, "include_stale": include_stale, "ret": owner})
        return ns_obj(owner)

    def describe_stub(owner):
        d = cin("describe_ret", "")
        rec({"ev": "describe", "ret": d})
        return d

    def owns_stub(role):
        if cin("owns_raises", False):
            rec({"ev": "owns_host_lock", "role": role, "raised": True})
            raise RuntimeError("lock probe failed")
        r = cin("owns", False)
        rec({"ev": "owns_host_lock", "role": role, "ret": r})
        return r

    hr = types.ModuleType("gateway.host_rendezvous")
    hr.ROLE_GATEWAY = "gateway"
    hr.HostLockOutcome = HostLockOutcome
    hr.claim_host_lock = claim_lock_stub
    hr.publish_record = publish_stub
    hr.cleanup_on_exit = cleanup_stub
    hr.read_record = read_record_stub
    hr.describe = describe_stub
    hr.owns_host_lock = owns_stub

    def attach_decision_stub(home, owner):
        CUR["attach_calls"] += 1
        dec = cin("attach_decision") if CUR["attach_calls"] == 1 else cin("attach_decision_recheck")
        rec({"ev": "attach_decision", "home": pstr(home), "owner": obj_json(owner), "ret": dec})
        return ns_obj(dec)

    def tenant_stub(owner_home, home):
        r = cin("tenant", False)
        rec({"ev": "launched_by_other_tenant", "owner_home": owner_home, "home": pstr(home), "ret": r})
        return r

    def host_gateway_stub(wait_for_channel=None):
        if CUR["func"] == "claim_host_gateway_role":
            owner = cin("live_owner")
            rec({"ev": "host_gateway", "wait_for_channel": wait_for_channel, "ret": owner})
            return ns_obj(owner)
        if cin("host_gateway_raises", False):
            rec({"ev": "host_gateway", "raised": True})
            raise RuntimeError("host probe failed")
        owner = cin("owner")
        rec({"ev": "host_gateway", "ret": owner})
        return ns_obj(owner)

    def profile_name_stub(home):
        if cin("name_raises", False):
            rec({"ev": "profile_name_for_home", "home": pstr(home), "raised": True})
            raise RuntimeError("name resolution failed")
        n = cin("profile_name", "")
        rec({"ev": "profile_name_for_home", "home": pstr(home), "ret": n})
        return n

    def serve_stub(name, owner=None):
        if cin("serve_raises", False):
            rec({"ev": "request_serve_profile", "name": name, "raised": True})
            raise RuntimeError("serve probe failed")
        ans = cin("answered")
        rec({"ev": "request_serve_profile", "name": name, "ret": ans})
        return ns_obj(ans)

    ha = types.ModuleType("gateway.host_attach")
    ha.REFUSE = "refuse"
    ha.START = "start"
    ha.ATTACH = "attach"
    ha.REPLACE_HOST = "replace-host"
    ha.ATTACH_CHANNEL_WAIT_S = 5.0
    ha.standalone_attach_decision = attach_decision_stub
    ha.launched_by_other_tenant = tenant_stub
    ha.host_gateway = host_gateway_stub
    ha.profile_name_for_home = profile_name_stub
    ha.request_serve_profile = serve_stub

    restart = types.ModuleType("gateway.restart")
    restart.GATEWAY_SERVICE_RESTART_EXIT_CODE = 75

    def liveness_stub(profile_dir=None, use_cache=False, pid_probe=None):
        home = pstr(profile_dir)
        if cin("probe_raises", False):
            rec({"ev": "resolve_liveness", "home": home, "raised": True})
            raise RuntimeError(cin("probe_error", "probe failed"))
        liv = cin("liveness")
        rec({"ev": "resolve_liveness", "home": home, "ret": liv})
        return ns_obj(liv)

    status = types.ModuleType("gateway.status")
    status.resolve_gateway_liveness = liveness_stub
    status.get_running_pid = lambda path, cleanup_stale=False: None

    def active_stub():
        a = cin("active")
        rec({"ev": "active_profile", "ret": a})
        return a

    def profile_dir_stub(name):
        if cin("dir_raises", False):
            rec({"ev": "profile_dir", "name": name, "raised": True})
            raise RuntimeError("profile dir failed")
        d = cin("profile_dir")
        rec({"ev": "profile_dir", "name": name, "ret": d})
        return Path(d)

    def standalone_stub(home):
        r = cin("standalone_profile", False)
        rec({"ev": "profile_is_standalone", "home": pstr(home), "ret": r})
        return r

    profiles = types.ModuleType("hermes_cli.profiles")
    profiles.get_active_profile_name = active_stub
    profiles.get_profile_dir = profile_dir_stub
    profiles.profile_is_standalone = standalone_stub

    migrate = types.ModuleType("hermes_cli.gateway_migrate")
    migrate.MIGRATE_COMMAND = "hermes gateway migrate --multiplex"

    tfc = types.ModuleType("agent.turn_failure_copy")
    tfc.SITE_FAILURE_CODES = ns["SITE_FAILURE_CODES"]

    gateway_pkg = types.ModuleType("gateway")
    gateway_pkg.host_rendezvous = hr
    gateway_pkg.host_attach = ha
    gateway_pkg.restart = restart
    gateway_pkg.status = status
    hermes_cli_pkg = types.ModuleType("hermes_cli")
    hermes_cli_pkg.profiles = profiles
    hermes_cli_pkg.gateway_migrate = migrate
    agent_pkg = types.ModuleType("agent")
    agent_pkg.turn_failure_copy = tfc
    for name, mod in (
        ("gateway", gateway_pkg), ("gateway.host_rendezvous", hr),
        ("gateway.host_attach", ha), ("gateway.restart", restart),
        ("gateway.status", status), ("hermes_cli", hermes_cli_pkg),
        ("hermes_cli.profiles", profiles), ("hermes_cli.gateway_migrate", migrate),
        ("agent", agent_pkg), ("agent.turn_failure_copy", tfc),
    ):
        sys.modules[name] = mod
    return live


def truthy(v):
    return bool(v)


def norm_result(x):
    """Path/tuple normalization for scope-list results."""
    if isinstance(x, Path):
        return x.as_posix()
    if isinstance(x, tuple):
        return [norm_result(v) for v in x]
    if isinstance(x, list):
        return [norm_result(v) for v in x]
    return x


def canon(x, ordered):
    if isinstance(x, dict):
        items = [(k, canon(v, ordered)) for k, v in x.items()]
        if not ordered:
            items = sorted(items, key=lambda kv: kv[0])
        return ("obj", items)
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v, ordered) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def drive(ns, func, inp):
    """Run the LIVE target with scripted seams; return the expected out dict."""
    if func == "normalize_empty_agent_response":
        ret = ns["_normalize_empty_agent_response"](
            inp.get("agent_result", {}), inp.get("response", ""),
            history_len=inp.get("history_len", 0))
        return {"ret": ret}

    if func == "build_replay_entry":
        ret = ns["_build_replay_entry"](
            inp.get("role", ""), inp.get("content"), inp.get("msg", {}),
            preserve_timestamp=inp.get("preserve_timestamp", False))
        return {"ret": ret}

    if func == "has_replayable_sidecar":
        ret = ns["_has_replayable_sidecar"](
            inp.get("role"), inp.get("content"), inp.get("msg", {}))
        return {"ret": ret}

    if func == "command_origin_for_source":
        sj = inp.get("source", {})

        class Src:
            pass
        if inp.get("platform_raises", False):
            def _boom(self):
                raise RuntimeError("platform unreadable")
            Src.platform = property(_boom)
        s = Src()
        if "platform" in sj and not inp.get("platform_raises", False):
            p = sj["platform"]
            s.platform = types.SimpleNamespace(**p) if isinstance(p, dict) else p
        for k in ("chat_id", "chat_name", "thread_id"):
            if k in sj:
                setattr(s, k, sj[k])
        return {"ret": ns["_command_origin_for_source"](s)}

    if func == "cached_agent_for_hygiene":
        gj = inp.get("gateway", {})

        class LockDouble:
            def __enter__(self):
                rec({"ev": "lock_acquire"})
                return self

            def __exit__(self, *exc):
                rec({"ev": "lock_release"})
                return False

        class CacheDouble:
            def get(self, key):
                if inp.get("cache_get_raises", False):
                    rec({"ev": "cache_get", "key": key, "raised": True})
                    raise RuntimeError("cache read failed")
                if "entry_tuple" in inp:
                    entry = tuple(inp["entry_tuple"])
                    is_tuple = True
                else:
                    entry = inp.get("entry_value")
                    is_tuple = False
                rec({"ev": "cache_get", "key": key,
                     "ret": list(entry) if is_tuple else entry, "is_tuple": is_tuple})
                return entry

        gw = types.SimpleNamespace()
        if gj.get("_agent_cache") is not None:
            gw._agent_cache = CacheDouble()
        if truthy(gj.get("_agent_cache_lock")):
            gw._agent_cache_lock = LockDouble()
        ret = ns["_cached_agent_for_hygiene"](gw, inp.get("session_key", ""))
        if isinstance(ret, tuple):
            ret = list(ret)
        return {"ret": ret}

    if func == "get_or_create_pool":
        st = inp.get("state", {})
        attr = inp.get("attr", "_executor")

        def exec_obj(d):
            return types.SimpleNamespace(_shutdown=d.get("_shutdown", False), _desc=d)

        class PoolLockDouble:
            # Silent context manager: the C port models the executor lock as a
            # state flag (no acquire/release seam events). object() is NOT a
            # context manager and breaks live `with lock:`.
            def __enter__(self):
                return self

            def __exit__(self, *exc):
                return False

        runner = types.SimpleNamespace()
        if st.get("_executor_lock") is not None:
            runner._executor_lock = PoolLockDouble()
        if "_executor_closing" in st:
            runner._executor_closing = st["_executor_closing"]
        if st.get(attr) is not None:
            setattr(runner, attr, exec_obj(st[attr]))

        def make_pool():
            desc = inp.get("make_pool_ret", {})
            rec({"ev": "make_pool", "ret": desc})
            return exec_obj(desc)

        try:
            ret = ns["_get_or_create_pool"](runner, attr, make_pool)
        except RuntimeError as e:
            rec({"ev": "runtime_error", "msg": str(e)})
            ret = None
        out_state = dict(st)
        if getattr(runner, "_executor_lock", None) is not None:
            out_state["_executor_lock"] = True
        if hasattr(runner, attr):
            ex = getattr(runner, attr)
            out_state[attr] = ex._desc if ex is not None else None
        return {"ret": ret._desc if ret is not None else None, "state": out_state}

    if func == "stop_pool":
        desc = inp.get("executor")

        class ExecDouble:
            def __init__(self, d):
                self._threads = d.get("_threads")

            def shutdown(self, wait=False, cancel_futures=False):
                raised = cancel_futures and inp.get("shutdown_typeerror", False)
                rec({"ev": "shutdown", "wait": wait, "cancel_futures": cancel_futures,
                     "raised": "TypeError" if raised else None})
                if raised:
                    raise TypeError("unexpected keyword argument 'cancel_futures'")

        ex = ExecDouble(desc) if desc is not None else None
        return {"ret": ns["_stop_pool"](ex)}

    if func == "resolve_session_key_or_none":
        def derive(source):
            if inp.get("derive_raises", False):
                rec({"ev": "session_key_for_source", "raised": True})
                raise RuntimeError("key derivation failed")
            r = inp.get("derive_ret", "")
            rec({"ev": "session_key_for_source", "ret": r})
            return r

        runner = types.SimpleNamespace(_session_key_for_source=derive)
        ret = ns["_resolve_session_key_or_none"](runner, inp.get("source"), inp.get("session_key"))
        return {"ret": ret}

    if func == "cron_profile_gate":
        ret = ns["_cron_profile_gate"](inp.get("name", ""), Path(inp.get("home", "")))
        return {"ret": ret}

    if func == "cron_tick_profile_homes":
        ret = ns["_cron_tick_profile_homes"](inp.get("config"))
        return {"ret": norm_result(ret)}

    if func == "handoff_watch_scopes":
        rj = inp.get("runner", {})
        runner = types.SimpleNamespace()
        if "config" in rj:
            cfg = rj["config"]
            runner.config = types.SimpleNamespace(
                multiplex_profiles=cfg.get("multiplex_profiles", False)) if cfg is not None else None
        ret = ns["_handoff_watch_scopes"](runner)
        return {"ret": norm_result(ret)}

    if func == "claim_host_gateway_role":
        try:
            ns["_claim_host_gateway_role"](force=inp.get("force", False))
            code = 0
        except SystemExit as e:
            code = int(e.code)
            rec({"ev": "system_exit", "code": code})
        return {"ret": None, "exit": code}

    if func == "owner_is_standalone":
        return {"ret": ns["_owner_is_standalone"]()}

    if func == "refuse_second_host_gateway":
        owner = ns_obj(inp.get("owner"))
        try:
            ns["_refuse_second_host_gateway"](owner)
            code = 0
        except SystemExit as e:
            code = int(e.code)
            rec({"ev": "system_exit", "code": code})
        return {"ret": None, "exit": code}

    if func == "refresh_host_gateway_record":
        rj = inp.get("runner", {})
        runner = types.SimpleNamespace()
        if "config" in rj:
            cfg = rj["config"]
            runner.config = types.SimpleNamespace(
                multiplex_profiles=cfg.get("multiplex_profiles", False)) if cfg is not None else None

        def served_names():
            if inp.get("served_raises", False):
                rec({"ev": "served_profile_names", "raised": True})
                raise RuntimeError("served set failed")
            names = inp.get("served_names", [])
            rec({"ev": "served_profile_names", "ret": names})
            return list(names)
        runner.served_profile_names = served_names
        ns["_refresh_host_gateway_record"](runner)
        return {"ret": None}

    raise ValueError("unknown func: %s" % func)


def main():
    ns = extract()
    install_stubs(ns)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["attach_calls"] = 0
        try:
            want = drive(ns, func, inp)
        except Exception as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        want["trace"] = CUR["trace"]
        # Dict key order is semantic for the constructed intake dicts; trace
        # event field order is incidental (compare unordered there).
        ordered = func in ("build_replay_entry", "command_origin_for_source")
        if ordered:
            ok = canon(want.get("ret"), True) == canon(got.get("ret"), True)
            ok = ok and canon({k: v for k, v in want.items() if k != "ret"}, False) == \
                canon({k: v for k, v in got.items() if k != "ret"}, False)
        else:
            ok = canon(want, False) == canon(got, False)
        if not ok:
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE gateway_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
