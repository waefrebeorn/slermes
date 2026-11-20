#!/usr/bin/env python3
"""Oracle for the UNCLAIMED-remainder FLOW cores of gateway/platforms/base.py
(src/gateway/platforms/platforms_base_flow3.c).

Drives the LIVE AST-extracted functions/methods with SCRIPTED seams: module
constants are seeded (Assign + AnnAssign), the _ExtractedResponse dataclass and
the nested _opt / _record_delivery closures are extracted real, the cache-dir
accessor factory is called to obtain real get_dir/cleanup closures, and every
claimed sibling / foreign-module helper / Path probe / regex finditer / logger
emission / context-manager enter-exit is replaced by a recording stub returning
the same scripted results the C harness feeds its universal seam. Compares the
DECISION SEQUENCE (ordered {ev,args,ret} trace) plus the return value, propagated
exception and echoed state (dropped / delivery flags).

Path doubles are a PurePosixPath subclass (RecPath) so str() keeps POSIX
separators on the Windows gate host. Complex regexes (MEDIA_TAG_CLEANUP_RE /
MEDIA_EXTENSIONLESS_TAG_RE) are replaced by doubles whose finditer yields
MatchDoubles built from the SAME scripted match list. Import gates set
sys.modules[full] = None (halt) vs a stub module (recorders). FAIL-CLOSED: a seam
with no script entry returns a RuntimeError raise spec. The BaseException trio
(+ asyncio.CancelledError) are real classes so they pass through except sites.

Input: stdin JSON lines from t_port_platforms_base_flow3:
    {"func": ..., "in": {...}, "out": {"ret","exc","trace"[,"dropped"|"state"]}}
Summary: ORACLE platforms_base_flow3: N cases, M mismatches. Exit 1 on any
mismatch, and fail-closed (exit 1) when zero cases were compared.
"""
import ast
import asyncio as _asyncio
import builtins
import copy
import json
import logging
import os
import re
import sys
import textwrap
import types
from dataclasses import dataclass, field, asdict
from pathlib import PurePosixPath

for _stream in (sys.stdout, sys.stdin):
    try:
        if hasattr(_stream, "reconfigure"):
            _stream.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

QUARRY = r"C:\Users\eman5\hermes-agent"
import __future__
ANN_FLAG = __future__.annotations.compiler_flag

CUR = {"func": None, "in": {}, "trace": [], "counts": {}, "state_out": {}}
LIVE = {}
REAL = {}


def rec(ev):
    CUR["trace"].append(ev)


def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


def is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


def mk_exc(kind, msg):
    """Real exception instance with str(e) == msg and the LIVE class identity for
    except-clause matching. The BaseException trio + CancelledError are real so
    they pass through every `except Exception`-shaped site. Unicode*Error builtins
    need a __new__ bypass (their __init__ demands 5 args)."""
    if kind == "CancelledError":
        return _asyncio.CancelledError(msg)
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


def consume(name):
    """Script for a seam: single value reused, bare array consumed per call, or
    {"__script__":true,"ret":X} constant (list) result. FAIL-CLOSED on a missing
    entry (key presence, not .get() is None)."""
    io = CUR["in"].get("io") or {}
    if name not in io:
        return {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    script = io[name]
    if isinstance(script, dict) and script.get("__script__") is True:
        return script.get("ret")
    if isinstance(script, list):
        i = CUR["counts"].get(name, 0)
        CUR["counts"][name] = i + 1
        if not script:
            return None
        return script[min(i, len(script) - 1)]
    return script


def record(name, args, script):
    rec({"ev": name, "args": args, "ret": script})


def maybe_raise(script):
    if is_raise(script):
        raise mk_exc(script["__raise__"], script.get("msg", ""))


def exc_norm(a):
    return {"__exc__": type(a).__name__, "msg": str(a)}


def match_proj(m):
    return {"start": m.start(), "end": m.end(), "path": m.group("path")}


def proj_source(src):
    if src is None:
        return None
    if isinstance(src, dict):
        return src
    return dict(vars(src))


# ── AST extraction ──
def extract_consts(rel, ns):
    p = os.path.join(QUARRY, rel)
    src = open(p, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        tg = []
        if isinstance(node, ast.Assign):
            tg = [t.id for t in node.targets if isinstance(t, ast.Name)]
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            tg = [node.target.id]
        if tg:
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec", ANN_FLAG), ns)
            except Exception:
                pass
    return tree, src, p


def extract_funcs(tree, p, wanted, ns):
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted:
            mm = copy.copy(node)
            mm.decorator_list = []
            exec(compile(ast.Module(body=[mm], type_ignores=[]), p, "exec", ANN_FLAG), ns)


def extract_methods(tree, p, class_name, wanted, ns):
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == class_name:
            for m in node.body:
                if isinstance(m, (ast.FunctionDef, ast.AsyncFunctionDef)) and m.name in wanted:
                    mm = copy.copy(m)
                    mm.decorator_list = []
                    exec(compile(ast.Module(body=[mm], type_ignores=[]), p, "exec", ANN_FLAG), ns)


def find_nested(tree, method_name, nested_name):
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "BasePlatformAdapter":
            for m in node.body:
                if isinstance(m, (ast.FunctionDef, ast.AsyncFunctionDef)) and m.name == method_name:
                    for sub in ast.walk(m):
                        if isinstance(sub, ast.FunctionDef) and sub.name == nested_name:
                            return sub
    return None


MODULE_TARGETS = [
    "_cache_dir_accessors", "validate_media_delivery_path", "_validated_delivery_path",
    "_deliverable_tag_spans", "_real_media_tag_spans", "_extensionless_media_matches",
]
METHOD_TARGETS = ["_synthesize_auto_tts", "_extract_response_content"]
# ported targets that are ALSO seamed siblings of other ported flows
PORTED_SIBLINGS = {
    "validate_media_delivery_path": "validate_media_delivery_path",
    "_real_media_tag_spans": "real_media_tag_spans",
    "_extensionless_media_matches": "extensionless_media_matches",
}
KINDS = {nm: nm for nm in MODULE_TARGETS + METHOD_TARGETS}
KINDS.update({"get_dir": "get_dir", "cleanup": "cleanup",
              "_record_delivery": "_record_delivery", "_opt": "_opt"})


def build_ns():
    mod = types.ModuleType("platforms_base_flow3_extracted")
    sys.modules["platforms_base_flow3_extracted"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "platforms_base_flow3_extracted", "json": json, "os": os,
               "re": re, "sys": sys, "logging": logging, "asyncio": _asyncio,
               "Path": RecPath, "dataclass": dataclass, "field": field, "asdict": asdict,
               "Optional": object, "Callable": object, "Any": object})
    tree, src, p = extract_consts("gateway/platforms/base.py", ns)
    extract_funcs(tree, p, set(MODULE_TARGETS), ns)
    extract_methods(tree, p, "BasePlatformAdapter", set(METHOD_TARGETS), ns)
    # _ExtractedResponse dataclass — keep the decorator on ClassDef exec.
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_ExtractedResponse":
            start = node.decorator_list[0].lineno - 1 if node.decorator_list else node.lineno - 1
            seg = "\n".join(src.splitlines()[start:node.end_lineno])
            exec(compile(seg, p, "exec", ANN_FLAG), ns)
    # nested _opt closure (no free vars) — extract standalone.
    opt_node = find_nested(tree, "build_source", "_opt")
    if opt_node is None:
        raise RuntimeError("nested _opt not found")
    mm = copy.copy(opt_node); mm.decorator_list = []
    exec(compile(ast.Module(body=[mm], type_ignores=[]), p, "exec", ANN_FLAG), ns)
    # nested _record_delivery closure — wrap in a factory holding the two cells.
    rd_node = find_nested(tree, "_process_message_background", "_record_delivery")
    if rd_node is None:
        raise RuntimeError("nested _record_delivery not found")
    nested_src = textwrap.dedent(ast.get_source_segment(src, rd_node))
    factory_src = (
        "def _record_factory(_init_a, _init_s):\n"
        "    delivery_attempted = _init_a\n"
        "    delivery_succeeded = _init_s\n"
        + textwrap.indent(nested_src, "    ") + "\n"
        "    return _record_delivery, lambda: (delivery_attempted, delivery_succeeded)\n"
    )
    exec(compile(factory_src, p, "exec", ANN_FLAG), ns)
    # bottom-of-module imports the seeder never sees (ImportFrom, not Assign).
    ns.setdefault("MEDIA_CACHE_MAX_AGE_HOURS", 24)
    for nm in MODULE_TARGETS + METHOD_TARGETS + ["_opt", "_record_factory"]:
        if nm not in ns:
            raise RuntimeError("extraction missing %s" % nm)
        LIVE[nm] = ns[nm]
        REAL[nm] = ns[nm]
    for nm in ("_IMAGE_EXTS", "_ExtractedResponse", "MEDIA_TAG_CLEANUP_RE",
               "MEDIA_EXTENSIONLESS_TAG_RE"):
        if nm not in ns:
            raise RuntimeError("const seeding missing %s" % nm)
    return ns


# ── recording Path double (PurePosixPath keeps str() POSIX on Windows) ──
class RecPath(PurePosixPath):
    def is_file(self):
        sc = consume("is_file")
        record("is_file", {"path": str(self)}, sc)
        maybe_raise(sc)
        return sc

    def expanduser(self):
        sc = consume("expanduser")
        record("expanduser", {"path": str(self)}, sc)
        maybe_raise(sc)
        return RecPath(sc) if isinstance(sc, str) else sc


# ── regex match / pattern doubles (finditer seamed) ──
class MatchDouble:
    def __init__(self, d):
        self._d = d

    def start(self):
        return self._d["start"]

    def end(self):
        return self._d["end"]

    def span(self):
        return (self._d["start"], self._d["end"])

    def group(self, name=None):
        if name is None:
            return self._d.get("raw", self._d.get("path", ""))
        return self._d.get(name, "")


class RegexDouble:
    def __init__(self, seam_name):
        self.seam = seam_name

    def finditer(self, text):
        sc = consume(self.seam)
        record(self.seam, {"text": text}, sc)
        maybe_raise(sc)
        return [MatchDouble(m) for m in (sc or [])]


# ── recording stubs ──
def rec_fn(seam_name, argmap, build=None):
    def f(*args, **kwargs):
        a = argmap(*args, **kwargs)
        sc = consume(seam_name)
        record(seam_name, drop_none(a), sc)
        maybe_raise(sc)
        return build(sc) if build else sc
    return f


def rec_async_fn(seam_name, argmap, build=None):
    async def f(*args, **kwargs):
        a = argmap(*args, **kwargs)
        sc = consume(seam_name)
        record(seam_name, drop_none(a), sc)
        maybe_raise(sc)
        return build(sc) if build else sc
    return f


def _path_build(sc):
    return RecPath(sc) if isinstance(sc, str) else sc


def _path_list_build(sc):
    return [RecPath(x) if isinstance(x, str) else x for x in (sc or [])]


class RecLogger:
    def _emit(self, level, msg, *args, **kwargs):
        norm = [exc_norm(a) if isinstance(a, BaseException)
                else (str(a) if isinstance(a, PurePosixPath) else a) for a in args]
        sc = consume("log_" + level)
        record("log_" + level, {"msg": msg, "args": norm}, sc)
        maybe_raise(sc)

    def debug(self, msg, *a, **k): self._emit("debug", msg, *a, **k)
    def info(self, msg, *a, **k): self._emit("info", msg, *a, **k)
    def warning(self, msg, *a, **k): self._emit("warning", msg, *a, **k)
    def error(self, msg, *a, **k): self._emit("error", msg, *a, **k)


def build_auto_tts_output_path(*a, **k):
    return None


class ScopeCM:
    """_media_delivery_scope context-manager double: enter/exit are seams; exit
    returns the scripted truthiness (suppress) and may itself raise."""
    def __init__(self, source):
        self.source = source

    def __enter__(self):
        sc = consume("media_delivery_scope_enter")
        record("media_delivery_scope_enter", drop_none({"source": proj_source(self.source)}), sc)
        maybe_raise(sc)
        return sc

    def __exit__(self, et, ev, tb):
        kind = et.__name__ if et else None
        sc = consume("media_delivery_scope_exit")
        record("media_delivery_scope_exit", drop_none({"exc": kind}), sc)
        maybe_raise(sc)
        return bool(sc)


IMPORT_GATES = {
    "tools.hosted_tool_policy": ("hosted_policy_import_fail", {
        "host_feature_enabled": rec_fn("host_feature_enabled", lambda feature: {"feature": feature}),
    }),
    "gateway.media_policy": ("media_policy_import_fail", {
        "media_delivery_strict": rec_fn("media_delivery_strict", lambda: {}),
    }),
    "gateway.media_fetch": ("media_fetch_import_fail", {
        "fetch_remote_media": rec_fn("fetch_remote_media", lambda raw: {"raw": raw}),
    }),
    "gateway.platforms.base_auto_tts": ("auto_tts_import_fail", {
        "synthesize_auto_tts": rec_async_fn(
            "synthesize_auto_tts",
            lambda slf, text, source, build_fn: {
                "self": {"name": slf.name}, "text": text,
                "source": proj_source(source), "build_fn": build_fn.__name__}),
    }),
    "gateway.platforms.docker_media_paths": ("docker_media_import_fail", {
        "hand_back_unreadable_media": rec_async_fn(
            "hand_back_unreadable_media", lambda paths: {"paths": paths}),
    }),
}


def install_import_stubs(inp):
    for pname in ("tools", "gateway", "gateway.platforms"):
        m = types.ModuleType(pname)
        m.__path__ = []
        sys.modules[pname] = m
    sys.modules["gateway"].platforms = sys.modules["gateway.platforms"]
    for full, (flag, attrs) in IMPORT_GATES.items():
        parent, _, leaf = full.rpartition(".")
        if inp.get(flag):
            sys.modules[full] = None
            if sys.modules.get(parent) is not None:
                setattr(sys.modules[parent], leaf, None)
        else:
            m = types.ModuleType(full)
            m.__path__ = []
            for k, v in attrs.items():
                setattr(m, k, v)
            sys.modules[full] = m
            if sys.modules.get(parent) is not None:
                setattr(sys.modules[parent], leaf, m)


# seamed siblings + foreign module globals (always recorders)
def install_stubs(ns, inp, func):
    for nm, fn in REAL.items():
        ns[nm] = fn
    ns["Path"] = RecPath
    ns["logger"] = RecLogger()
    ns["os"] = types.SimpleNamespace(
        path=types.SimpleNamespace(expanduser=rec_fn("expanduser", lambda path: {"path": path}),
                                   splitext=os.path.splitext))
    ns["MEDIA_CACHE_MAX_AGE_HOURS"] = inp.get("media_cache_max_age_hours", 24)
    ns["get_hermes_dir"] = rec_fn("get_hermes_dir",
                                  lambda subpath, old_name: {"subpath": subpath, "old_name": old_name},
                                  build=_path_build)
    ns["_secure_media_cache_dir"] = rec_fn("secure_media_cache_dir", lambda path: {"path": str(path)})
    ns["unlink_files_older_than"] = rec_fn(
        "unlink_files_older_than",
        lambda path, pattern, max_age_seconds: {"path": str(path), "pattern": pattern,
                                                "max_age_seconds": max_age_seconds})
    ns["is_project_env_basename"] = rec_fn("is_project_env_basename", lambda name: {"name": name})
    ns["_normalize_media_tag_path"] = rec_fn("normalize_media_tag_path", lambda path: {"path": path})
    ns["_translate_docker_container_media_path"] = rec_fn(
        "translate_docker", lambda path, session_key="": {"path": str(path), "session_key": session_key},
        build=_path_build)
    ns["_resolve_path"] = rec_fn(
        "resolve_path", lambda path, strict=False, expand=False: {"path": str(path), "strict": strict, "expand": expand},
        build=_path_build)
    ns["_media_delivery_allowed_roots"] = rec_fn("allowed_roots", lambda: {}, build=_path_list_build)
    ns["_path_is_within"] = rec_fn("path_is_within", lambda path, root: {"path": str(path), "root": str(root)})
    ns["_path_under_denied_prefix"] = rec_fn("path_under_denied_prefix", lambda path: {"path": str(path)})
    ns["_media_delivery_recency_seconds"] = rec_fn("recency_seconds", lambda: {})
    ns["_file_is_recently_produced"] = rec_fn("file_is_recently_produced",
                                              lambda path, window: {"path": str(path), "window": window})
    ns["_existing_regular_file"] = rec_fn("existing_regular_file", lambda raw: {"raw": raw})
    ns["_log_safe_path"] = rec_fn("log_safe_path", lambda raw: {"raw": raw})
    ns["_mask_media_scan_text"] = rec_fn("mask_media_scan_text", lambda text: {"text": text})
    ns["_terminal_sentinel_start"] = rec_fn("terminal_sentinel_start", lambda text: {"text": text})
    ns["_path_lacks_deliverable_extension"] = rec_fn("path_lacks_deliverable_extension", lambda path: {"path": path})
    ns["_match_extensionless_path"] = rec_fn(
        "match_extensionless_path",
        lambda scan_text, match: {"masked": scan_text, "match": match_proj(match)})
    ns["_strip_media_directives"] = rec_fn("strip_media_directives", lambda text: {"text": text})
    ns["footer_onto_photo_caption"] = rec_fn(
        "footer_onto_photo_caption",
        lambda slf, extracted, image_exts: {"self": {"name": slf.name}, "extracted": asdict(extracted),
                                            "image_exts": sorted(image_exts)})
    ns["build_auto_tts_output_path"] = build_auto_tts_output_path
    ns["MEDIA_TAG_CLEANUP_RE"] = RegexDouble("cleanup_finditer")
    ns["MEDIA_EXTENSIONLESS_TAG_RE"] = RegexDouble("extensionless_finditer")
    # ported siblings seamed when they are NOT the current target
    for live_nm, seam_nm in PORTED_SIBLINGS.items():
        if live_nm != func:
            if live_nm == "_extensionless_media_matches":
                ns[live_nm] = _ext_matches_rec(seam_nm)
            elif live_nm == "_real_media_tag_spans":
                ns[live_nm] = _real_spans_rec(seam_nm)
            else:
                ns[live_nm] = rec_fn(seam_nm, lambda path, session_key="": {"path": path, "session_key": session_key})
    install_import_stubs(inp)


def _ext_matches_rec(seam_nm):
    def f(masked):
        sc = consume(seam_nm)
        record(seam_nm, {"masked": masked}, sc)
        maybe_raise(sc)
        return [(MatchDouble(m), safe, end) for m, safe, end in (sc or [])]
    return f


def _real_spans_rec(seam_nm):
    def f(masked):
        sc = consume(seam_nm)
        record(seam_nm, {"masked": masked}, sc)
        maybe_raise(sc)
        return [tuple(sp) for sp in (sc or [])]
    return f


def project_ret(r):
    if isinstance(r, PurePosixPath):
        return str(r)
    if isinstance(r, tuple):
        return [project_ret(x) for x in r]
    if isinstance(r, list):
        return [project_ret(x) for x in r]
    if isinstance(r, set):
        return sorted(r)
    if r is not None and type(r).__name__ == "_ExtractedResponse":
        return asdict(r)
    return r


def make_adapter_double(inp):
    """Self double for _extract_response_content: .name + seamed collaborator
    methods + the _media_delivery_scope CM factory."""
    def scope(source):
        return ScopeCM(source)
    ns_d = {
        "name": inp.get("name", "telegram"),
        "extract_media": staticmethod(rec_fn("extract_media", lambda content: {"content": content})),
        "filter_media_delivery_paths": staticmethod(rec_fn(
            "filter_media_delivery_paths",
            lambda media_files, session_key="", dropped=None: {"media_files": media_files, "session_key": session_key})),
        "extract_images": staticmethod(rec_fn("extract_images", lambda response: {"response": response})),
        "extract_local_files": staticmethod(rec_fn("extract_local_files", lambda text: {"text": text})),
        "filter_local_delivery_paths": staticmethod(rec_fn(
            "filter_local_delivery_paths",
            lambda file_paths, session_key="": {"files": file_paths, "session_key": session_key})),
        "_bounded_history_media_paths_for_session": staticmethod(rec_async_fn(
            "bounded_history", lambda session_key: {"session_key": session_key})),
        "_media_delivery_scope": staticmethod(scope),
    }
    return type("BasePlatformAdapter", (), ns_d)()


# ── drive the LIVE target for one case ──
def drive(ns, func, inp):
    CUR["state_out"] = {}
    if func == "_cache_dir_accessors":
        gd, cl = ns["_cache_dir_accessors"](inp.get("kind", "image"),
                                            inp.get("constant_name", "IMAGE_CACHE_DIR"),
                                            inp.get("new_subpath", "cache/images"),
                                            inp.get("old_name", "image_cache"))
        return {"ret": {"get_dir_name": gd.__name__, "cleanup_name": cl.__name__}}
    if func in ("get_dir", "cleanup"):
        cn = inp.get("constant_name", "IMAGE_CACHE_DIR")
        ns[cn] = inp.get("constant_value")
        ns["_CACHE_DIR_IMPORT_DEFAULTS"] = inp.get("import_defaults", {})
        gd, cl = ns["_cache_dir_accessors"](inp.get("kind", "image"), cn,
                                            inp.get("new_subpath", "cache/images"),
                                            inp.get("old_name", "image_cache"))
        if func == "get_dir":
            return {"ret": str(gd())}
        return {"ret": cl(inp.get("max_age_hours", ns["MEDIA_CACHE_MAX_AGE_HOURS"]))}
    if func == "validate_media_delivery_path":
        return {"ret": ns["validate_media_delivery_path"](inp.get("path", ""),
                                                          session_key=inp.get("session_key", ""))}
    if func == "_validated_delivery_path":
        dropped = inp.get("dropped")
        dcopy = copy.deepcopy(dropped) if isinstance(dropped, list) else None
        CUR["state_out"]["dropped"] = dcopy   # echoed even when the call raises
        r = ns["_validated_delivery_path"](inp.get("raw_path", ""), inp.get("session_key", ""),
                                           inp.get("label", "MEDIA directive path"), dcopy)
        return {"ret": r}
    if func == "_deliverable_tag_spans":
        return {"ret": project_ret(ns["_deliverable_tag_spans"](inp.get("text", "")))}
    if func == "_real_media_tag_spans":
        return {"ret": [list(sp) for sp in ns["_real_media_tag_spans"](inp.get("masked", ""))]}
    if func == "_extensionless_media_matches":
        r = list(ns["_extensionless_media_matches"](inp.get("masked", "")))
        return {"ret": [[match_proj(m), safe, end] for m, safe, end in r]}
    if func == "_synthesize_auto_tts":
        slf = types.SimpleNamespace(name=inp.get("name", "telegram"))
        src = types.SimpleNamespace(**inp["source"]) if inp.get("source") else None
        r = _asyncio.run(ns["_synthesize_auto_tts"](slf, inp.get("text_content", ""), src))
        return {"ret": project_ret(r)}
    if func == "_record_delivery":
        st = inp.get("state") or {}
        closure, getter = ns["_record_factory"](st.get("delivery_attempted", False),
                                                st.get("delivery_succeeded", False))
        res = inp.get("result")
        result = types.SimpleNamespace(**res) if isinstance(res, dict) else res
        closure(result)
        a, s = getter()
        CUR["state_out"]["state"] = {"delivery_attempted": a, "delivery_succeeded": s}
        return {"ret": None}
    if func == "_opt":
        return {"ret": ns["_opt"](inp.get("value"))}
    if func == "_extract_response_content":
        slf = make_adapter_double(inp)
        event = types.SimpleNamespace(source=types.SimpleNamespace(**(inp.get("source") or {})))
        r = _asyncio.run(ns["_extract_response_content"](
            slf, inp.get("response", ""), event, inp.get("session_key", ""),
            is_ephemeral_response=inp.get("is_ephemeral_response", False)))
        return {"ret": project_ret(r)}
    raise ValueError("unknown func: %s" % func)


def run_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["counts"] = {}
    CUR["state_out"] = {}
    try:
        out = drive(ns, func, inp)
        out["exc"] = None
    except BaseException as e:
        out = {"ret": None, "exc": {"__raise__": type(e).__name__, "msg": str(e)}}
    for k, v in CUR["state_out"].items():
        out[k] = v
    out["trace"] = CUR["trace"]
    return out


# ── comparison ──
def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def compare(want, got):
    if canon(want.get("trace", [])) != canon(got.get("trace", [])):
        return False
    for k in set(want) | set(got):
        if k == "trace":
            continue
        if canon(want.get(k)) != canon(got.get(k)):
            return False
    return True


def main():
    ns = build_ns()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record_in = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record_in.get("func")
        inp = record_in.get("in") or {}
        got = record_in.get("out") or {}
        cases += 1
        install_stubs(ns, inp, func)
        try:
            want = run_case(ns, func, inp)
        except BaseException as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        if not compare(want, got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE platforms_base_flow3: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())