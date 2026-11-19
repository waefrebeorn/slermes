#!/usr/bin/env python3
"""Oracle for the agent/credential_pool.py FLOW-core port
(src/agent/port_credential_pool_flow.c).

Drives the LIVE agent/credential_pool.py selection/rotation defs with SCRIPTED I/O:
every I/O sibling method and module-level dependency is a recording stub fed the same
scripts the C harness feeds its seams. Compares the DECISION SEQUENCE (ordered seam
names in "trace"), the return ("ret", entry flows projected to {id,last_status}), the
scalar pool state ("state", _entries excluded) and structural entry projections
("entries"/"seeder") for the seeding/pruning flows.

Live defs are AST-extracted (PooledCredential dataclass + module constants + helpers
seeded); the heavy quarry import chain is never imported. Pure siblings stay REAL so
nested decision logic is exercised, matching the C port. Fail-closed: 0 cases => exit 1.
Summary: 'ORACLE credential_pool_flow: N cases, M mismatches'.

Deterministic subset: non-nous providers (runtime_api_key == str(access_token));
aware ISO timestamps; errnos/exc codes carried symbolically.
"""
import ast
import copy
import json
import os
import sys
import types
import uuid
from dataclasses import dataclass, fields, replace
from datetime import datetime, timezone, UTC
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "credential_pool.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "credential_pool.py")
TS_SRC = str(QUARRY / "agent" / "credential_pool_timestamps.py")

PY = {
    "is_terminal_auth_failure": "_is_terminal_auth_failure",
    "is_sole_credential": "_is_sole_credential",
    "next_priority": "_next_priority",
    "is_manual_source": "_is_manual_source",
    "exhausted_ttl": "_exhausted_ttl",
    "exhausted_until": "_exhausted_until",
    "singleton_predates_entry": "_singleton_predates_entry",
    "codex_principal_identity": "_codex_principal_identity",
    "codex_entry_tracks_singleton": "_codex_entry_tracks_singleton",
    "norm_url": "_norm_url",
    "normalize_custom_pool_name": "_normalize_custom_pool_name",
    "store_owns_pool_provider": "_store_owns_pool_provider",
    "retain_sources_not_in": "_retain_sources_not_in",
    "normalize_pool_priorities": "_normalize_pool_priorities",
    "prune_stale_seeded_entries": "_prune_stale_seeded_entries",
    "get_env_prefer_dotenv": "get_env_prefer_dotenv",
    "env_payload": "_env_payload",
    "env_key_var_candidates": "_env_key_var_candidates",
    "find": "_find",
    "replace_entry": "_replace_entry",
    "adopt": "_adopt",
    "quarantine_sources": "_quarantine_sources",
    "mark_exhausted": "_mark_exhausted",
    "available_entries": "_available_entries",
    "lift_reopened_cooldowns": "lift_reopened_cooldowns",
    "reclaim": "reclaim",
    "identify_failed_entry": "_identify_failed_entry",
    "rotate_unmatched": "_rotate_unmatched",
    "entry_needs_refresh": "_entry_needs_refresh",
    "reset_cleared_after": "_reset_cleared_after",
    "resync_stale_entry": "_resync_stale_entry",
    "refresh_pending_entries": "_refresh_pending_entries",
    "try_refresh_current_unlocked": "_try_refresh_current_unlocked",
    "refresh_entry": "_refresh_entry",
    "mark_dead_refresh_grant": "_mark_dead_refresh_grant",
    "fail_closed_unpersisted_rotation": "_fail_closed_unpersisted_rotation",
    "recover_failed_refresh": "_recover_failed_refresh",
    "upsert_entry": "_upsert_entry",
    "seeder_upsert": "_Seeder.upsert",
}

# Methods the C port calls as SEAMS per target (recorder, not real).
SEAM_METHODS = {
    "available_entries": {"_resync_stale_entry", "_codex_quota_restored_upstream",
                          "_entry_needs_refresh", "_refresh_entry", "_find", "_adopt", "_persist"},
    "lift_reopened_cooldowns": {"_resync_stale_entry", "_codex_quota_restored_upstream",
                                "_entry_needs_refresh", "_refresh_entry", "_find", "_adopt", "_persist"},
    "reclaim": {"_resync_stale_entry", "_codex_quota_restored_upstream", "_entry_needs_refresh",
                "_refresh_entry", "_find", "_adopt", "_persist", "_refresh_pending_entries"},
    "rotate_unmatched": {"_resync_stale_entry", "_codex_quota_restored_upstream", "_entry_needs_refresh",
                         "_refresh_entry", "_find", "_adopt", "_persist", "_select_unlocked"},
    "mark_exhausted": {"_persist"},
    "adopt": {"_persist"},
    "quarantine_sources": {"_persist"},
    "resync_stale_entry": {"_persist", "_sync_anthropic_entry_from_credentials_file",
                           "_sync_nous_entry_from_auth_store", "_sync_entry_from_auth_store"},
    "reset_cleared_after": set(),
    "refresh_entry": {"_refresh_entry_impl", "_sync_entry_from_auth_store", "_sync_entry_from_pool_store",
                      "_sync_anthropic_entry_from_credentials_file", "_claude_code_credentials_lock",
                      "_single_use_refresh_lock_timeout", "_entry_needs_refresh", "_persist"},
    "entry_needs_refresh": set(),
    "recover_failed_refresh": {"_sync_anthropic_entry_from_credentials_file", "_sync_entry_from_pool_store",
                               "_sync_entry_from_auth_store", "_sync_nous_entry_from_auth_store",
                               "_commit_anthropic_rotation", "_clear_terminal_tokens_state",
                               "_clear_terminal_nous_state", "_sync_device_code_entry_to_auth_store", "_persist"},
    "mark_dead_refresh_grant": {"_persist"},
    "fail_closed_unpersisted_rotation": {"_persist"},
    "try_refresh_current_unlocked": {"_refresh_entry"},
    "refresh_pending_entries": {"_refresh_entry"},
    "identify_failed_entry": set(),
    "replace_entry": set(),
    "find": set(),
    "is_sole_credential": set(),
    "is_terminal_auth_failure": set(),
}

# Module-level I/O names recorded as seams.
IO_NAMES = {
    "_normalize_error_context", "model_cooldown_until", "plugin_refresh_hook",
    "plugin_row_is_expiring", "_codex_access_token_is_expiring",
    "_xai_proactive_refresh_skew_seconds", "_xai_access_token_is_expiring",
    "read_credential_pool", "_decode_jwt_claims", "is_borrowed_credential_source",
    "load_env", "_get_secret", "get_secret_source", "fingerprint_secret_value",
    "uuid_hex6", "is_suppressed", "redact_error_text", "refresh_anthropic_oauth_pure",
    "is_terminal_anthropic_refresh_error", "_is_terminal_codex_oauth_refresh_error",
    "_is_terminal_xai_oauth_refresh_error", "_is_terminal_nous_refresh_error",
    "recover_failed_plugin_refresh", "spent_rotation_source_path",
    "mark_rotation_consumed_uncommitted", "_auth_store_lock", "write_credential_pool",
}
# module functions kept REAL (pure)
PURE_MODULE = {"_is_manual_source", "_exhausted_until", "_exhausted_ttl", "_next_priority",
               "_normalize_pool_auth_type", "_parse_absolute_timestamp", "_norm_url",
               "_normalize_custom_pool_name", "_store_owns_pool_provider"}


def make_exc(spec):
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if t == "TimeoutError":
        e = TimeoutError(msg)
    elif t == "ValueError":
        e = ValueError(msg)
    elif t == "RuntimeError":
        e = RuntimeError(msg)
    else:
        e = Exception(msg)
    if spec.get("code") is not None:
        e.code = spec["code"]
    return e


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state", {}) or {})
        self.seam_methods = set()

    def script_next(self, name):
        entry = self.io.get(name)
        if entry is None:
            return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return None
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def recorder(self, name):
        ctx = self
        def call(*args, **kwargs):
            ctx.trace.append(name)
            result = ctx.script_next(name)
            if isinstance(result, dict):
                eff = result.get("state")
                if isinstance(eff, dict):
                    ctx.state.update(eff)
                raise_spec = result.get("raise")
                if isinstance(raise_spec, dict):
                    raise make_exc(raise_spec)
                ret = result.get("ret")
            else:
                ret = result
            return convert_seam_ret(name, ret, args, ctx)
        return call


CURRENT = {"rec": None}


def _stub_fn(name):
    def f(*a, **k):
        return CURRENT["rec"](name)(*a, **k)
    return f


def install_module_stubs():
    agent = sys.modules.setdefault("agent", types.ModuleType("agent"))
    agent.__path__ = []
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m
    mod("agent.credential_pool_errors", _normalize_error_context=_stub_fn("_normalize_error_context"),
        redact_error_text=_stub_fn("redact_error_text"))
    mod("agent.credential_pool_model_cooldowns", model_cooldown_until=_stub_fn("model_cooldown_until"))
    mod("agent.credential_pool_plugin", plugin_row_is_expiring=_stub_fn("plugin_row_is_expiring"),
        recover_failed_plugin_refresh=_stub_fn("recover_failed_plugin_refresh"),
        apply_plugin_refresh_result=_stub_fn("apply_plugin_refresh_result"))
    mod("agent.credential_pool_timestamps", _parse_absolute_timestamp=None)  # bound real later
    mod("agent.credential_persistence", read_credential_pool=_stub_fn("read_credential_pool"),
        write_credential_pool=_stub_fn("write_credential_pool"),
        sanitize_borrowed_credential_payload=_stub_fn("sanitize_borrowed_credential_payload"),
        is_borrowed_credential_source=_stub_fn("is_borrowed_credential_source"),
        fingerprint_secret_value=_stub_fn("fingerprint_secret_value"))
    mod("agent.anthropic_credentials", refresh_anthropic_oauth_pure=_stub_fn("refresh_anthropic_oauth_pure"),
        is_terminal_anthropic_refresh_error=_stub_fn("is_terminal_anthropic_refresh_error"),
        spent_rotation_source_path=_stub_fn("spent_rotation_source_path"),
        mark_rotation_consumed_uncommitted=_stub_fn("mark_rotation_consumed_uncommitted"),
        is_rotation_consumed_uncommitted=_stub_fn("is_rotation_consumed_uncommitted"))
    mod("hermes_cli.auth_plugin_providers", plugin_refresh_hook=_stub_fn("plugin_refresh_hook"))
    auth = mod("hermes_cli.auth", _is_terminal_codex_oauth_refresh_error=_stub_fn("_is_terminal_codex_oauth_refresh_error"),
               _is_terminal_xai_oauth_refresh_error=_stub_fn("_is_terminal_xai_oauth_refresh_error"),
               _is_terminal_nous_refresh_error=_stub_fn("_is_terminal_nous_refresh_error"),
               _codex_access_token_is_expiring=_stub_fn("_codex_access_token_is_expiring"),
               _xai_access_token_is_expiring=_stub_fn("_xai_access_token_is_expiring"),
               _xai_proactive_refresh_skew_seconds=_stub_fn("_xai_proactive_refresh_skew_seconds"),
               _auth_store_lock=_stub_fn("_auth_store_lock"),
               _load_auth_store=_stub_fn("_load_auth_store"),
               _save_auth_store=_stub_fn("_save_auth_store"),
               NOUS_DEVICE_CODE_SOURCE="device_code")
    mod("hermes_cli.env_loader", get_secret_source=_stub_fn("get_secret_source"))
    mod("agent.secret_scope", get_secret=_stub_fn("_get_secret"))
    hc = mod("hermes_cli.config", load_env=_stub_fn("load_env"))
    return auth


AUTH_MOD = install_module_stubs()


def extract():
    import re as _re
    import logging as _logging
    import threading as _threading
    import time as _time
    ns = {
        "__name__": "agent.credential_pool",
        "re": _re, "os": os, "json": json, "sys": sys, "uuid": uuid,
        "logging": _logging, "logger": _logging.getLogger("agent.credential_pool"),
        "threading": _threading, "time": _time,
        "dataclass": dataclass, "fields": fields, "replace": replace,
        "datetime": datetime, "timezone": timezone, "UTC": UTC, "Path": Path,
        "Any": object, "Optional": object, "Callable": object, "Iterable": object,
        "Dict": dict, "List": list, "Set": set, "Tuple": tuple,
        "OPENROUTER_BASE_URL": "https://openrouter.ai/api/v1",
    }
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # PooledCredential dataclass + _Seeder + CredentialPool methods
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("PooledCredential", "_Seeder"):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    methods = {}
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "CredentialPool":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef):
                    sub.decorator_list = []
                    try:
                        exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec"), ns)
                        methods[sub.name] = ns[sub.name]
                    except Exception:
                        pass
    # _parse_absolute_timestamp real from timestamps module
    tsrc = open(TS_SRC, encoding="utf-8").read()
    ttree = ast.parse(tsrc)
    for node in ttree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_parse_absolute_timestamp":
            exec(compile(ast.Module(body=[node], type_ignores=[]), TS_SRC, "exec"), ns)
    sys.modules["agent.credential_pool_timestamps"]._parse_absolute_timestamp = ns["_parse_absolute_timestamp"]
    ns["_METHODS"] = methods
    missing = [v for v in PY.values() if "." not in v and v not in ns and v not in methods]
    if missing:
        print("FATAL credential_pool_flow: extraction missing names:", sorted(missing))
        sys.exit(2)
    return ns


NS = extract()
PC = NS["PooledCredential"]


def mk_entry(provider, d):
    d = d or {}
    return PC(
        provider=provider, id=d.get("id", ""), label=d.get("label", ""),
        auth_type=d.get("auth_type", "api_key"), priority=d.get("priority", 0),
        source=d.get("source", "manual"), access_token=d.get("access_token", ""),
        refresh_token=d.get("refresh_token"), last_status=d.get("last_status"),
        last_status_at=d.get("last_status_at"), last_error_code=d.get("last_error_code"),
        last_error_reason=d.get("last_error_reason"), last_error_message=d.get("last_error_message"),
        last_error_reset_at=d.get("last_error_reset_at"), status_cleared_at=d.get("status_cleared_at"),
        base_url=d.get("base_url"), expires_at=d.get("expires_at"), expires_at_ms=d.get("expires_at_ms"),
        last_refresh=d.get("last_refresh"), inference_base_url=d.get("inference_base_url"),
        agent_key=d.get("agent_key"), agent_key_expires_at=d.get("agent_key_expires_at"),
        request_count=d.get("request_count", 0), model_cooldowns=d.get("model_cooldowns"),
        extra=d.get("extra") or {},
    )


def entry_proj(e):
    if e is None:
        return None
    return {"id": getattr(e, "id", ""), "last_status": getattr(e, "last_status", None)}


def entry_struct(e):
    if isinstance(e, dict):
        g = e.get
    else:
        g = lambda k, d=None: getattr(e, k, d)
    return {"id": g("id", ""), "source": g("source", ""), "priority": g("priority", 0),
            "label": g("label", ""), "access_token": g("access_token", ""),
            "last_status": g("last_status", None)}


# Entry-returning method seams: convert scripted dicts back into PooledCredential
# objects so the live decision logic (attribute access / identity) runs faithfully.
ENTRY_SEAMS = {"_find", "_adopt", "_refresh_entry", "_refresh_entry_impl",
               "_sync_entry_from_auth_store", "_sync_entry_from_pool_store",
               "_sync_anthropic_entry_from_credentials_file", "_sync_nous_entry_from_auth_store"}


def convert_seam_ret(name, ret, args, ctx):
    provider = ctx.state.get("provider", "")
    if name == "_resync_stale_entry":
        # null script => unchanged: return the SAME entry object (identity preserved,
        # matching the C value-equality "no change" path); dict => a new (changed) entry.
        if ret is None:
            return args[0] if args else None
        if isinstance(ret, dict):
            return mk_entry(provider, ret)
        return ret
    if name == "_select_unlocked":
        if isinstance(ret, list) and ret:
            e = mk_entry(provider, ret[0]) if isinstance(ret[0], dict) else ret[0]
            pend = ret[1] if len(ret) > 1 else []
            return (e, pend)
        return (None, [])
    if name in ENTRY_SEAMS:
        if isinstance(ret, dict):
            return mk_entry(provider, ret)
        return ret
    return ret


class StubSelf:
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)
    def __getattr__(self, name):
        ctx = object.__getattribute__(self, "_ctx")
        if name in ctx.seam_methods:
            return ctx.recorder(name)
        methods = NS["_METHODS"]
        if name in methods:
            return types.MethodType(methods[name], self)
        if name in ctx.state:
            return ctx.state[name]
        if name in ("_borrowed_root_ids",):
            return set()
        if name in ("_persisted_token_pairs", "_active_leases"):
            return {}
        if name == "_lock":
            import contextlib
            return contextlib.nullcontext()
        if name == "_max_concurrent":
            return 1
        if name == "_last_no_entries_log_at":
            return None
        raise AttributeError(name)
    def __setattr__(self, name, value):
        ctx = object.__getattribute__(self, "_ctx")
        ctx.state[name] = value


class _Clock:
    def __init__(self, ctx):
        self._ctx = ctx
    def time(self):
        return self._ctx.recorder("time.time")()
    def sleep(self, s):
        return self._ctx.recorder("time.sleep")(s)
    def monotonic(self):
        return self._ctx.recorder("time.monotonic")()


class _Logger:
    def __init__(self, ctx):
        self._ctx = ctx
    def __getattr__(self, level):
        return self._ctx.recorder("logger." + level)


def seed_case(ctx, inp, func):
    NS["time"] = _Clock(ctx)
    NS["logger"] = _Logger(ctx)
    CURRENT["rec"] = ctx.recorder
    ctx.seam_methods = set(SEAM_METHODS.get(func, set()))
    for nm in IO_NAMES:
        NS[nm] = ctx.recorder(nm)
    NS["auth_mod"] = AUTH_MOD
    # auth_mod attributes that are seams
    for nm in ("_is_terminal_codex_oauth_refresh_error", "_is_terminal_xai_oauth_refresh_error",
               "_is_terminal_nous_refresh_error", "_codex_access_token_is_expiring",
               "_xai_access_token_is_expiring", "_xai_proactive_refresh_skew_seconds"):
        setattr(AUTH_MOD, nm, ctx.recorder(nm))
    # _auth_store_lock context manager
    class _Lock:
        def __enter__(self):
            ctx.recorder("_auth_store_lock.enter")()
            return self
        def __exit__(self, *a):
            ctx.recorder("_auth_store_lock.exit")()
            return False
    NS["_auth_store_lock"] = lambda *a, **k: _Lock()
    AUTH_MOD._auth_store_lock = lambda *a, **k: _Lock()
    # claude_code_credentials_lock method seam -> context manager
    class _CLock:
        def __enter__(self):
            ctx.recorder("_claude_code_credentials_lock.enter")()
            return self
        def __exit__(self, *a):
            ctx.recorder("_claude_code_credentials_lock.exit")()
            return False
    ctx.seam_methods.discard("_claude_code_credentials_lock")
    NS["_METHODS"]["_claude_code_credentials_lock"] = lambda self: _CLock()
    # entries
    provider = ctx.state.get("provider", "")
    ctx.state["_entries"] = [mk_entry(provider, d) for d in ctx.state.get("_entries", [])]
    # from_dict / to_dict seams for upsert
    def from_dict(cls, prov, payload):
        r = ctx.recorder("from_dict")(prov, payload)
        return mk_entry(prov, r if isinstance(r, dict) else payload)
    PC.from_dict = classmethod(from_dict)
    PC.to_dict = lambda self: ctx.recorder("to_dict")(self)
    NS["uuid_hex6"] = ctx.recorder("uuid_hex6")
    # live _upsert_entry uses uuid.uuid4().hex[:6]; record it as the uuid_hex6 seam
    NS["uuid"] = types.SimpleNamespace(
        uuid4=lambda: types.SimpleNamespace(hex=(ctx.recorder("uuid_hex6")() or "") + "000000"))


def scalar_state(ctx, inp):
    return {k: ctx.state[k] for k in (inp.get("state") or {}) if k != "_entries" and k in ctx.state}


def run_case(func, inp):
    ctx = CaseCtx(inp)
    seed_case(ctx, inp, func)
    out = {"ret": None, "trace": ctx.trace}
    py = PY[func]
    provider = ctx.state.get("provider", "")
    entries = ctx.state.get("_entries", [])
    self = StubSelf(ctx)
    methods = NS["_METHODS"]

    def call_method(name, *a, **k):
        return methods[name](self, *a, **k)

    if func == "is_terminal_auth_failure":
        out["ret"] = call_method("_is_terminal_auth_failure", inp.get("status_code"), inp.get("normalized_error") or {})
    elif func == "is_sole_credential":
        out["ret"] = call_method("_is_sole_credential")
    elif func == "next_priority":
        out["ret"] = NS[py]([mk_entry(provider, d) for d in inp.get("entries", [])])
    elif func == "is_manual_source":
        out["ret"] = NS[py](inp.get("source", ""))
    elif func == "exhausted_ttl":
        out["ret"] = NS[py](inp.get("error_code"), sole_credential=inp.get("sole_credential", False),
                            failure_reason=inp.get("failure_reason"))
    elif func == "exhausted_until":
        e = mk_entry(provider, inp.get("entry"))
        out["ret"] = NS[py](e, sole_credential=inp.get("sole_credential", False))
    elif func == "singleton_predates_entry":
        out["ret"] = NS[py](inp.get("state"), mk_entry(provider, inp.get("entry")))
    elif func == "codex_principal_identity":
        r = NS[py](inp.get("access_token"))
        out["ret"] = list(r) if r is not None else None
    elif func == "codex_entry_tracks_singleton":
        out["ret"] = NS[py](mk_entry(provider, inp.get("entry")), inp.get("singleton_tokens") or {})
    elif func == "norm_url":
        out["ret"] = NS[py](inp.get("url"))
    elif func == "normalize_custom_pool_name":
        out["ret"] = NS[py](inp.get("name", ""))
    elif func == "store_owns_pool_provider":
        out["ret"] = NS[py](inp.get("auth_store") or {}, inp.get("provider", ""))
    elif func == "retain_sources_not_in":
        es = [mk_entry(provider, d) for d in inp.get("entries", [])]
        out["ret"] = NS[py](es, set(inp.get("drop", [])))
        out["entries"] = [entry_struct(e) for e in es]
    elif func == "normalize_pool_priorities":
        es = [mk_entry(provider, d) for d in inp.get("entries", [])]
        out["ret"] = NS[py](inp.get("provider", ""), es)
        out["entries"] = [entry_struct(e) for e in es]
    elif func == "prune_stale_seeded_entries":
        es = [mk_entry(provider, d) for d in inp.get("entries", [])]
        out["ret"] = NS[py](es, set(inp.get("active_sources", [])),
                            prune_env_sources=inp.get("prune_env_sources", True))
        out["entries"] = [entry_struct(e) for e in es]
    elif func == "get_env_prefer_dotenv":
        out["ret"] = NS[py](inp.get("key", ""))
    elif func == "env_payload":
        out["ret"] = NS[py](env_var=inp.get("env_var", ""), token=inp.get("token", ""),
                            base_url=inp.get("base_url", ""))
    elif func == "env_key_var_candidates":
        es = [mk_entry(provider, d) for d in inp.get("entries", [])]
        out["ret"] = NS[py](inp.get("env_vars", []), es)
    elif func == "find":
        match = inp.get("match") or {}
        if "id" in match:
            pred = lambda e: e.id == match["id"]
        else:
            pred = lambda e: e.runtime_api_key == match.get("runtime_api_key")
        r = call_method("_find", pred)
        out["ret"] = r.id if r is not None else None
    elif func == "replace_entry":
        old = mk_entry(provider, {"id": inp.get("old_id", "")})
        new = mk_entry(provider, inp.get("new_entry"))
        call_method("_replace_entry", old, new)
        out["ret"] = None
    elif func == "adopt":
        e = mk_entry(provider, inp.get("entry"))
        r = call_method("_adopt", e, persist=inp.get("persist", True), **(inp.get("updates") or {}))
        out["ret"] = entry_proj(r)
    elif func == "quarantine_sources":
        e = mk_entry(provider, inp.get("entry"))
        call_method("_quarantine_sources", e, set(inp.get("sources", [])))
        out["ret"] = None
    elif func == "mark_exhausted":
        e = mk_entry(provider, inp.get("entry"))
        r = call_method("_mark_exhausted", e, inp.get("status_code"), inp.get("error_context"),
                        persist=inp.get("persist", True), failure_reason=inp.get("failure_reason"))
        out["ret"] = entry_proj(r)
    elif func == "available_entries":
        avail, pending = call_method("_available_entries", clear_expired=inp.get("clear_expired", False),
                                     refresh=inp.get("refresh", False), model=inp.get("model"))
        out["ret"] = {"available": [e.id for e in avail], "pending": [e.id for e in pending]}
    elif func == "lift_reopened_cooldowns":
        out["ret"] = call_method("lift_reopened_cooldowns", model=inp.get("model"))
    elif func == "reclaim":
        r = call_method("reclaim", inp.get("credential_id", ""), model=inp.get("model"))
        out["ret"] = r.id if r is not None else None
    elif func == "identify_failed_entry":
        r = call_method("_identify_failed_entry", inp.get("credential_id") or None, inp.get("api_key_hint") or None)
        out["ret"] = r.id if r is not None else None
    elif func == "rotate_unmatched":
        r = call_method("_rotate_unmatched")
        out["ret"] = r.id if r is not None else None
    elif func == "entry_needs_refresh":
        out["ret"] = call_method("_entry_needs_refresh", mk_entry(provider, inp.get("entry")))
    elif func == "reset_cleared_after":
        out["ret"] = call_method("_reset_cleared_after", mk_entry(provider, inp.get("entry")))
    elif func == "resync_stale_entry":
        r = call_method("_resync_stale_entry", mk_entry(provider, inp.get("entry")))
        out["ret"] = entry_proj(r)
    elif func == "refresh_pending_entries":
        call_method("_refresh_pending_entries", [mk_entry(provider, d) for d in inp.get("pending", [])])
        out["ret"] = None
    elif func == "try_refresh_current_unlocked":
        r = call_method("_try_refresh_current_unlocked", inp.get("failed_key"))
        out["ret"] = entry_proj(r)
    elif func == "refresh_entry":
        r = call_method("_refresh_entry", mk_entry(provider, inp.get("entry")),
                        force=inp.get("force", False), failed_key=inp.get("failed_key"))
        out["ret"] = entry_proj(r)
    elif func == "mark_dead_refresh_grant":
        call_method("_mark_dead_refresh_grant", mk_entry(provider, inp.get("entry")), make_exc(inp.get("exc") or {}))
        out["ret"] = None
    elif func == "fail_closed_unpersisted_rotation":
        call_method("_fail_closed_unpersisted_rotation", mk_entry(provider, inp.get("entry")),
                    make_exc(inp.get("exc") or {}), store=inp.get("store", ""))
        out["ret"] = None
    elif func == "recover_failed_refresh":
        r = call_method("_recover_failed_refresh", mk_entry(provider, inp.get("entry")), make_exc(inp.get("exc") or {}))
        out["ret"] = entry_proj(r)
    elif func == "upsert_entry":
        es = [mk_entry(provider, d) for d in inp.get("entries", [])]
        payload = copy.deepcopy(inp.get("payload") or {})
        out["ret"] = NS[py](es, inp.get("provider", provider), inp.get("source", ""), payload)
        out["entries"] = [entry_struct(e) for e in es]
    elif func == "seeder_upsert":
        seeder = copy.deepcopy(inp.get("seeder") or {})
        es = [mk_entry(seeder.get("provider", provider), d) for d in seeder.get("entries", [])]
        seed_obj = NS["_Seeder"](seeder.get("provider", provider), es)
        seed_obj.changed = seeder.get("changed", False)
        seed_obj.active_sources = set(seeder.get("active_sources", []))
        seed_obj.is_suppressed = ctx.recorder("is_suppressed")
        r = seed_obj.upsert(inp.get("source", ""), copy.deepcopy(inp.get("payload") or {}))
        out["ret"] = r
        out["seeder"] = {"provider": seed_obj.provider, "changed": seed_obj.changed,
                         "active_sources": sorted(seed_obj.active_sources),
                         "entries": [entry_struct(e) for e in seed_obj.entries]}
    else:
        raise KeyError(func)

    out["state"] = scalar_state(ctx, inp)
    return out


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        func = c.get("func", "")
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            want = run_case(func, c.get("in") or {})
        except Exception as e:
            mism += 1
            print(f"PYERR {func}: {type(e).__name__}: {e}")
            continue
        got = c.get("out") or {}
        ok = (got.get("trace") == want.get("trace")
              and got.get("ret") == want.get("ret")
              and got.get("state") == want.get("state"))
        for k in ("entries", "seeder"):
            if k in want:
                ok = ok and got.get(k) == want.get(k)
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE credential_pool_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())