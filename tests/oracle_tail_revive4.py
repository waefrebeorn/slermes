#!/usr/bin/env python3
"""oracle_tail_revive4.py — differential oracle for the tail group-4 revival.

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the LIVE
quarry defs, seeds module constants (Assign + AnnAssign), drives each live
function under the SAME scripted seam results the C harness consumed, and
compares ordered trace + ret + post-state. BaseException is caught per case
(SystemExit included); missing/exhausted scripts fail closed on both sides.

Seam conventions (identical on both sides):
- In-function imports become "import_<name>" seams; a scripted ImportError
  models a failed import. The imported callable is the "<name>" seam.
- run=/sleep=/shutil.which/sys.platform/os.environ.get and object methods
  (probe, quiet_period, is_closing, done, stat, get_provider_auth_state,
  plugin_env_flag, human_wait_seconds, ...) are seams named in the drivers.
- Integral JSON numbers model Python ints, fractional ones floats; JSON
  arrays model lists (tuples where the live code returns tuples); objects
  model attribute carriers (SimpleNamespace) or dicts where noted.
- frozenset results normalize to SORTED arrays.

Gate:
  bash tests/run_one_oracle.sh tail_revive4 oracle_tail_revive4.py "" "lib/libjson/json.o"
"""
import ast
import builtins
import copy
import __future__
import hashlib
import hmac
import json
import math
import os
import re
import sys
import types
from pathlib import Path
from urllib.parse import urlparse

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home_t4"))

FUT = __future__.annotations.compiler_flag
MISSING = object()


def norm(x):
    """Integral floats -> ints; tuples/sets -> lists (sets sorted); Path -> posix."""
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x) and abs(x) < 1e16:
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(norm(v) for v in x)
    if isinstance(x, Path):
        return x.as_posix()
    return x


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


# Exception identity: scripted "TimeoutExpired" must BE the class the live
# except clause catches (subprocess.TimeoutExpired), not a fresh namesake.
SubprocessTimeoutExpired = type("TimeoutExpired", (Exception,), {})
EXC_REGISTRY = {"TimeoutExpired": SubprocessTimeoutExpired}


class Raise:
    def __init__(self, spec):
        ty = spec.get("type", "Exception")
        msg = spec.get("message", "")
        self.spec = {"__raise__": ty, "message": msg}
        cls = EXC_REGISTRY.get(ty) or getattr(builtins, ty, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
            cls = type(ty, (Exception,), {})
        self.exc = cls(msg)


class Script:
    def __init__(self, io_map):
        self.io = io_map or {}
        self.cursors = {}

    def next(self, name):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            raise RuntimeError(f"no script for {name}")
        if isinstance(entry, list):
            i = self.cursors.get(name, 0)
            if i >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = i + 1
            entry = entry[i]
        if isinstance(entry, dict) and entry.get("__script__"):
            if "raise" in entry:
                return Raise(entry["raise"])
            return entry.get("ret")
        return entry


# ── AST extraction ──────────────────────────────────────────────────────────

def compile_node(node, filename):
    mod = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, filename, "exec", flags=FUT, dont_inherit=True)


def load_ns(relpath, seeds, want_funcs=(), want_classes=(), strict_consts=False):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_t4_" + re.sub(r"\W", "_", relpath)
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seeds)
    ns["__name__"] = modname  # dataclass resolves cls.__module__ via sys.modules
    consts = [n for n in tree.body
              if isinstance(n, (ast.Assign, ast.AnnAssign))
              and n.value is not None
              and all(isinstance(t, ast.Name) for t in
                      (n.targets if isinstance(n, ast.Assign) else [n.target]))]

    def seed_consts(strict):
        for n in consts:
            try:
                exec(compile_node(n, relpath), ns)
            except Exception:
                if strict:
                    raise

    seed_consts(False)
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name in want_classes:
            exec(compile_node(n, relpath), ns)
    for n in ast.walk(tree):
        if isinstance(n, ast.FunctionDef) and n.name in want_funcs:
            # Strip decorators (e.g. @property): drivers call the plain def
            # with an explicit self double.
            n2 = copy.copy(n)
            n2.decorator_list = []
            exec(compile_node(n2, relpath), ns)
    seed_consts(strict_consts)
    for name in list(want_funcs) + list(want_classes):
        assert name in ns, f"seed failure: {relpath}:{name}"
    return ns


def base_seeds(modname):
    logging_double = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            warning=lambda *a, **k: None, debug=lambda *a, **k: None,
            info=lambda *a, **k: None, error=lambda *a, **k: None))
    return {
        "__name__": modname,
        "Any": object, "Callable": object, "Optional": object, "Dict": object,
        "Literal": object, "Mapping": object, "Protocol": object, "Tuple": object,
        "Awaitable": object, "Iterable": object, "Set": object,
        "dataclass": __import__("dataclasses").dataclass,
        "field": __import__("dataclasses").field,
        "contextlib": __import__("contextlib"),
        "functools": __import__("functools"),
        "threading": types.SimpleNamespace(Lock=NoopLock),
        "weakref": types.SimpleNamespace(WeakKeyDictionary=dict),
        "logging": logging_double,
        "time": types.SimpleNamespace(monotonic=lambda: 0.0, time=lambda: 0.0),
        "asyncio": types.SimpleNamespace(Event=object, Lock=NoopLock,
                                         CancelledError=type("CancelledError", (Exception,), {})),
        "math": math, "re": re, "json": json, "hashlib": hashlib, "hmac": hmac,
        "os": types.SimpleNamespace(environ={}),
        "socket": types.SimpleNamespace(AF_UNIX=1),
        "secrets": types.SimpleNamespace(token_urlsafe=lambda n: "tok"),
        "sys": sys,
        "urlparse": urlparse,
        "ContextVar": type("ContextVar", (), {
            "__init__": lambda self, name, default=None: setattr(self, "default", default),
            "get": lambda self: self.default,
        }),
    }


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


# ── module namespaces (built once) ──────────────────────────────────────────

NS_CRA = load_ns(
    "agent/codex_responses_adapter.py", base_seeds("quarry_t4_cra"),
    want_funcs=("_canonical_call_id_from_fc", "_split_responses_tool_id"))

NS_CX = load_ns(
    "agent/transports/codex.py", base_seeds("quarry_t4_cx"),
    want_funcs=("_is_post_tool_replay", "_is_opencode_responses_backend",
                "_is_azure_foundry_responses", "_native_compaction_active"))

_urr_seeds = base_seeds("quarry_t4_urr")
_urr_seeds["subprocess"] = types.SimpleNamespace(TimeoutExpired=SubprocessTimeoutExpired)
NS_URR = load_ns(
    "hermes_cli/update_restart_recovery.py", _urr_seeds,
    want_funcs=("_serve_unit_replaced", "_unit_is_active", "_systemd_verified_active",
                "_unit_main_pid", "_run_quiet", "_succeeded", "_stdout",
                "_systemd_unit_candidates"))

NS_GGUF = load_ns(
    "hermes_cli/local_runtime/gguf.py", base_seeds("quarry_t4_gguf"),
    want_classes=("GGUFHeader",))

NS_TSV = load_ns(
    "tools/tool_search_validation.py", base_seeds("quarry_t4_tsv"),
    want_funcs=("_schema_has_external_ref",))

# _DEFAULT_DEFERRED_TOOLS derives from DEFAULT_CONFIG; pull the live literal
# straight out of the config_defaults AST (no import side effects).
_cd_src = (QUARRY / "hermes_cli/config_defaults.py").read_text(encoding="utf-8")
_cd_tree = ast.parse(_cd_src)
_defer_list = None
for _n in _cd_tree.body:
    if isinstance(_n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "DEFAULT_CONFIG" for t in _n.targets):
        for _k1, _v1 in zip(_n.value.keys, _n.value.values):
            if isinstance(_k1, ast.Constant) and _k1.value == "tools":
                for _k2, _v2 in zip(_v1.keys, _v1.values):
                    if isinstance(_k2, ast.Constant) and _k2.value == "tool_search":
                        for _k3, _v3 in zip(_v2.keys, _v2.values):
                            if isinstance(_k3, ast.Constant) and _k3.value == "defer":
                                _defer_list = ast.literal_eval(_v3)
assert _defer_list is not None, "DEFAULT_CONFIG defer list not found"
_ts_seeds = base_seeds("quarry_t4_ts")
_ts_seeds["_DEFAULT_DEFERRED_TOOLS"] = frozenset(_defer_list)
NS_TS = load_ns("tools/tool_search.py", _ts_seeds,
                want_funcs=("effective_defer_tools",))

NS_SC = load_ns(
    "gateway/stream_consumer.py", base_seeds("quarry_t4_sc"),
    want_funcs=("_stream_is_message",))

NS_SCF = load_ns(
    "gateway/stream_consumer_fallback.py", base_seeds("quarry_t4_scf"),
    want_funcs=("_is_flood_error",))

NS_RF = load_ns(
    "gateway/runtime_footer.py", base_seeds("quarry_t4_rf"),
    want_funcs=("_env_cwd",))

NS_RIQ = load_ns(
    "agent/review_idle_queue.py", base_seeds("quarry_t4_riq"),
    want_funcs=("defer_max_age_s", "_quiet_for"))

NS_PC = load_ns(
    "agent/prompt_caching.py", base_seeds("quarry_t4_pc"),
    want_funcs=("envelope_tool_part_cache_markers_supported", "is_qwen_model"))

NS_TTC = load_ns(
    "tools/terminal_tool_config.py", base_seeds("quarry_t4_ttc"),
    want_funcs=("_is_container_backend",))

NS_TTS = load_ns(
    "tools/terminal_tool_sudo.py", base_seeds("quarry_t4_tts"),
    want_funcs=("_in_delegated_child_context",))

NS_LP = load_ns(
    "hermes_cli/local_runtime/load_progress.py", base_seeds("quarry_t4_lp"),
    want_funcs=("_composite_percent", "_pct"))

NS_NAK = load_ns(
    "hermes_cli/nous_auth_keepalive.py", base_seeds("quarry_t4_nak"),
    want_funcs=("_observed_lifetime_seconds",))

NS_CH = load_ns(
    "agent/codex_headers.py", base_seeds("quarry_t4_ch"),
    want_funcs=("is_official_codex_base_url",))

NS_LSP = load_ns(
    "agent/lsp/client.py", base_seeds("quarry_t4_lsp"),
    want_funcs=("_connection_is_open",))

NS_TDH = load_ns(
    "agent/tool_dispatch_helpers.py", base_seeds("quarry_t4_tdh"),
    want_funcs=("_detect_upstream_elision",))

NS_CG = load_ns(
    "tools/clarify_gateway.py", base_seeds("quarry_t4_cg"),
    want_funcs=("_selection_attempt_tokens", "_split_tokens", "_is_int"))

NS_SAA = load_ns(
    "tools/skills_ast_audit.py", base_seeds("quarry_t4_saa"),
    want_funcs=("_is_importlib",))

NS_TEX = load_ns(
    "agent/tool_executor.py", base_seeds("quarry_t4_tex"),
    want_funcs=("_human_wait_seconds",))

NS_VT = load_ns(
    "tools/vision_tools.py", base_seeds("quarry_t4_vt"),
    want_funcs=("_profile_rejects_tool_media",))

NS_CM = load_ns(
    "agent/checkpoints/manager.py", base_seeds("quarry_t4_cm"),
    want_funcs=("_exceeds_size_cap",))

NS_UAR = load_ns(
    "hermes_cli/update_abort_recovery.py", base_seeds("quarry_t4_uar"),
    want_funcs=("_serve_unit_recovery_available",))

# Real adapter siblings for _is_post_tool_replay's in-function import.
_cra_mod = types.ModuleType("agent.codex_responses_adapter")
_cra_mod._canonical_call_id_from_fc = NS_CRA["_canonical_call_id_from_fc"]
_cra_mod._split_responses_tool_id = NS_CRA["_split_responses_tool_id"]
sys.modules["agent.codex_responses_adapter"] = _cra_mod
pkg("agent").codex_responses_adapter = _cra_mod

# ── recording seam factory ──────────────────────────────────────────────────

def make_rec(script, trace, armed):
    def rec(name, argb, make=None):
        def call(*a, **k):
            r = script.next(name)
            ev = {"ev": name, "args": argb(a, k), "ret": None}
            if isinstance(r, Raise):
                ev["ret"] = r.spec
                if armed[0]:
                    trace.append(ev)
                raise r.exc
            ev["ret"] = norm(r)
            if armed[0]:
                trace.append(ev)
            return make(r) if make else r
        return call
    return rec


def seam_import(modname, attr, import_seam, rec, call_seam, call_argb):
    """Model `from <modname> import <attr>`: the import itself is a seam
    (scripted ImportError fails it); the attribute is the call seam."""
    mod = types.ModuleType(modname)

    def mod_getattr(name):
        if name != attr:
            raise AttributeError(name)
        rec(import_seam, lambda a, k: {})()
        return rec(call_seam, call_argb)

    mod.__getattr__ = mod_getattr
    sys.modules[modname] = mod
    parent, _, child = modname.rpartition(".")
    if parent:
        setattr(pkg(parent), child, mod)
    return mod


def minus_io(work):
    work.pop("io", None)
    return work


def ns_obj(r):
    """Scripted dict -> attribute carrier; anything else passes through."""
    return types.SimpleNamespace(**r) if isinstance(r, dict) else r


# ── per-function drivers ────────────────────────────────────────────────────

def d_is_post_tool_replay(work, trace, script):
    return NS_CX["_is_post_tool_replay"](work.get("messages"))


def d_is_opencode_responses_backend(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("hermes_cli.models", "opencode_provider_family",
                "import_opencode_provider_family", rec,
                "opencode_provider_family", lambda a, k: {"provider": a[0]})
    seam_import("utils", "base_url_hostname",
                "import_base_url_hostname", rec,
                "base_url_hostname", lambda a, k: {"url": a[0]})
    return NS_CX["_is_opencode_responses_backend"](work.get("params"))


def d_is_azure_foundry_responses(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("utils", "base_url_host_matches",
                "import_base_url_host_matches", rec,
                "base_url_host_matches", lambda a, k: {"url": a[0], "host": a[1]})
    return NS_CX["_is_azure_foundry_responses"](work.get("params"))


def d_native_compaction_active(work, trace, script):
    return NS_CX["_native_compaction_active"](work.get("context_management"))


def _run_seam(rec):
    return rec("run", lambda a, k: {"argv": a[0], **k}, make=ns_obj)


def d_unit_is_active(work, trace, script):
    rec = make_rec(script, trace, [True])
    return NS_URR["_unit_is_active"](
        work["scope"], work.get("unit"), run=_run_seam(rec),
        require_rc0=work.get("require_rc0", False))


def d_unit_main_pid(work, trace, script):
    rec = make_rec(script, trace, [True])
    return NS_URR["_unit_main_pid"](work["scope"], work.get("unit"), run=_run_seam(rec))


def d_serve_unit_replaced(work, trace, script):
    rec = make_rec(script, trace, [True])
    return NS_URR["_serve_unit_replaced"](
        work["scope"], work.get("unit"), work["previous_pid"],
        run=_run_seam(rec), sleep=rec("sleep", lambda a, k: {"seconds": a[0]}))


def d_systemd_verified_active(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_URR["shutil"] = types.SimpleNamespace(
        which=rec("shutil.which", lambda a, k: {"name": a[0]}))
    return NS_URR["_systemd_verified_active"](work.get("profile"), run=_run_seam(rec))


def _gguf(work):
    return NS_GGUF["GGUFHeader"](path="x", version=3, metadata=work.get("metadata", {}))


def d_n_vocab(work, trace, script):
    return _gguf(work).n_vocab


def d_n_head(work, trace, script):
    return _gguf(work).n_head


def d_head_dim_k(work, trace, script):
    return _gguf(work).head_dim_k


def d_head_dim_v(work, trace, script):
    return _gguf(work).head_dim_v


def d_effective_defer_tools(work, trace, script):
    dbl = types.SimpleNamespace(defer_tools=work.get("defer_tools"))
    return NS_TS["effective_defer_tools"](dbl)


def d_schema_has_external_ref(work, trace, script):
    return NS_TSV["_schema_has_external_ref"](work.get("node"))


def d_stream_is_message(work, trace, script):
    rec = make_rec(script, trace, [True])
    a = work.get("adapter")
    probe_kind = a.get("probe", "absent") if isinstance(a, dict) else "absent"
    if isinstance(a, dict):
        cls_attrs = {}
        if probe_kind == "callable":
            cls_attrs["stream_is_message_for_chat"] = rec(
                "probe", lambda a_, k: {"chat_id": a_[1]})
        elif probe_kind == "noncallable":
            cls_attrs["stream_is_message_for_chat"] = 5
        adapter = type("Adapter", (), cls_attrs)()
        if "draft_stream_is_message" in a:
            adapter.draft_stream_is_message = a["draft_stream_is_message"]
    else:
        adapter = a
    dbl = types.SimpleNamespace(adapter=adapter, chat_id=work.get("chat_id"))
    return NS_SC["_stream_is_message"](dbl)


def d_is_flood_error(work, trace, script):
    result = ns_obj(work.get("result"))
    return NS_SCF["_is_flood_error"](types.SimpleNamespace(), result)


def d_env_cwd(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("tools.terminal_scope", "terminal_env",
                "import_terminal_env", rec,
                "terminal_env", lambda a, k: {"name": a[0], "default": a[1]})
    NS_RF["os"] = types.SimpleNamespace(environ=types.SimpleNamespace(
        get=rec("environ_get",
                lambda a, k: {"key": a[0], "default": a[1] if len(a) > 1 else ""})))
    return NS_RF["_env_cwd"]()


def d_defer_max_age_s(work, trace, script):
    return NS_RIQ["defer_max_age_s"](work.get("task_cfg"))


def d_quiet_for(work, trace, script):
    rec = make_rec(script, trace, [True])
    dbl = types.SimpleNamespace(quiet_period=rec(
        "quiet_period", lambda a, k: {},
        make=lambda r: tuple(r) if isinstance(r, list) else r))
    return NS_RIQ["_quiet_for"](dbl)


def d_envelope_markers(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("agent.agent_runtime_helpers", "_is_litellm_route",
                "import_is_litellm_route", rec,
                "is_litellm_route",
                lambda a, k: {"provider_lower": a[0], "base_url": a[1]})
    return NS_PC["envelope_tool_part_cache_markers_supported"](
        work.get("provider"), work.get("base_url"))


def d_is_qwen_model(work, trace, script):
    return NS_PC["is_qwen_model"](work.get("model"))


def d_is_container_backend(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_TTC["_plugin_env_flag"] = rec(
        "plugin_env_flag", lambda a, k: {"env_type": a[0], "attr": a[1]})
    return NS_TTC["_is_container_backend"](work.get("env_type"))


def d_in_delegated_child_context(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("agent.delegation_context", "is_delegated_child_context",
                "import_is_delegated_child_context", rec,
                "is_delegated_child_context", lambda a, k: {})
    return NS_TTS["_in_delegated_child_context"]()


def d_composite_percent(work, trace, script):
    return NS_LP["_composite_percent"](
        work.get("stages"), work.get("current"), work.get("value"))


def d_observed_lifetime_seconds(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_NAK["get_provider_auth_state"] = rec(
        "get_provider_auth_state", lambda a, k: {"provider": a[0]})
    return NS_NAK["_observed_lifetime_seconds"]()


def d_is_official_codex_base_url(work, trace, script):
    return NS_CH["is_official_codex_base_url"](work.get("base_url"))


def d_connection_is_open(work, trace, script):
    rec = make_rec(script, trace, [True])
    s = work.get("self")
    if isinstance(s, dict):
        proc = None
        p = s.get("_proc")
        if isinstance(p, dict):
            stdin = None
            if isinstance(p.get("stdin"), dict):
                stdin = types.SimpleNamespace(
                    is_closing=rec("stdin_is_closing", lambda a, k: {}))
            elif p.get("stdin") is not None:
                stdin = p["stdin"]
            proc = types.SimpleNamespace(returncode=p.get("returncode"), stdin=stdin)
        elif p is not None:
            proc = p
        reader = None
        rt = s.get("_reader_task")
        if isinstance(rt, dict):
            reader = types.SimpleNamespace(done=rec("reader_done", lambda a, k: {}))
        elif rt is not None:
            reader = rt
        dbl = types.SimpleNamespace(_state=s.get("_state"), _proc=proc, _reader_task=reader)
    else:
        dbl = s
    return NS_LSP["_connection_is_open"](dbl)


def d_detect_upstream_elision(work, trace, script):
    return NS_TDH["_detect_upstream_elision"](work.get("content"))


def d_selection_attempt_tokens(work, trace, script):
    return NS_CG["_selection_attempt_tokens"](work.get("text"), work.get("choices"))


def d_is_importlib(work, trace, script):
    return NS_SAA["_is_importlib"](work.get("name"))


def d_human_wait_seconds(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("tools.approval_human_wait", "human_wait_seconds",
                "import_human_wait_seconds", rec,
                "human_wait_seconds", lambda a, k: {"session_key": a[0]})
    s = work.get("self")
    dbl = types.SimpleNamespace(**s) if isinstance(s, dict) else s
    return NS_TEX["_human_wait_seconds"](dbl)


def d_profile_rejects_tool_media(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("providers", "routed_model_rejects_vision_tool_messages",
                "import_routed_model_rejects_vision_tool_messages", rec,
                "routed_model_rejects_vision_tool_messages",
                lambda a, k: {"provider": a[0], "model": a[1]})
    return NS_VT["_profile_rejects_tool_media"](work.get("provider"), work.get("model", ""))


def d_exceeds_size_cap(work, trace, script):
    rec = make_rec(script, trace, [True])

    class PathDouble:
        def __init__(self, raw):
            self.raw = raw

        def stat(self):
            r = rec("stat", lambda a, k: {"path": self.raw})()
            if isinstance(r, dict):
                return types.SimpleNamespace(st_size=r.get("st_size"))
            return r

    path_raw = work.get("path")
    path = PathDouble(path_raw) if isinstance(path_raw, str) else path_raw
    dbl = types.SimpleNamespace(max_file_size_mb=work.get("max_file_size_mb"))
    return NS_CM["_exceeds_size_cap"](dbl, path)


def d_serve_unit_recovery_available(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_UAR["sys"] = types.SimpleNamespace(platform=rec("sys_platform", lambda a, k: {})())
    NS_UAR["shutil"] = types.SimpleNamespace(
        which=rec("shutil.which", lambda a, k: {"name": a[0]}))
    return NS_UAR["_serve_unit_recovery_available"]()


DRIVERS = {
    "is_post_tool_replay": d_is_post_tool_replay,
    "is_opencode_responses_backend": d_is_opencode_responses_backend,
    "is_azure_foundry_responses": d_is_azure_foundry_responses,
    "native_compaction_active": d_native_compaction_active,
    "unit_is_active": d_unit_is_active,
    "unit_main_pid": d_unit_main_pid,
    "serve_unit_replaced": d_serve_unit_replaced,
    "systemd_verified_active": d_systemd_verified_active,
    "n_vocab": d_n_vocab,
    "n_head": d_n_head,
    "head_dim_k": d_head_dim_k,
    "head_dim_v": d_head_dim_v,
    "effective_defer_tools": d_effective_defer_tools,
    "schema_has_external_ref": d_schema_has_external_ref,
    "stream_is_message": d_stream_is_message,
    "is_flood_error": d_is_flood_error,
    "_env_cwd": d_env_cwd,
    "defer_max_age_s": d_defer_max_age_s,
    "_quiet_for": d_quiet_for,
    "envelope_tool_part_cache_markers_supported": d_envelope_markers,
    "is_qwen_model": d_is_qwen_model,
    "is_container_backend": d_is_container_backend,
    "in_delegated_child_context": d_in_delegated_child_context,
    "composite_percent": d_composite_percent,
    "observed_lifetime_seconds": d_observed_lifetime_seconds,
    "is_official_codex_base_url": d_is_official_codex_base_url,
    "connection_is_open": d_connection_is_open,
    "detect_upstream_elision": d_detect_upstream_elision,
    "selection_attempt_tokens": d_selection_attempt_tokens,
    "_is_importlib": d_is_importlib,
    "_human_wait_seconds": d_human_wait_seconds,
    "profile_rejects_tool_media": d_profile_rejects_tool_media,
    "exceeds_size_cap": d_exceeds_size_cap,
    "serve_unit_recovery_available": d_serve_unit_recovery_available,
}


def run_case(driver, inp):
    trace = []
    work = copy.deepcopy(inp)
    script = Script(work.get("io"))
    try:
        ret = driver(work, trace, script)
        minus_io(work)
        return {"ret": norm(ret), "trace": norm(trace), "state": norm(work)}
    except BaseException as e:  # SystemExit included — fail closed per case
        spec = {"__raise__": type(e).__name__, "message": str(e)}
        if isinstance(e, SystemExit):
            spec["code"] = e.code
        minus_io(work)
        return {"ret": spec, "trace": norm(trace), "state": norm(work)}


def main():
    n = 0
    mismatches = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as e:
            mismatches += 1
            print(f"BAD JSON from harness: {e}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        driver = DRIVERS.get(func)
        if driver is None:
            mismatches += 1
            print(f"UNKNOWN func {func!r}")
            continue
        if not isinstance(got, dict) or "error" in got:
            mismatches += 1
            print(f"HARNESS ERROR {func}: {got}")
            continue
        try:
            want = run_case(driver, inp)
        except BaseException as e:
            mismatches += 1
            print(f"ORACLE DRIVER FAILURE {func}: {type(e).__name__}: {e}")
            continue
        if got != want:
            mismatches += 1
            if shown < 8:
                shown += 1
                print(f"MISMATCH {func} case#{n}")
                for key in ("ret", "trace", "state"):
                    if got.get(key) != want.get(key):
                        print(f"  {key} C  : {json.dumps(got.get(key), ensure_ascii=False)[:600]}")
                        print(f"  {key} PY : {json.dumps(want.get(key), ensure_ascii=False)[:600]}")
    print(f"ORACLE tail_revive4: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()