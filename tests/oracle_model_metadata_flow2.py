#!/usr/bin/env python3
"""Oracle for the second UNCLAIMED-remainder FLOW port of agent/model_metadata.py
(src/agent/model_metadata_flow2.c).

Drives the LIVE model_metadata.py defs with SCRIPTED seams: every collaborator
(claimed siblings, foreign-module imports, HTTP client/file/clock doubles,
logger) is a recording stub fed the same scripts the C harness feeds mm2_call.
Compares the DECISION SEQUENCE (ordered "trace" of {ev,args,ret} events), the
canonical return ("ret") and the post-call state ("state"). Fail-closed:
0 cases => exit 1; a per-case exception rides as {"__raise__": cls}.

Representation contract (both sides, see src/agent/model_metadata_flow2.c):
- None -> null; integral floats collapse to ints; Path -> POSIX str.
- uncaught exception -> {"__raise__": cls}; an exception used as a seam /
  logger arg projects to {"__raise__": cls, "msg": str(e)}; logger exception
  args project to str(e).
- scripts: absent key -> None; {"$seq": [...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m} raises
  (trace ret is the WHOLE marker); any other value is reused. None-valued arg
  keys are dropped on both sides ("" is kept).
- except Exception never catches the BaseException trio; suppress(Exception)
  and except ImportError follow the real builtin hierarchy.
"""
import __future__
import ast
import builtins
import contextlib
import json
import logging
import os
import sys
import types
from pathlib import Path, PurePath, PurePosixPath
from typing import Any, Optional

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "model_metadata.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "model_metadata.py")

NAME = "model_metadata_flow2"
PORTED = [
    "_ollama_show",
    "_lmstudio_context",
    "_openai_models_list_context",
    "_load_context_cache_document",
    "_config_override_context_length",
    "_lmstudio_native_models",
]
WANTED = set(PORTED)
logging.disable(logging.CRITICAL)

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
_missing = WANTED - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── canonical projection ────────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, PurePath):
        return v.as_posix()
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams ──────────────────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        if name not in self.io:
            return None
        script = self.io[name]
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


class _InertCM:
    def __enter__(self):
        return self
    def __exit__(self, *exc):
        return False


class RespDouble:
    """Scripted HTTP response: {"status_code": int, "json": obj}."""
    def __init__(self, d):
        self.d = d if isinstance(d, dict) else {}

    @property
    def status_code(self):
        return self.d.get("status_code")

    def json(self):
        return self.d.get("json")

    def raise_for_status(self):
        return record("raise_for_status", {})


class GetClient:
    """client.get(url) -> scripted response (seam 'get')."""
    def get(self, url):
        return record("get", {"url": url})


class HttpxClientDouble:
    """httpx.Client(...) -> seam 'httpx_client'; .post -> seam 'post'."""
    def __enter__(self):
        return self
    def __exit__(self, *exc):
        return False
    def post(self, url, json=None):
        return record("post", {"url": url, "json": json})


class HttpxModule:
    @staticmethod
    def Client(*a, **kw):
        return _httpx_client(*a, **kw)


def _httpx_client(*a, **kw):
    # Record the seam, but the scripted value is an opaque handle only: the
    # client double is what the live `with ... as client` body drives.
    record("httpx_client", {
        "timeout": kw.get("timeout"), "headers": kw.get("headers"),
        "verify": kw.get("verify"),
    })
    return HttpxClientDouble()




class PathDouble:
    def __init__(self, s):
        self.s = s
    def __str__(self):
        return self.s
    def as_posix(self):
        return self.s
    def exists(self):
        return record("exists", {"path": self.s})


# These seams materialize their script into an object double.
_MATERIALIZE = {
    "get": lambda s: RespDouble(s),
    "http_get": lambda s: RespDouble(s),
    "post": lambda s: RespDouble(s),
    "get_context_cache_path": lambda s: PathDouble(s) if isinstance(s, str) else s,
}


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = norm(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    mat = _MATERIALIZE.get(name)
    return mat(script) if mat else script


class SeamFn:
    def __init__(self, name):
        self.name = name
    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": [norm(x) if not isinstance(x, BaseException) else str(x) for x in args]}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record("logger." + level, d)
    def debug(self, msg, *a, **kw):
        return self._log("debug", msg, a, kw)
    def info(self, msg, *a, **kw):
        return self._log("info", msg, a, kw)
    def warning(self, msg, *a, **kw):
        return self._log("warning", msg, a, kw)


_LOGGER = LoggerDouble()

# ── seam arg projections (MUST match the C mm2_arg key sets) ────────────────
SEAM_ARGS = {
    "auth_headers": lambda a, k: {"api_key": a[0] if a else k.get("api_key")},
    "resolve_verify": lambda a, k: {"base_url": a[0]},
    "httpx_client": lambda a, k: {"timeout": k.get("timeout"), "headers": k.get("headers"), "verify": k.get("verify")},
    "post": lambda a, k: {"url": a[0], "json": k.get("json")},
    "note_if_connect_timeout": lambda a, k: {"exc": a[0], "base_url": a[1]},
    "get": lambda a, k: {"url": a[0]},
    "http_get": lambda a, k: {
        "url": a[0], "headers": k.get("headers"), "timeout": k.get("timeout"), "verify": k.get("verify")},
    "model_id_matches": lambda a, k: {"candidate_id": a[0], "lookup_model": a[1]},
    "positive_int": lambda a, k: {"value": a[0]},
    "context_length_from_model_payload": lambda a, k: {"payload": a[0]},
    "get_context_cache_path": lambda a, k: {},
    "exists": lambda a, k: {"path": str(a[0])},
    "open": lambda a, k: {"path": str(a[0]), "encoding": k.get("encoding")},
    "yaml.safe_load": lambda a, k: {},
    "override_context_window": lambda a, k: {"provider": a[0], "model": a[1]},
    "get_custom_provider_context_length": lambda a, k: {
        "model": k.get("model"), "base_url": k.get("base_url"), "custom_providers": k.get("custom_providers")},
    "lmstudio_server_root": lambda a, k: {"base_url": a[0]},
    "raise_for_status": lambda a, k: {},
    "lmstudio_loaded_context": lambda a, k: {"model": a[0]},
    "endpoint_model_entry": lambda a, k: {"model": a[0], "model_id": a[1], "context_length": a[2]},
    "add_model_aliases": lambda a, k: {"cache": a[0], "model_id": a[1], "entry": a[2]},
}
for _nm in SEAM_ARGS:
    assert _nm == "httpx_client" or _nm in SEAM_ARGS  # (kept explicit for clarity)


# ── stub modules + base namespace ──────────────────────────────────────────
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# `_ollama_show` does `import httpx` INSIDE its body -> resolves sys.modules,
# not the exec namespace. Stub it so no real network I/O ever happens.
_mod("httpx", Client=_httpx_client)


agent_pkg = _mod("agent")
agent_pkg.__path__ = []
hc_pkg = _mod("hermes_cli")
hc_pkg.__path__ = []
_mod("agent.models_dev", _override_context_window=SeamFn("override_context_window"))
_mod("hermes_cli.config", get_custom_provider_context_length=SeamFn("get_custom_provider_context_length"))
_mod("hermes_cli")
_mod("agent.command_token_source", materialize_probe_api_key=lambda k: k)


def _open_seam(*a, **kw):
    # The scripted value is the file handle's opaque stand-in; ALWAYS return an
    # inert context manager so `with open(...) as f:` never trips on a str.
    record("open", {"path": str(a[0]) if a else kw.get("path"), "encoding": kw.get("encoding")})
    return _InertCM()


_BASE_NS = {
    "__name__": "mm2_live_model_metadata",
    "__builtins__": builtins,
    "Any": Any, "Optional": Optional,
    "Path": Path, "PurePosixPath": PurePosixPath,
    "contextlib": contextlib,
    "open": _open_seam,
    "logger": _LOGGER,
    "httpx": HttpxModule,
    "yaml": types.SimpleNamespace(safe_load=SeamFn("yaml.safe_load")),
    "model_metadata_http": types.SimpleNamespace(
        get=SeamFn("http_get"), resolve_verify=SeamFn("resolve_verify")),
    "_auth_headers": SeamFn("auth_headers"),
    "_note_if_connect_timeout": SeamFn("note_if_connect_timeout"),
    "_lmstudio_server_root": SeamFn("lmstudio_server_root"),
    "_model_id_matches": SeamFn("model_id_matches"),
    "_positive_int": SeamFn("positive_int"),
    "_context_length_from_model_payload": SeamFn("context_length_from_model_payload"),
    "_get_context_cache_path": SeamFn("get_context_cache_path"),
    "_lmstudio_loaded_context": SeamFn("lmstudio_loaded_context"),
    "_endpoint_model_entry": SeamFn("endpoint_model_entry"),
}
_MOD = types.ModuleType("mm2_live_model_metadata")
_MOD.__dict__.update(_BASE_NS)
sys.modules["mm2_live_model_metadata"] = _MOD
_BASE_NS = _MOD.__dict__


def _add_model_aliases_rec(cache, model_id, entry):
    record("add_model_aliases", {"cache": cache, "model_id": model_id, "entry": entry})
    cache[model_id] = entry
    if "/" in model_id:
        cache.setdefault(model_id.split("/", 1)[1], entry)


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    ns = dict(_BASE_NS)
    ns["logger"] = _LOGGER
    for _nm, _code in DEF_CODE.items():
        exec(_code, ns)
    # in-function imports rebind these names from stub modules -> same SeamFns.
    ns["_add_model_aliases"] = _add_model_aliases_rec
    return ns, trace


# ── live driver ─────────────────────────────────────────────────────────────
def drive(rec, ns):
    func = rec["func"]
    inp = rec["in"]
    if func == "_ollama_show":
        return ns[func](
            inp.get("server_url", ""), inp.get("api_key"), inp.get("bare_model", ""),
            timeout=inp.get("timeout", 3.0), note_blackhole=inp.get("note_blackhole", False))
    if func == "_lmstudio_context":
        return ns[func](GetClient(), inp.get("lmstudio_url", ""), inp.get("model", ""))
    if func == "_openai_models_list_context":
        return ns[func](GetClient(), inp.get("server_url", ""), inp.get("model", ""))
    if func == "_load_context_cache_document":
        return ns[func]()
    if func == "_config_override_context_length":
        return ns[func](inp.get("model", ""), inp.get("base_url", ""),
                        inp.get("provider", ""), inp.get("custom_providers"))
    if func == "_lmstudio_native_models":
        return ns[func](inp.get("normalized", ""), inp.get("headers"))
    raise KeyError(f"unknown func {func}")


def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        try:
            ns, trace = build_ns(rec)
            try:
                ret = drive(rec, ns)
                out = {"ret": norm(ret), "trace": trace, "state": {}}
            except BaseException as e:
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace, "state": {}}
        except BaseException as e:
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test ───────────────────────────────────────────────────────────────
def selftest(fixtures):
    bad = 0
    for i, rec in enumerate(fixtures):
        try:
            ns, trace = build_ns(rec)
            try:
                drive(rec, ns)
            except BaseException as e:
                msg = str(e)
                # Only flag a binding bug when the missing attribute is a PORTED
                # function or a real object-double method we are supposed to have
                # bound (not a plain str/dict method like "get", whose absence on a
                # scripted non-dict is an INTENTIONAL exception path).
                bind_names = set(PORTED) | {"exists", "raise_for_status", "post", "load"}
                if isinstance(e, AttributeError) and any(
                        f"'{nm}'" in msg for nm in bind_names):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: method-named AttributeError: {msg}")
        except BaseException as e:
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "model_metadata_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").split("\n") if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()
