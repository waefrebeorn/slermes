#!/usr/bin/env python3
"""oracle_ws_revive4.py — differential oracle for the web-server group-4 revival.

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the
LIVE quarry defs, seeds module constants (Assign + AnnAssign, plain-Name
targets only), drives each live function under the SAME scripted seam
results the C harness consumed, and compares ordered trace + ret +
post-state. BaseException is caught per case (SystemExit included);
missing/exhausted scripts fail closed on both sides.

Seam conventions (identical on both sides, see ws_revive4.c header):
- One universal seam per live I/O call; args objects drop None-valued
  top-level keys on BOTH sides; the trace records {ev, args, ret} in call
  order with the RAW scripted ret.
- In-function imports are pre-stubbed in sys.modules (imports always
  succeed); the imported callable is the seam.
- Attribute carriers (entry/policy/m/record/provider) are SimpleNamespace
  doubles; missing-attribute errors read 'types.SimpleNamespace' (py3.14).
- Path seams return Path(scripted string) on the PY side; norm() projects
  Path -> as_posix so it matches the C string projection.
- Integral JSON numbers model Python ints; str()-projection cases use int
  literals only; truthy container str() inputs are out of fixture scope.
- HTTPException is a double with starlette's __str__ ("404: <detail>").
- os.environ is an EnvironDouble replicating os._Environ's non-str-key
  TypeError ("str expected, not <type>").

Gate (explicit objects — libjson only):
  bash tests/run_one_oracle.sh ws_revive4 oracle_ws_revive4.py "" "lib/libjson/json.o"
"""
import ast
import asyncio
import builtins
import copy
import __future__
import json
import math
import os
import re
import sys
import types
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home_ws4"))

FUT = __future__.annotations.compiler_flag
MISSING = object()


def norm(x):
    """Integral floats -> ints (libjson emits %.0f); tuples -> lists; Path -> posix."""
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x) and abs(x) < 1e16:
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, Path):
        return x.as_posix()
    return x


def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


class HTTPException(Exception):
    """starlette.exceptions.HTTPException shape (str probed on the quarry).
    Named HTTPException so type(e).__name__ matches the C raise spec."""
    def __init__(self, status_code, detail=None, headers=None):
        self.status_code = status_code
        self.detail = detail
        super().__init__(detail)

    def __str__(self):
        return f"{self.status_code}: {self.detail}"


class EnvironDouble(dict):
    """os._Environ rejects non-str keys even in .get (probed 3.14)."""
    def get(self, k, d=None):
        if not isinstance(k, str):
            raise TypeError(f"str expected, not {type(k).__name__}")
        return dict.get(self, k, d)


EXC_REGISTRY = {
    "CancelledError": asyncio.CancelledError,
    "HTTPException": HTTPException,
}


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
        self.raised = []  # Raise instances handed out this case

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
                r = Raise(entry["raise"])
                self.raised.append(r)
                return r
            return entry.get("ret")
        return entry


def make_stub(trace, script, name, argmap, make=None):
    """Recording seam stub: identical event shape to the C dispatcher.
    argmap is a fixed args dict or a callable building it from the call.
    make transforms the scripted result for the LIVE return (e.g. Path);
    the trace always records the RAW scripted value."""
    def f(*a, **k):
        args = drop_none(dict(argmap(*a, **k)) if callable(argmap) else dict(argmap))
        try:
            entry = script.next(name)
        except RuntimeError as e:
            spec = {"__raise__": "RuntimeError", "message": str(e)}
            trace.append({"ev": name, "args": args, "ret": spec})
            raise
        if isinstance(entry, Raise):
            trace.append({"ev": name, "args": args, "ret": entry.spec})
            raise entry.exc
        trace.append({"ev": name, "args": args, "ret": entry})
        return make(entry) if make else entry
    return f


# ── AST extraction ──────────────────────────────────────────────────────────

def compile_node(node, filename):
    mod = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, filename, "exec", flags=FUT, dont_inherit=True)


def base_seeds(modname):
    logging_double = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            warning=lambda *a, **k: None, debug=lambda *a, **k: None,
            info=lambda *a, **k: None, error=lambda *a, **k: None))
    return {
        "__name__": modname,
        "Any": object, "Callable": object, "Optional": object, "Dict": object,
        "Literal": object, "Mapping": object, "Protocol": object, "Tuple": object,
        "Awaitable": object, "Iterable": object, "List": object,
        "dataclass": __import__("dataclasses").dataclass,
        "field": __import__("dataclasses").field,
        "contextlib": __import__("contextlib"),
        "logging": logging_double,
        "math": math, "re": re, "json": json,
        "os": types.SimpleNamespace(environ={}),
        "sys": sys,
        "Path": Path,
        "HTTPException": HTTPException,
    }


def load_ns(relpath, seeds, want_funcs=(), want_consts=()):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_ws4_" + re.sub(r"\W", "_", relpath)
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seeds)
    ns["__name__"] = modname
    # py3.14 dataclass/annotation resolution: register every alias too.
    alias = seeds.get("__name__", modname)
    sys.modules.setdefault(alias, mod)
    consts = [n for n in tree.body
              if isinstance(n, (ast.Assign, ast.AnnAssign))
              and n.value is not None
              and all(isinstance(t, ast.Name) for t in
                      (n.targets if isinstance(n, ast.Assign) else [n.target]))]

    def seed_consts():
        for n in consts:
            try:
                exec(compile_node(n, relpath), ns)
            except Exception:
                pass

    seed_consts()
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in want_funcs:
            n2 = copy.copy(n)
            n2.decorator_list = []
            exec(compile_node(n2, relpath), ns)
    seed_consts()  # re-seed after defs (constants may reference helpers)
    for name in want_funcs:
        assert name in ns, f"seed failure: {relpath}:{name}"
    for name in want_consts:
        assert name in ns, f"const seed failure: {relpath}:{name}"
    return ns


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def bind_func(ns, relpath, name):
    """Extract a live helper def from another module into ns."""
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == name:
            exec(compile_node(n, relpath), ns)
            assert name in ns
            return
    raise RuntimeError(f"def not found: {relpath}:{name}")


# ── module namespaces (built once) ──────────────────────────────────────────

NS_MEM = load_ns(
    "hermes_cli/web_server_memory.py", base_seeds("quarry_ws4_mem"),
    want_funcs=("_normalize_memory_provider_name", "_env_lookup",
                "_field_default", "_coerce_bool", "_load_memory_provider",
                "_memory_provider_manifest"))

_mp_seeds = base_seeds("quarry_ws4_mp")
NS_MP = load_ns(
    "hermes_cli/web_routers/memory_providers.py", _mp_seeds,
    want_funcs=("_trim_setup_output", "_require_valid_memory_provider_name",
                "_unknown_provider", "_write_provider_flat", "_flat_json_path"),
    want_consts=("_MEMORY_PROVIDER_NAME_RE",))

NS_CE = load_ns(
    "hermes_cli/web_routers/config_env.py", base_seeds("quarry_ws4_ce"),
    want_funcs=("_custom_endpoint_id", "_config_api_key_is_env_ref"))
# coerce_provider_id is imported from hermes_cli.config (facade); the live
# def lives in config_providers.py — extract it REAL (no seam).
bind_func(NS_CE, "hermes_cli/config_providers.py", "coerce_provider_id")

NS_OPS = load_ns(
    "hermes_cli/web_routers/ops.py", base_seeds("quarry_ws4_ops"),
    want_funcs=("_pool_entry_summary",))

NS_GW = load_ns(
    "hermes_cli/web_server_gateway.py", base_seeds("quarry_ws4_gw"),
    want_funcs=("_strip_session_list_rows",),
    want_consts=("_SESSION_LIST_HEAVY_FIELDS",))

NS_DASH = load_ns(
    "hermes_cli/web_server_dashboard.py", base_seeds("quarry_ws4_dash"),
    want_funcs=("_strip_dashboard_manifest",))

NS_FILES = load_ns(
    "hermes_cli/web_server_files.py", base_seeds("quarry_ws4_files"),
    want_funcs=("_managed_response_meta",))

NS_SK = load_ns(
    "hermes_cli/web_routers/skills.py", base_seeds("quarry_ws4_sk"),
    want_funcs=("_skill_meta_to_payload", "_clear_skills_prompt_cache"))

NS_PROF = load_ns(
    "hermes_cli/web_server_profiles.py", base_seeds("quarry_ws4_prof"),
    want_funcs=("_aux_usage_rows",))

NS_MSG = load_ns(
    "hermes_cli/web_server_messaging.py", base_seeds("quarry_ws4_msg"),
    want_funcs=("_write_platform_enabled", "_whatsapp_session_path",
                "_whatsapp_onboarding_payload", "_telegram_onboarding_base_url"),
    want_consts=("_WHATSAPP_PAYLOAD_FIELDS", "_TELEGRAM_ONBOARDING_DEFAULT_URL"))

NS_PR = load_ns(
    "hermes_cli/web_routers/profiles.py", base_seeds("quarry_ws4_pr"),
    want_funcs=("_profile_setup_command",))

NS_OA = load_ns(
    "hermes_cli/web_routers/oauth.py", base_seeds("quarry_ws4_oa"),
    want_funcs=("_validate_oauth_profile",))
# _oauth_profile_name is imported from web_server_oauth.py — run it REAL.
bind_func(NS_OA, "hermes_cli/web_server_oauth.py", "_oauth_profile_name")

# ── real-def restore table ──────────────────────────────────────────────────
# A ported function can ALSO be a seamed sibling of another flow
# (_flat_json_path is seamed by _write_provider_flat). A prior case's
# recorder would otherwise leak into the target's own later cases, so
# every drive() restores ALL extracted defs to real before installing
# the case's stubs.
REAL = [
    (NS_MEM, {k: NS_MEM[k] for k in (
        "_normalize_memory_provider_name", "_env_lookup", "_field_default",
        "_coerce_bool", "_load_memory_provider", "_memory_provider_manifest")}),
    (NS_MP, {k: NS_MP[k] for k in (
        "_trim_setup_output", "_require_valid_memory_provider_name",
        "_unknown_provider", "_write_provider_flat", "_flat_json_path")}),
    (NS_CE, {k: NS_CE[k] for k in (
        "_custom_endpoint_id", "_config_api_key_is_env_ref", "coerce_provider_id")}),
    (NS_OPS, {k: NS_OPS[k] for k in ("_pool_entry_summary",)}),
    (NS_GW, {k: NS_GW[k] for k in ("_strip_session_list_rows",)}),
    (NS_DASH, {k: NS_DASH[k] for k in ("_strip_dashboard_manifest",)}),
    (NS_FILES, {k: NS_FILES[k] for k in ("_managed_response_meta",)}),
    (NS_SK, {k: NS_SK[k] for k in (
        "_skill_meta_to_payload", "_clear_skills_prompt_cache")}),
    (NS_PROF, {k: NS_PROF[k] for k in ("_aux_usage_rows",)}),
    (NS_MSG, {k: NS_MSG[k] for k in (
        "_write_platform_enabled", "_whatsapp_session_path",
        "_whatsapp_onboarding_payload", "_telegram_onboarding_base_url")}),
    (NS_PR, {k: NS_PR[k] for k in ("_profile_setup_command",)}),
    (NS_OA, {k: NS_OA[k] for k in (
        "_validate_oauth_profile", "_oauth_profile_name")}),
]

# ── sys.modules doubles for in-function imports ─────────────────────────────

pkg("agent")
MOD_AGENT_MEM = pkg("agent.memory_provider")
sys.modules["agent"].memory_provider = MOD_AGENT_MEM
MOD_PROMPT = pkg("agent.prompt_builder")
sys.modules["agent"].prompt_builder = MOD_PROMPT
pkg("hermes_cli")
MOD_CONFIG = pkg("hermes_cli.config")
sys.modules["hermes_cli"].config = MOD_CONFIG
MOD_MEM_SETUP = pkg("hermes_cli.memory_setup")
sys.modules["hermes_cli"].memory_setup = MOD_MEM_SETUP
pkg("plugins")
MOD_PLUGINS_MEM = pkg("plugins.memory")
sys.modules["plugins"].memory = MOD_PLUGINS_MEM
MOD_HCONST = pkg("hermes_constants")


def path_make(r):
    return Path(r) if isinstance(r, str) else r


# ── per-case drive ──────────────────────────────────────────────────────────

def drive(func, work, trace, script):
    inp = work
    for ns, real in REAL:
        ns.update(real)

    if func == "ws_u_normalize_memory_provider_name":
        MOD_AGENT_MEM.is_core_memory_provider = make_stub(
            trace, script, "is_core_memory_provider",
            lambda provider: {"provider": provider})
        return NS_MEM["_normalize_memory_provider_name"](inp.get("name"))

    if func == "ws_u_env_lookup":
        MOD_CONFIG.load_env = make_stub(trace, script, "load_env", {})
        NS_MEM["os"] = types.SimpleNamespace(
            environ=EnvironDouble(inp.get("env") or {}))
        return NS_MEM["_env_lookup"](inp.get("env_key"))

    if func == "ws_u_field_default":
        return NS_MEM["_field_default"](inp.get("field"))

    if func == "ws_u_load_memory_provider":
        MOD_PLUGINS_MEM.load_memory_provider = make_stub(
            trace, script, "load_memory_provider",
            lambda name, register_skills=None: {
                "name": name, "register_skills": register_skills})
        return NS_MEM["_load_memory_provider"](
            inp.get("name"), register_skills=inp.get("register_skills"))

    if func == "ws_u_memory_provider_manifest":
        MOD_MEM_SETUP.memory_provider_dependency_inputs = make_stub(
            trace, script, "memory_provider_dependency_inputs",
            lambda name: {"name": name})
        return NS_MEM["_memory_provider_manifest"](inp.get("name"))

    if func == "ws_u_trim_setup_output":
        return NS_MP["_trim_setup_output"](inp.get("value"), inp.get("limit", 4000))

    if func == "ws_u_require_valid_memory_provider_name":
        return NS_MP["_require_valid_memory_provider_name"](inp.get("name"))

    if func == "ws_u_write_provider_flat":
        provider_d = types.SimpleNamespace(name=inp.get("provider"))
        NS_MP["_read_flat_json"] = make_stub(
            trace, script, "read_flat_json",
            lambda provider: {"provider": provider.name})
        NS_MP["_save_submitted_secrets"] = make_stub(
            trace, script, "save_submitted_secrets",
            lambda provider, values: {"provider": provider.name, "values": values})
        NS_MP["_apply_field_values"] = make_stub(
            trace, script, "apply_field_values",
            lambda provider, values, existing: {
                "provider": provider.name, "values": values, "existing": existing})
        NS_MP["_flat_json_path"] = make_stub(
            trace, script, "flat_json_path",
            lambda provider: {"provider": provider.name})
        NS_MP["_write_json_0600"] = make_stub(
            trace, script, "write_json_0600",
            lambda path, data: {"path": path, "data": data})
        return NS_MP["_write_provider_flat"](provider_d, inp.get("values"))

    if func == "ws_u_flat_json_path":
        provider_d = types.SimpleNamespace(**(inp.get("provider") or {}))
        NS_MP["get_hermes_home"] = make_stub(
            trace, script, "get_hermes_home", {}, make=path_make)
        return NS_MP["_flat_json_path"](provider_d)

    if func == "ws_u_custom_endpoint_id":
        return NS_CE["_custom_endpoint_id"](inp.get("raw"), inp.get("fallback", "custom"))

    if func == "ws_u_config_api_key_is_env_ref":
        NS_CE["_raw_provider_api_key"] = make_stub(
            trace, script, "_raw_provider_api_key",
            lambda endpoint_id: {"endpoint_id": endpoint_id})
        return NS_CE["_config_api_key_is_env_ref"](inp.get("endpoint_id"))

    if func == "ws_u_pool_entry_summary":
        entry_d = types.SimpleNamespace(**(inp.get("entry") or {}))
        NS_OPS["redact_key"] = make_stub(
            trace, script, "redact_key", lambda token: {"token": token})
        return NS_OPS["_pool_entry_summary"](entry_d, inp.get("index"))

    if func == "ws_u_strip_session_list_rows":
        # live mutates the rows in place — drive on the WORK copy so the
        # post-state comparison sees the pops.
        return NS_GW["_strip_session_list_rows"](work.get("sessions"))

    if func == "ws_u_strip_dashboard_manifest":
        return NS_DASH["_strip_dashboard_manifest"](inp.get("p"))

    if func == "ws_u_managed_response_meta":
        policy_d = types.SimpleNamespace(**(inp.get("policy") or {}))
        return NS_FILES["_managed_response_meta"](policy_d)

    if func == "ws_u_skill_meta_to_payload":
        m_d = types.SimpleNamespace(**(inp.get("m") or {}))
        return NS_SK["_skill_meta_to_payload"](m_d)

    if func == "ws_u_clear_skills_prompt_cache":
        MOD_PROMPT.clear_skills_system_prompt_cache = make_stub(
            trace, script, "clear_skills_system_prompt_cache",
            lambda clear_snapshot: {"clear_snapshot": clear_snapshot})
        return NS_SK["_clear_skills_prompt_cache"]()

    if func == "ws_u_aux_usage_rows":
        analytics_d = types.SimpleNamespace(
            auxiliary_usage_by_model_task=make_stub(
                trace, script, "auxiliary_usage_by_model_task",
                lambda started_after: {"started_after": started_after}))
        persistence_d = types.SimpleNamespace(usage_analytics=analytics_d)
        return NS_PROF["_aux_usage_rows"](persistence_d, inp.get("cutoff"))

    if func == "ws_u_write_platform_enabled":
        NS_MSG["write_platform_config_field"] = make_stub(
            trace, script, "write_platform_config_field",
            lambda platform_id, field, value: {
                "platform_id": platform_id, "field": field, "value": value})
        return NS_MSG["_write_platform_enabled"](
            inp.get("platform_id"), inp.get("enabled"))

    if func == "ws_u_whatsapp_session_path":
        MOD_HCONST.get_hermes_dir = make_stub(
            trace, script, "get_hermes_dir",
            lambda new_subpath, old_name: {
                "new_subpath": new_subpath, "old_name": old_name},
            make=path_make)
        return NS_MSG["_whatsapp_session_path"]()

    if func == "ws_u_whatsapp_onboarding_payload":
        record_d = types.SimpleNamespace(**(inp.get("record") or {}))
        return NS_MSG["_whatsapp_onboarding_payload"](
            inp.get("pairing_id"), record_d)

    if func == "ws_u_telegram_onboarding_base_url":
        env = inp.get("env") or {}
        NS_MSG["os"] = types.SimpleNamespace(
            getenv=lambda k, d=None: env.get(k, d), environ=env)
        return NS_MSG["_telegram_onboarding_base_url"]()

    if func == "ws_u_profile_setup_command":
        NS_PR["_resolve_profile_dir"] = make_stub(
            trace, script, "_resolve_profile_dir", lambda name: {"name": name})
        return NS_PR["_profile_setup_command"](inp.get("name"))

    if func == "ws_u_validate_oauth_profile":
        NS_OA["_resolve_profile_dir"] = make_stub(
            trace, script, "_resolve_profile_dir", lambda name: {"name": name})
        return NS_OA["_validate_oauth_profile"](inp.get("profile"))

    raise RuntimeError(f"unknown func {func}")


def run_case(func, inp):
    trace = []
    work = copy.deepcopy(inp)
    script = Script(work.get("io"))
    try:
        ret = drive(func, work, trace, script)
    except BaseException as e:  # SystemExit included — fail closed per case
        # Scripted raises round-trip their spec BARE (the C port parks the
        # script verbatim); reconstructing str(e) would mangle classes whose
        # constructor takes more than a message (e.g. HTTPException).
        ret = None
        for r in script.raised:
            if e is r.exc:
                ret = r.spec
                break
        if ret is None:
            ret = {"__raise__": type(e).__name__, "message": str(e)}
    work.pop("io", None)
    return {"ret": norm(ret), "trace": norm(trace), "state": norm(work)}


def main():
    n = 0
    mismatches = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as e:
            print(f"FATAL: harness line not JSON: {e}")
            sys.exit(2)
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out") or {}
        if "error" in got:
            mismatches += 1
            print(f"MISMATCH [{n}] {func}: harness error {got['error']!r}")
            continue
        want = run_case(func, inp)
        bad = False
        for key in ("ret", "trace", "state"):
            if got.get(key) != want[key]:
                if not bad:
                    print(f"MISMATCH [{n}] {func}")
                    print(f"  in: {json.dumps(inp, ensure_ascii=False)}")
                bad = True
                print(f"  {key} want: {json.dumps(want[key], ensure_ascii=False)}")
                print(f"  {key} got:  {json.dumps(got.get(key), ensure_ascii=False)}")
        if bad:
            mismatches += 1
    print(f"ORACLE ws_revive4: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
