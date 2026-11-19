#!/usr/bin/env python3
"""Oracle for the tools/vision_tools.py vision-analysis FLOW + leaf port
(src/tools/vision_analysis_flow.c).

Drives the LIVE vision-analysis orchestration defs with SCRIPTED I/O: every
foreign sub-call (config, registry, providers, image_source, image_prep,
history budget, url_safety, website_policy, interrupt, PIL/pm, asyncio, logger,
tool_error, json.dumps, debug session) and every CLAIMED sibling is a recording
stub fed the same scripts the C harness feeds its seams. Unclaimed siblings
that are themselves ports here are seamed when composed and driven REAL by
their own direct cases; _run_analysis runs REAL as the backbone of the
vision_analyze_tool wrapper. Compares the DECISION SEQUENCE (ordered seam calls
{"name","args"} in "trace"), the return ("ret") and the echoed globals ("state").

Live defs are AST-extracted (module constants + the _ImagePrepError /
_PreparedImage classes seeded); the heavy quarry import chain is never imported.
In-function imports are satisfied with sys.modules stubs delegating to the
current case's recorder. Fail-closed: missing defs or 0 cases => exit 2; a
missing/exhausted seam script raises RuntimeError on both sides (recorded on the
trace before propagation). Catch BaseException (scripted SystemExit /
KeyboardInterrupt / GeneratorExit / CancelledError propagate through `except
Exception` sites). Async targets are driven with asyncio.run + async recorders.

Representation contract (shared with the C port header):
- paths are PathDouble (str-backed); proj -> str; methods fire path.* seams.
- resolved image -> SimpleNamespace(data=bytes, mime); data fixtures ASCII str.
- _PreparedImage -> {path,mime,size_bytes,crop_offset} (real namedtuple live).
- normalize/crop seam results -> 3-tuples; resize_prepared -> data_url str +
  scale_out mutation; aux_probe_mode -> CM double; os.fdopen -> handle double.
- Integral floats normalize to ints; fixtures ASCII, no non-finite floats.
Summary: 'ORACLE vision_analysis_flow: N cases, M mismatches'.
"""
import ast
import asyncio
import copy
import json
import math
import os
import re
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "tools" / "vision_tools.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "tools" / "vision_tools.py")

# live name -> C seam-driven target (the "func" key in fixtures is the live name)
PY = {
    "_media_messages": "_media_messages",
    "_debug_call_data": "_debug_call_data",
    "_too_large_message": "_too_large_message",
    "_unsupported_video_format": "_unsupported_video_format",
    "_build_native_vision_tool_result": "_build_native_vision_tool_result",
    "_cfg_auxiliary": "_cfg_auxiliary",
    "_read_vision_setting": "_read_vision_setting",
    "_managed_install": "_managed_install",
    "_secure_cache_dir": "_secure_cache_dir",
    "_write_private_bytes": "_write_private_bytes",
    "_import_pillow_for_resize": "_import_pillow_for_resize",
    "_accepts_tool_result_images": "_accepts_tool_result_images",
    "_native_tool_result_images": "_native_tool_result_images",
    "_should_use_native_vision_fast_path": "_should_use_native_vision_fast_path",
    "_unlink_quietly": "_unlink_quietly",
    "_aux_call_kwargs": "_aux_call_kwargs",
    "_configured_aux_model": "_configured_aux_model",
    "_load_auxiliary_client": "_load_auxiliary_client",
    "check_video_requirements": "check_video_requirements",
    "_validate_image_url_async": "_validate_image_url_async",
    "_ssrf_redirect_guard": "_ssrf_redirect_guard",
    "_call_vision_llm": "_call_vision_llm",
    "_prepare_image": "_prepare_image",
    "_vision_analyze_native": "_vision_analyze_native",
    "_run_analysis": "_run_analysis",
    "vision_analyze_tool": "vision_analyze_tool",
    "_handle_vision_analyze": "_handle_vision_analyze",
}
ASYNC_TARGETS = {
    "_validate_image_url_async", "_ssrf_redirect_guard", "_call_vision_llm",
    "_prepare_image", "_vision_analyze_native", "_run_analysis",
    "vision_analyze_tool", "_handle_vision_analyze",
}
REQUIRED_CONSTS = [
    "_MAX_BASE64_BYTES", "_RESIZE_TARGET_BYTES", "_EMBED_MAX_DIMENSION",
    "_VIDEO_MIME_TYPES", "_SIZE_ERROR_HINTS", "_BILLING_HINTS",
    "_IMAGE_ERROR_RULES", "_VIDEO_ERROR_RULES", "_ANALYSIS_KINDS",
    "_VISION_MAX_VALIDATED_FRAME_COUNT", "_VISION_MAX_VALIDATED_AGGREGATE_PIXELS",
]

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(seg(src, node), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


class ImageResolutionError(Exception):
    pass


class ResolveContext:
    def __init__(self, task_id=None):
        self.task_id = task_id


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "tools.vision_tools",
        "re": re, "os": os, "sys": sys, "json": json, "math": math,
        "asyncio": asyncio, "types": types, "copy": copy,
        "base64": __import__("base64"), "uuid": __import__("uuid"),
        "logging": __import__("logging"),
        "Path": Path, "Any": object, "Awaitable": object, "Callable": object,
        "Dict": dict, "NamedTuple": __import__("typing").NamedTuple,
        "Optional": __import__("typing").Optional,
        "urlparse": __import__("urllib.parse", fromlist=["urlparse"]).urlparse,
        "ThreadPoolExecutor": __import__("concurrent.futures", fromlist=["ThreadPoolExecutor"]).ThreadPoolExecutor,
        "BytesIO": __import__("io").BytesIO,
        "logger": _SilentLogger(),
        # imported collaborators (seamed per case; placeholders for constant exec)
        "get_hermes_dir": lambda *a, **k: None,
        "make_user_dir": lambda *a, **k: None,
        "check_website_access": lambda *a, **k: None,
        "DebugSession": lambda *a, **k: types.SimpleNamespace(
            log_call=lambda *a, **k: None, save=lambda *a, **k: None),
        "async_call_llm": None,
        "extract_content_or_reasoning": None,
        "tool_error": lambda *a, **k: "",
        "_native_turn_duplicate": lambda *a, **k: None,
        "_record_embed": lambda *a, **k: None,
        "_release_embed": lambda *a, **k: None,
        "_repeat_refusal": lambda *a, **k: None,
        "_resolve_repeat_cap": lambda *a, **k: 0,
        "_resolve_embed_target_bytes": lambda *a, **k: 0,
        "_crop_image_region": lambda *a, **k: None,
        "_determine_mime_type": lambda *a, **k: "image/png",
        "_image_exceeds_dimension": lambda *a, **k: False,
        "_normalize_to_supported_image": lambda *a, **k: None,
        "_validate_raster_image_decodable": lambda *a, **k: None,
        "_VISION_MAX_VALIDATED_AGGREGATE_PIXELS": 100000000,
        "_VISION_MAX_VALIDATED_FRAME_COUNT": 100,
    }
    tree = ast.parse(src)

    def consts():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue
                try:
                    exec_node(src, node, ns, SRC)
                except Exception:
                    pass

    # classes first (_ImagePrepError / _PreparedImage needed by defs + consts)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("_ImagePrepError", "_PreparedImage"):
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    consts()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    consts()  # re-seed constants that reference functions
    missing = [t for t in PY.values() if t not in ns]
    for c in REQUIRED_CONSTS:
        if c not in ns:
            missing.append(c)
    for c in ("_ImagePrepError", "_PreparedImage"):
        if c not in ns:
            missing.append(c)
    if missing:
        print(f"FATAL oracle_vision_analysis_flow: missing defs/consts: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in set(PY.values()) if nm in NS}
CURRENT = {"ctx": None}


# ── exception materialization (REAL builtin trio; module-local classes) ─────
EXC_CLASSES = {
    "OSError": OSError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "Exception": Exception, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "UnicodeDecodeError": UnicodeDecodeError,
    "TimeoutError": TimeoutError, "NotImplementedError": NotImplementedError,
    "ImageResolutionError": ImageResolutionError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if kind == "SystemExit":
        return SystemExit(msg)
    if kind == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if kind == "GeneratorExit":
        return GeneratorExit(msg)
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    if kind == "_ImagePrepError":
        return NS["_ImagePrepError"](msg)
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── path / object doubles ───────────────────────────────────────────────────
class PathDouble:
    def __init__(self, s):
        self.s = str(s)

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __repr__(self):
        return f"PathDouble({self.s!r})"

    def __truediv__(self, other):
        base = self.s.rstrip("/")
        return PathDouble(base + "/" + str(other))

    def __eq__(self, other):
        if isinstance(other, PathDouble):
            return self.s == other.s
        if isinstance(other, str):
            return self.s == other
        return NotImplemented

    def __hash__(self):
        return hash(self.s)

    @property
    def name(self):
        return self.s.rsplit("/", 1)[-1]

    @property
    def suffix(self):
        n = self.name
        return ("." + n.rsplit(".", 1)[-1]) if "." in n else ""

    def mkdir(self, parents=False, exist_ok=False):
        return CURRENT["ctx"].call("path.mkdir",
                                   {"path": self.s, "parents": parents, "exist_ok": exist_ok})

    def unlink(self, missing_ok=False):
        return CURRENT["ctx"].call("path.unlink", {"path": self.s, "missing_ok": missing_ok})

    def exists(self):
        return CURRENT["ctx"].call("path.exists", {"path": self.s})

    def stat(self):
        r = CURRENT["ctx"].call("path.stat", {"path": self.s})
        return types.SimpleNamespace(st_size=(r or {}).get("st_size", 0))

    def write_bytes(self, data):
        return CURRENT["ctx"].call("path.write_bytes", {"path": self.s, "data": proj(data)})


class HandleDouble:
    def __enter__(self):
        return self

    def __exit__(self, et, ev, tb):
        exc = {"type": et.__name__, "message": str(ev)} if et is not None else None
        r = CURRENT["ctx"].call("os.fdopen.__exit__", {"exc": exc})
        return bool(r)

    def write(self, data):
        return CURRENT["ctx"].call("handle.write", {"data": proj(data)})


class ProbeCM:
    def __enter__(self):
        return CURRENT["ctx"].call("aux_probe_mode.__enter__", {})

    def __exit__(self, et, ev, tb):
        exc = {"type": et.__name__, "message": str(ev)} if et is not None else None
        r = CURRENT["ctx"].call("aux_probe_mode.__exit__", {"exc": exc})
        return bool(r)


class AuxModule(types.ModuleType):
    """agent.auxiliary_client double: explicit attrs for call-bound imports;
    __getattr__ fires aux.<name> seams for the _load_auxiliary_client reads."""
    def __getattr__(self, name):
        if name in ("async_call_llm", "extract_content_or_reasoning"):
            return CURRENT["ctx"].call("aux." + name, {})
        raise AttributeError(name)


# ── per-case script backend (fail-closed; record then raise) ────────────────
class Ctx:
    def __init__(self, inp):
        self.inp = inp
        self.io = inp.get("io") or {}
        self.cursors = {}
        self.trace = []

    def next_entry(self, name):
        if name not in self.io:
            raise RuntimeError(f"no script for {name}")
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def call(self, name, args):
        self.trace.append({"name": name, "args": args})
        entry = self.next_entry(name)
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry


# ── projection (live object -> JSON-comparable) ─────────────────────────────
def proj(v):
    if isinstance(v, PathDouble):
        return v.s
    if isinstance(v, bytes):
        return v.decode("utf-8", errors="replace")
    if isinstance(v, BaseException):
        return proj_exc(v)
    if isinstance(v, NS["_PreparedImage"]):
        return {"path": proj(v.path), "mime": v.mime,
                "size_bytes": v.size_bytes, "crop_offset": proj(v.crop_offset)}
    if isinstance(v, types.SimpleNamespace):
        return {k: proj(x) for k, x in vars(v).items()}
    if isinstance(v, dict):
        return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [proj(x) for x in v]
    return v


def canon(v):
    if isinstance(v, float):
        if math.isnan(v) or math.isinf(v):
            return v
        if v == int(v) and abs(v) < 1e16:
            return int(v)
        return v
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    return v


def deep_eq(a, b):
    return canon(proj(a)) == canon(proj(b))


# ── seam recorders ──────────────────────────────────────────────────────────
def rec(name, argbuilder=None, mat=None):
    def f(*a, **k):
        args = argbuilder(*a, **k) if argbuilder else {}
        r = CURRENT["ctx"].call(name, args)
        return mat(r, a, k) if mat else r
    return f


def arec(name, argbuilder=None, mat=None):
    async def f(*a, **k):
        args = argbuilder(*a, **k) if argbuilder else {}
        r = CURRENT["ctx"].call(name, args)
        return mat(r, a, k) if mat else r
    return f


def mat_path(r, a, k):
    return PathDouble(r) if isinstance(r, str) else r


def mat_resolved(r, a, k):
    if isinstance(r, dict):
        data = r.get("data", "")
        if isinstance(data, str):
            data = data.encode("utf-8")
        return types.SimpleNamespace(data=data, mime=r.get("mime"))
    return r


def mat_prepared(r, a, k):
    if isinstance(r, dict):
        return NS["_PreparedImage"](
            PathDouble(r.get("path", "")), r.get("mime"),
            r.get("size_bytes", 0), dict(r.get("crop_offset") or {}))
    return r


def mat_norm(r, a, k):
    if isinstance(r, dict):
        np = r.get("normalized_path")
        return (PathDouble(np) if isinstance(np, str) else np,
                r.get("mime"), r.get("norm_err"))
    return r


def mat_crop(r, a, k):
    if isinstance(r, dict):
        # mutate the passed offset_out dict in place (live _crop_image_region)
        off = k.get("offset_out")
        if isinstance(off, dict) and isinstance(r.get("offset_out"), dict):
            off.clear()
            off.update(r["offset_out"])
        cp = r.get("cropped_path")
        return (PathDouble(cp) if isinstance(cp, str) else cp,
                r.get("mime"), r.get("crop_err"))
    return r


def mat_resize(r, a, k):
    if isinstance(r, dict):
        si = a[1] if len(a) > 1 else None
        if isinstance(si, dict) and isinstance(r.get("scale_out"), dict):
            si.update(r["scale_out"])
        return r.get("data_url")
    return r


def mat_probe(r, a, k):
    return ProbeCM()


def mat_fdopen(r, a, k):
    return HandleDouble()


def _fdopen_double(r):
    return HandleDouble()


def mat_pillow(r, a, k):
    return r  # Image marker


def _proj_prepared_arg(p):
    return proj(p)


def install_seams(ctx, skip):
    CURRENT["ctx"] = ctx
    ns = NS
    for nm, obj in REAL.items():
        ns[nm] = obj
    ns["logger"] = types.SimpleNamespace(
        debug=lambda fmt, *a, **k: ctx.call("logger.debug", {
            "fmt": fmt, "args": [str(x) if isinstance(x, BaseException) else x for x in a], **k}),
        info=_SilentLogger().info, warning=_SilentLogger().warning, error=_SilentLogger().error)
    ns["_debug"] = types.SimpleNamespace(
        log_call=lambda tool_name, data: ctx.call("debug.log_call",
                                                  {"tool_name": tool_name, "debug_call_data": proj(data)}),
        save=lambda: ctx.call("debug.save", {}))
    ns["uuid"] = types.SimpleNamespace(uuid4=lambda: ctx.call("uuid.uuid4", {}))
    ns["json"] = types.SimpleNamespace(
        dumps=lambda obj, indent=None, ensure_ascii=True: ctx.call(
            "json.dumps", {"obj": proj(obj), "indent": indent, "ensure_ascii": ensure_ascii}))
    ns["os"] = types.SimpleNamespace(
        getenv=lambda var, default="": ctx.call("os.getenv", {"var": var, "default": default}),
        open=lambda path, flags, mode=0o777: ctx.call("os.open", {"path": str(path), "flags": flags, "mode": mode}),
        fdopen=lambda fd, mode: _fdopen_double(ctx.call("os.fdopen", {"fd": fd, "mode": mode})),
        O_WRONLY=1, O_CREAT=64, O_TRUNC=512)
    ns["asyncio"] = types.SimpleNamespace(
        to_thread=_async_to_thread, sleep=_async_sleep,
        get_running_loop=lambda: types.SimpleNamespace(run_in_executor=None))
    ns["urlparse"] = lambda url: types.SimpleNamespace(
        netloc=ctx.call("urlparse_netloc", {"url": url}))
    # fn markers passed to the seamed CPU executor project to their live name
    for _nm in ("_validate_raster_image_decodable", "_image_exceeds_dimension", "_image_to_base64_data_url"):
        def _marker(*a, _n=_nm, **k):
            return None
        _marker._va_seam_name = _nm
        ns[_nm] = _marker
    ns["get_hermes_dir"] = rec("get_hermes_dir",
                               lambda sub, old: {"new_subpath": sub, "old_name": old}, mat_path)
    ns["make_user_dir"] = rec("make_user_dir",
                              lambda path, parents=False, exist_ok=False, mode=0o700:
                              {"path": str(path), "parents": parents, "exist_ok": exist_ok, "mode": mode})
    ns["tool_error"] = rec("tool_error", lambda message, **extra: {"message": str(message), **extra})
    ns["async_call_llm"] = arec("async_call_llm", lambda **kw: {"kwargs": proj(kw)})
    ns["extract_content_or_reasoning"] = rec("extract_content_or_reasoning",
                                             lambda response: {"response": proj(response)})
    # composed siblings (live name -> seam name)
    sib = {
        "_cfg_auxiliary": ("cfg_auxiliary", lambda *keys, default=None: {"keys": list(keys), "default": default}),
        "_managed_install": ("managed_install", lambda: {}),
        "_secure_cache_dir": ("secure_cache_dir", lambda sub, old: {"new_subpath": sub, "old_name": old}),
        "_write_private_bytes": ("write_private_bytes", lambda path, data: {"path": str(path), "data": proj(data)}),
        "_unlink_quietly": ("unlink_quietly", lambda path: {"path": proj(path)}),
        "_media_messages": ("media_messages", lambda p, pt, du: {"user_prompt": p, "part_type": pt, "data_url": du}),
        "_aux_call_kwargs": ("aux_call_kwargs", lambda messages, model, default_timeout, *, min_timeout=None:
                             {"messages": proj(messages), "model": model, "default_timeout": default_timeout, "min_timeout": min_timeout}),
        "_load_auxiliary_client": ("load_auxiliary_client", lambda: {}),
        "_debug_call_data": ("debug_call_data", lambda kind, source, user_prompt, model:
                             {"kind": kind, "source": source, "user_prompt": user_prompt, "model": model}),
        "_should_use_native_vision_fast_path": ("should_use_native_vision_fast_path", lambda: {}),
        "_configured_aux_model": ("configured_aux_model", lambda sections, env_vars:
                                  {"sections": list(sections), "env_vars": list(env_vars)}),
        "_accepts_tool_result_images": ("accepts_tool_result_images", lambda p, m, cfg:
                                        {"provider": p, "model": m, "cfg": proj(cfg)}),
        "_native_tool_result_images": ("native_tool_result_images", lambda p, m, cfg:
                                       {"provider": p, "model": m, "cfg": proj(cfg)}),
        "_profile_rejects_tool_media": ("profile_rejects_tool_media", lambda p, m="": {"provider": p, "model": m}),
        "_supports_media_in_tool_results": ("supports_media_in_tool_results", lambda p, m: {"provider": p, "model": m}),
        "_is_image_size_error": ("is_image_size_error", lambda error: {"error": proj_exc(error) if isinstance(error, BaseException) else error}),
        "_build_scale_note": ("build_scale_note", lambda scale_info, crop_offset:
                              {"scale_info": proj(scale_info), "crop_offset": proj(crop_offset)}),
        "_build_native_vision_tool_result": ("build_native_vision_tool_result",
                                             lambda image_url="", question="", image_data_url="", image_size_bytes=0, scale_note=None:
                                             {"image_url": image_url, "question": question, "image_data_url": image_data_url,
                                              "image_size_bytes": image_size_bytes, "scale_note": scale_note}),
        "_too_large_message": ("too_large_message", lambda image_data_url: {"image_data_url": image_data_url}),
        "_run_encode_on_cpu_executor": ("run_encode_on_cpu_executor", None),
        "_resize_prepared": ("resize_prepared", None),
        "_native_turn_duplicate": ("native_turn_duplicate", lambda image_url, region: {"image_url": image_url, "region": region}),
        "_repeat_refusal": ("repeat_refusal", lambda image_url: {"image_url": image_url}),
        "_resolve_repeat_cap": ("resolve_repeat_cap", lambda: {}),
        "_resolve_embed_target_bytes": ("resolve_embed_target_bytes", lambda: {}),
        "_record_embed": ("record_embed", lambda image_url: {"image_url": image_url}),
        "_release_embed": ("release_embed", lambda image_url: {"image_url": image_url}),
        "_normalize_to_supported_image": ("normalize_to_supported_image", lambda path, mime: {"path": str(path), "mime": mime}),
        "_crop_image_region": ("crop_image_region", lambda path, region, *, offset_out=None:
                               {"path": str(path), "region": region, "offset_out": proj(offset_out)}),
    }
    for live, (seam, ab) in sib.items():
        if live == skip:
            continue
        if live == "_run_encode_on_cpu_executor":
            ns[live] = arec(seam, _encode_args)
        elif live == "_resize_prepared":
            ns[live] = arec(seam, _resize_args, mat_resize)
        elif live == "_secure_cache_dir":
            ns[live] = rec(seam, ab, mat_path)
        elif live == "_normalize_to_supported_image":
            ns[live] = rec(seam, ab, mat_norm)
        elif live == "_crop_image_region":
            ns[live] = rec(seam, ab, mat_crop)
        elif live in ("_prepare_image",):
            ns[live] = arec(seam, _prepare_args, mat_prepared)
        else:
            ns[live] = rec(seam, ab)
    # async composed siblings not in sib
    if skip != "_prepare_image":
        ns["_prepare_image"] = arec("prepare_image", _prepare_args, mat_prepared)
    if skip != "_call_vision_llm":
        ns["_call_vision_llm"] = arec("call_vision_llm",
                                      lambda call_kwargs, empty_log, response=None:
                                      {"call_kwargs": proj(call_kwargs), "empty_log": empty_log, "response": proj(response)})
    if skip != "_vision_analyze_native":
        ns["_vision_analyze_native"] = arec("vision_analyze_native",
                                            lambda image_url, question, task_id=None, region=None:
                                            {"image_url": image_url, "question": question, "task_id": task_id, "region": region})
    if skip != "vision_analyze_tool":
        ns["vision_analyze_tool"] = arec("vision_analyze_tool",
                                         lambda image_url, user_prompt, model=None, task_id=None, region=None:
                                         {"image_url": image_url, "user_prompt": user_prompt, "model": model, "task_id": task_id, "region": region})
    # backbone: vision_analyze_tool drives _run_analysis REAL
    if skip == "vision_analyze_tool":
        ns["_run_analysis"] = REAL["_run_analysis"]
    _install_module_stubs(ctx, skip)


def _encode_args(fn, *a, **k):
    fname = getattr(fn, "_va_seam_name", None)
    if fname is None:
        fname = getattr(fn, "__name__", str(fn))
    return {"fn": fname, "args": [proj(x) for x in a], "kwargs": proj(k)}


def _resize_args(prepared, scale_info, **kw):
    return {"prepared": proj(prepared), "scale_info": proj(scale_info), "kwargs": proj(kw)}


def _prepare_args(image_url, task_id, region, *, validate_decode):
    return {"image_url": image_url, "task_id": task_id, "region": region, "validate_decode": validate_decode}


async def _async_to_thread(fn, *a, **k):
    return fn(*a, **k)


async def _async_sleep(*a, **k):
    return None


def _install_module_stubs(ctx, skip):
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    def pkg(name):
        m = sys.modules.get(name)
        if not isinstance(m, types.ModuleType):
            m = types.ModuleType(name)
            m.__path__ = []
            sys.modules[name] = m
        return m

    pkg("agent")
    pkg("hermes_cli")
    pkg("tools")
    pkg("PIL")
    mod("hermes_cli.config",
        cfg_get=rec("cfg_get", lambda config, *keys, default=None:
                    {"config": proj(config), "keys": list(keys), "default": default}),
        load_config=rec("load_config", lambda: {}),
        is_managed=rec("is_managed", lambda: {}),
        _secure_dir=rec("secure_dir", lambda path: {"path": str(path)}))
    mod("tools.interrupt", is_interrupted=rec("is_interrupted", lambda: {}))
    mod("tools.hosted_tool_policy", host_feature_enabled=rec("host_feature_enabled", lambda name: {"name": name}))
    mod("tools.url_safety",
        async_is_safe_url=arec("async_is_safe_url", lambda url: {"url": url}),
        redirect_target_from_response=rec("redirect_target_from_response", lambda response: {"response": proj(response)}))
    mod("tools.image_source",
        resolve_image_source=arec("resolve_image_source",
                                  lambda src, ctx, *, permitted=("image",):
                                  {"src": src, "task_id": getattr(ctx, "task_id", None)}, mat_resolved),
        ImageResolutionError=ImageResolutionError, ResolveContext=ResolveContext)
    mod("agent.image_routing",
        _lookup_supports_vision=rec("lookup_supports_vision", lambda p, m, cfg: {"provider": p, "model": m, "cfg": proj(cfg)}),
        decide_image_input_mode=rec("decide_image_input_mode", lambda p, m, cfg: {"provider": p, "model": m, "cfg": proj(cfg)}))
    aux = AuxModule("agent.auxiliary_client")
    aux._read_main_provider = rec("read_main_provider", lambda: {})
    aux._read_main_model = rec("read_main_model", lambda: {})
    aux.aux_probe_mode = rec("aux_probe_mode", lambda: {}, mat_probe)
    aux.resolve_vision_provider_client = rec("resolve_vision_provider_client", lambda **kw: {"kwargs": proj(kw)})
    sys.modules["agent.auxiliary_client"] = aux
    sys.modules["agent"].auxiliary_client = aux
    mod("pm", ensure_import=rec("ensure_import", lambda name: {"name": name}))
    # PIL.Image attribute read fires import_pillow seam
    pil = sys.modules["PIL"]

    class PILMod(types.ModuleType):
        def __getattr__(self, name):
            if name == "Image":
                return CURRENT["ctx"].call("import_pillow", {})
            raise AttributeError(name)
    pil2 = PILMod("PIL")
    sys.modules["PIL"] = pil2


# ── per-func live drive ─────────────────────────────────────────────────────
def _stage_recorder(ctx):
    async def stage(user_prompt, debug_call_data, temp_paths):
        r = ctx.call("stage", {"user_prompt": user_prompt,
                               "debug_call_data": proj(debug_call_data),
                               "temp_paths": proj(temp_paths)})
        for p in (ctx.inp.get("stage_paths") or []):
            temp_paths.append(PathDouble(p))
        if isinstance(r, list) and len(r) == 2:
            return (r[0], r[1])
        return r
    return stage


def run_case(func, inp):
    ctx = Ctx(inp)
    install_seams(ctx, PY[func])
    ns = NS
    # module-global state for _load_auxiliary_client (only that case reads/writes
    # the globals as state; other cases keep the install_seams recorders)
    st = inp.get("state") or {}
    if func == "_load_auxiliary_client":
        ns["async_call_llm"] = st.get("async_call_llm", None)
        ns["extract_content_or_reasoning"] = st.get("extract_content_or_reasoning", None)
    out = {}
    ret = None
    g = inp.get

    def drive_sync():
        if func == "_media_messages":
            return ns["_media_messages"](g("user_prompt", ""), g("part_type", ""), g("data_url", ""))
        if func == "_debug_call_data":
            return ns["_debug_call_data"](g("kind", ""), g("source"), g("user_prompt", ""), g("model"))
        if func == "_too_large_message":
            return ns["_too_large_message"](g("image_data_url", ""))
        if func == "_unsupported_video_format":
            return ns["_unsupported_video_format"](g("suffix", ""))
        if func == "_build_native_vision_tool_result":
            return ns["_build_native_vision_tool_result"](
                image_url=g("image_url", ""), question=g("question"),
                image_data_url=g("image_data_url", ""), image_size_bytes=g("image_size_bytes", 0),
                scale_note=g("scale_note"))
        if func == "_cfg_auxiliary":
            return ns["_cfg_auxiliary"](*(g("keys") or []), default=g("default"))
        if func == "_read_vision_setting":
            cast_name = g("cast", "int")
            cast_fn = rec("cast", lambda raw: {"raw": proj(raw), "cast": cast_name})
            return ns["_read_vision_setting"](g("env_var", ""), g("key", ""), cast_fn, minimum=g("minimum"))
        if func == "_managed_install":
            return ns["_managed_install"]()
        if func == "_secure_cache_dir":
            return ns["_secure_cache_dir"](g("new_subpath", ""), g("old_name", ""))
        if func == "_write_private_bytes":
            data = g("data", "")
            if isinstance(data, str):
                data = data.encode("utf-8")
            return ns["_write_private_bytes"](PathDouble(g("path", "")), data)
        if func == "_import_pillow_for_resize":
            return ns["_import_pillow_for_resize"]()
        if func == "_accepts_tool_result_images":
            return ns["_accepts_tool_result_images"](g("provider", ""), g("model", ""), g("cfg"))
        if func == "_native_tool_result_images":
            return ns["_native_tool_result_images"](g("provider", ""), g("model", ""), g("cfg"))
        if func == "_should_use_native_vision_fast_path":
            return ns["_should_use_native_vision_fast_path"]()
        if func == "_unlink_quietly":
            p = g("path")
            return ns["_unlink_quietly"](PathDouble(p) if isinstance(p, str) else p)
        if func == "_aux_call_kwargs":
            return ns["_aux_call_kwargs"](g("messages") or [], g("model"), g("default_timeout", 0),
                                          min_timeout=g("min_timeout"))
        if func == "_configured_aux_model":
            return ns["_configured_aux_model"](tuple(g("sections") or []), tuple(g("env_vars") or []))
        if func == "_load_auxiliary_client":
            return ns["_load_auxiliary_client"]()
        if func == "check_video_requirements":
            return ns["check_video_requirements"]()
        raise ValueError("unknown sync func " + func)

    async def drive_async():
        if func == "_validate_image_url_async":
            return await ns["_validate_image_url_async"](g("url"))
        if func == "_ssrf_redirect_guard":
            resp = g("response")
            if isinstance(resp, dict):
                resp = types.SimpleNamespace(**resp)
            return await ns["_ssrf_redirect_guard"](resp)
        if func == "_call_vision_llm":
            return await ns["_call_vision_llm"](g("call_kwargs") or {}, g("empty_log", ""), g("response"))
        if func == "_prepare_image":
            return await ns["_prepare_image"](g("image_url"), g("task_id"), g("region"),
                                              validate_decode=g("validate_decode", False))
        if func == "_vision_analyze_native":
            return await ns["_vision_analyze_native"](g("image_url"), g("question"),
                                                      task_id=g("task_id"), region=g("region"))
        if func == "_run_analysis":
            return await ns["_run_analysis"](g("kind", ""), g("source"), g("user_prompt"),
                                             g("model"), _stage_recorder(ctx))
        if func == "vision_analyze_tool":
            return await ns["vision_analyze_tool"](g("image_url"), g("user_prompt"), g("model"),
                                                   task_id=g("task_id"), region=g("region"))
        if func == "_handle_vision_analyze":
            return await ns["_handle_vision_analyze"](g("args") or {}, **(g("kw") or {}))
        raise ValueError("unknown async func " + func)

    try:
        if func in ASYNC_TARGETS:
            ret = asyncio.run(drive_async())
        else:
            ret = drive_sync()
        ret = proj(ret)
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity
        ret = proj_exc(exc)
    out["ret"] = ret
    out["trace"] = ctx.trace
    if func == "_load_auxiliary_client":
        out["state"] = {"async_call_llm": proj(ns["async_call_llm"]),
                        "extract_content_or_reasoning": proj(ns["extract_content_or_reasoning"])}
    else:
        out["state"] = copy.deepcopy(inp.get("state") or {})
    return out


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        try:
            recd = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = recd.get("func")
        inp = recd.get("in", {}) or {}
        c_out = recd.get("out")
        cases += 1
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            py_out = run_case(func, inp)
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print(f"PYERR {func}: {type(exc).__name__}: {exc}")
            continue
        if c_out is None:
            mism += 1
            print(f"MISMATCH {func}: missing C out")
            continue
        bad = []
        for key, want in py_out.items():
            got = c_out.get(key)
            if not deep_eq(got, want):
                bad.append(key)
        if bad:
            mism += 1
            print(f"MISMATCH {func} keys={bad}")
            for key in bad:
                print(f"  C.{key}={json.dumps(canon(proj(c_out.get(key))), default=str)[:600]}")
                print(f"  PY.{key}={json.dumps(canon(proj(py_out.get(key))), default=str)[:600]}")
    if cases == 0:
        print("FATAL oracle_vision_analysis_flow: 0 cases")
        print("ORACLE vision_analysis_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE vision_analysis_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


def seam_names():
    return ["load_config", "cfg_get", "is_managed", "secure_dir", "import_pillow",
            "ensure_import", "get_hermes_dir", "make_user_dir", "path.mkdir", "os.open",
            "os.fdopen", "handle.write", "os.fdopen.__exit__", "path.write_bytes",
            "path.unlink", "path.exists", "path.stat", "os.getenv", "cast", "cfg_auxiliary",
            "managed_install", "profile_rejects_tool_media", "supports_media_in_tool_results",
            "lookup_supports_vision", "decide_image_input_mode", "read_main_provider",
            "read_main_model", "aux.async_call_llm", "aux.extract_content_or_reasoning",
            "aux_probe_mode", "aux_probe_mode.__enter__", "aux_probe_mode.__exit__",
            "resolve_vision_provider_client", "async_is_safe_url", "redirect_target_from_response",
            "host_feature_enabled", "resolve_image_source", "secure_cache_dir",
            "write_private_bytes", "normalize_to_supported_image", "crop_image_region",
            "run_encode_on_cpu_executor", "image_exceeds_dimension", "image_to_base64_data_url",
            "resize_prepared", "native_turn_duplicate", "repeat_refusal", "resolve_repeat_cap",
            "resolve_embed_target_bytes", "record_embed", "release_embed", "is_interrupted",
            "tool_error", "prepare_image", "media_messages", "aux_call_kwargs",
            "load_auxiliary_client", "async_call_llm", "is_image_size_error", "call_vision_llm",
            "build_scale_note", "debug_call_data", "debug.log_call", "debug.save", "json.dumps",
            "stage", "should_use_native_vision_fast_path", "vision_analyze_native",
            "configured_aux_model", "vision_analyze_tool", "logger.debug", "uuid.uuid4",
            "urlparse_netloc", "unlink_quietly", "accepts_tool_result_images",
            "native_tool_result_images", "extract_content_or_reasoning", "too_large_message",
            "build_native_vision_tool_result"]


def self_test(fixpath):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    names = set(PY.values()) | set(seam_names())
    seen = set()
    problems = []
    n = 0
    for line in open(fixpath, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("{"):
            continue
        recd = json.loads(line)
        func = recd["func"]
        seen.add(func)
        n += 1
        try:
            out = run_case(func, recd.get("in") or {})
        except BaseException as exc:  # noqa: BLE001
            problems.append(f"DRIVE-FAIL {func}: {type(exc).__name__}: {exc}")
            continue
        ret = out.get("ret")
        if isinstance(ret, dict) and ret.get("__raise__") == "AttributeError":
            msg = ret.get("message", "")
            for nm in names:
                if f"'{nm}'" in msg:
                    problems.append(f"SUSPECT-ATTR {func}: {msg}")
                    break
    missing = set(PY) - seen
    if missing:
        problems.append(f"FIXTURE missing funcs: {sorted(missing)}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELF-TEST vision_analysis_flow: {n} cases, {len(problems)} problems")
    for p in problems[:40]:
        print("  " + p)
    return 0 if not problems else 1


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        fix = sys.argv[2] if len(sys.argv) > 2 else str(_HERE.parent / "fixtures" / "vision_analysis_flow.jsonl")
        sys.exit(self_test(fix))
    sys.exit(main())