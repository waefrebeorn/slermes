#!/usr/bin/env python3
"""Oracle for the UNCLAIMED-remainder FLOW cores of gateway/platforms/base.py
(src/gateway/platforms/platforms_base_flow2.c).

Drives the LIVE AST-extracted functions/methods with SCRIPTED seams: module
constants are seeded (Assign + AnnAssign), the CachedMedia dataclass and the
I/O-free _or_default helper are extracted real, and every cache-dir getter /
size validation / sniff / uuid / os.open / fdopen / Path probe / terminal-env
read / foreign-module helper / asyncio.to_thread dispatch / logger emission is
replaced by a recording stub returning the same scripted results the C harness
feeds its universal seam. Compares the DECISION SEQUENCE (ordered {ev,args,ret}
trace) plus the return value, propagated exception and self state.

Path doubles are a PurePosixPath subclass (RecPath) so str() keeps POSIX
separators on the Windows gate host; bytes fixtures are UTF-8 strings.
Import gates set sys.modules[full] = None (halt) vs a stub module (recorders).
FAIL-CLOSED: a seam with no script entry returns a RuntimeError raise spec.

Input: stdin JSON lines from t_port_platforms_base_flow2:
    {"func": ..., "in": {...}, "out": {"ret","exc","trace"[,"self"]}}
Summary: ORACLE platforms_base_flow2: N cases, M mismatches. Exit 1 on any
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
import types
import uuid as _uuid_mod
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


def proj_bytes(b):
    return b.decode("utf-8", errors="replace") if isinstance(b, bytes) else b


def mk_exc(kind, msg):
    """Real exception instance with str(e) == msg and the LIVE class identity
    for except-clause matching; the BaseException trio are real builtins so
    they pass through every `except Exception`-shaped site."""
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(msg)


def consume(name):
    """Script for a seam: single value reused, bare array consumed per call,
    or {"__script__":true,"ret":X} constant (list) result. FAIL-CLOSED on a
    missing entry (key presence, not .get() is None)."""
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


MODULE_TARGETS = [
    "_secure_media_cache_dir", "_write_cache_file",
    "cache_image_from_bytes", "cache_image_from_bytes_async",
    "cache_audio_from_bytes", "cache_audio_from_bytes_async",
    "cache_video_from_bytes", "cache_video_from_bytes_async",
    "cache_document_from_bytes", "cache_document_from_bytes_async",
    "_resolve_media_ext", "cache_media_bytes", "cache_media_bytes_async",
    "_sqlite_files", "_docker_env_active", "_docker_persistent_active",
    "_cache_dir_container_mounts", "_media_delivery_recency_seconds",
    "_resolve_path", "_parse_docker_volume_mounts", "_profile_dirs",
    "_credential_home_roots", "_existing_regular_file", "_mask_media_scan_text",
]
METHOD_TARGETS = [
    "_session_task_is_stale", "truncate_message", "supports_exec_approval_buttons",
]
# case func name -> extraction name (cross-checked against the fixture set)
KINDS = {nm: nm for nm in MODULE_TARGETS + METHOD_TARGETS}


def build_ns():
    mod = types.ModuleType("platforms_base_flow2_extracted")
    sys.modules["platforms_base_flow2_extracted"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "platforms_base_flow2_extracted", "json": json, "os": os,
               "re": re, "sys": sys, "logging": logging, "uuid": _uuid_mod,
               "asyncio": _asyncio, "Path": RecPath, "dataclass": dataclass,
               "field": field, "asdict": asdict})
    tree, src, p = extract_consts("gateway/platforms/base.py", ns)
    extract_funcs(tree, p, set(MODULE_TARGETS) | {"_or_default"}, ns)
    extract_methods(tree, p, "BasePlatformAdapter", set(METHOD_TARGETS), ns)
    # CachedMedia dataclass — keep the @dataclass decorator on ClassDef exec.
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "CachedMedia":
            start = node.decorator_list[0].lineno - 1 if node.decorator_list else node.lineno - 1
            seg = "\n".join(src.splitlines()[start:node.end_lineno])
            exec(compile(seg, p, "exec", ANN_FLAG), ns)
    for nm in MODULE_TARGETS + METHOD_TARGETS + ["_or_default"]:
        if nm not in ns:
            raise RuntimeError("extraction missing %s" % nm)
        LIVE[nm] = ns[nm]
        REAL[nm] = ns[nm]
    for nm in ("_TRUTHY", "_MEDIA_DELIVERY_TRUST_RECENT_DEFAULT_SECONDS", "_MIME_TO_EXT",
               "SUPPORTED_IMAGE_DOCUMENT_TYPES", "SUPPORTED_VIDEO_TYPES",
               "SUPPORTED_DOCUMENT_TYPES", "_AUDIO_MIME_TYPES", "CachedMedia"):
        if nm not in ns:
            raise RuntimeError("const seeding missing %s" % nm)
    return ns


# ── recording Path double (PurePosixPath keeps str() POSIX on Windows) ──
class RecPath(PurePosixPath):
    def expanduser(self):
        sc = consume("expanduser")
        record("expanduser", {"path": str(self)}, sc)
        maybe_raise(sc)
        return RecPath(sc) if isinstance(sc, str) else sc

    def resolve(self, strict=False):
        sc = consume("resolve")
        record("resolve", {"path": str(self), "strict": strict}, sc)
        maybe_raise(sc)
        return RecPath(sc) if isinstance(sc, str) else sc

    def is_file(self):
        sc = consume("is_file")
        record("is_file", {"path": str(self)}, sc)
        maybe_raise(sc)
        return sc

    def is_dir(self):
        sc = consume("is_dir")
        record("is_dir", {"path": str(self)}, sc)
        maybe_raise(sc)
        return sc

    def iterdir(self):
        sc = consume("iterdir")
        record("iterdir", {"path": str(self)}, sc)
        maybe_raise(sc)
        return [RecPath(x) if isinstance(x, str) else x for x in (sc or [])]

    def mkdir(self, parents=False, exist_ok=False):
        sc = consume("mkdir")
        record("mkdir", drop_none({"path": str(self), "parents": parents, "exist_ok": exist_ok}), sc)
        maybe_raise(sc)
        return sc

    def write_bytes(self, data):
        sc = consume("write_bytes")
        record("write_bytes", {"path": str(self), "data": proj_bytes(data)}, sc)
        maybe_raise(sc)
        return sc

    def is_relative_to(self, other):
        sc = consume("is_relative_to")
        record("is_relative_to", {"path": str(self), "other": str(other)}, sc)
        maybe_raise(sc)
        return sc


# ── recording stubs ──
def rec_fn(seam_name, argmap):
    def f(*args, **kwargs):
        a = argmap(*args, **kwargs)
        sc = consume(seam_name)
        record(seam_name, drop_none(a), sc)
        maybe_raise(sc)
        return sc
    return f


def dir_getter(seam_name):
    def f():
        sc = consume(seam_name)
        record(seam_name, {}, sc)
        maybe_raise(sc)
        return RecPath(sc) if isinstance(sc, str) else sc
    return f


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


def stub_uuid4():
    sc = consume("uuid4_hex")
    record("uuid4_hex", {}, sc)
    maybe_raise(sc)
    return types.SimpleNamespace(hex=sc)


def stub_os_open(path, flags, mode):
    sc = consume("os_open")
    record("os_open", {"path": path, "flags": flags, "mode": mode}, sc)
    maybe_raise(sc)
    return sc


class FdHandle:
    def __init__(self, fd):
        self.fd = fd

    def write(self, data):
        sc = consume("fdopen_write")
        record("fdopen_write", {"fd": self.fd, "data": proj_bytes(data)}, sc)
        maybe_raise(sc)
        return sc

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def stub_fdopen(fd, mode):
    return FdHandle(fd)


async def stub_to_thread(fn, *args, **kwargs):
    a = {"fn": fn.__name__, "args": [proj_bytes(x) for x in args]}
    if kwargs:
        a["kwargs"] = drop_none({k: proj_bytes(v) for k, v in kwargs.items()})
    sc = consume("to_thread")
    record("to_thread", a, sc)
    maybe_raise(sc)
    return sc


def mk_os(inp):
    return types.SimpleNamespace(
        open=stub_os_open, fdopen=stub_fdopen,
        O_WRONLY=1, O_CREAT=256, O_TRUNC=512,
        path=types.SimpleNamespace(
            expanduser=rec_fn("expanduser", lambda path: {"path": path}),
            splitext=os.path.splitext),
        pathsep=os.pathsep, environ={})


BASE_PROMPT_FN = lambda *a, **k: None


def make_adapter_double():
    return type("BasePlatformAdapter", (), {
        "_mask_protected_spans": staticmethod(rec_fn("mask_protected_spans", lambda text: {"text": text})),
        "_mask_json_string_media": staticmethod(rec_fn("mask_json_string_media", lambda text: {"text": text})),
        "_send_exec_approval_prompt": BASE_PROMPT_FN,
    })


IMPORT_GATES = {
    "hermes_cli.config": ("config_import_fail", {
        "is_managed": rec_fn("is_managed", lambda: {}),
        "_secure_dir": rec_fn("secure_dir", lambda path: {"path": str(path)}),
    }),
    "tools.audio_container": ("audio_import_fail", {
        "sniff_audio_ext": rec_fn("sniff_audio_ext",
                                  lambda data, fallback_ext=".ogg": {"data": proj_bytes(data), "ext": fallback_ext}),
    }),
    "tools.credential_files": ("cred_import_fail", {
        "get_cache_directory_mounts": rec_fn("get_cache_directory_mounts", lambda: {}),
    }),
    "gateway.media_policy": ("policy_import_fail", {
        "media_delivery_trust_recent": rec_fn("media_delivery_trust_recent", lambda: {}),
        "media_delivery_trust_recent_seconds": rec_fn("media_delivery_trust_recent_seconds", lambda: {}),
    }),
    "gateway.platforms.base_chunking": ("chunking_import_fail", {
        "chunk_markdown_message": rec_fn("chunk_markdown_message",
                                         lambda content, max_length, len_fn=None:
                                         {"content": content, "max_length": max_length, "len_fn": len_fn is not None}),
    }),
}


def install_import_stubs(inp):
    for pname in ("hermes_cli", "tools", "gateway", "gateway.platforms"):
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


def install_stubs(ns, inp):
    # restore every target to REAL first (a prior case may have left a recorder)
    for nm, fn in REAL.items():
        ns[nm] = fn
    ns["Path"] = RecPath
    ns["logger"] = RecLogger()          # AFTER const seeding (logger Assign)
    ns["os"] = mk_os(inp)
    ns["uuid"] = types.SimpleNamespace(uuid4=stub_uuid4)
    ns["asyncio"] = types.SimpleNamespace(to_thread=stub_to_thread)
    ns["_HERMES_ROOT"] = RecPath(inp.get("hermes_root", "/root/.hermes"))
    ns["validate_inbound_media_size"] = rec_fn(
        "validate_inbound_media_size",
        lambda size, *, media_type="media", max_bytes=None:
        {"size": size, "media_type": media_type, "max_bytes": max_bytes})
    ns["_looks_like_image"] = rec_fn("looks_like_image", lambda data: {"data": proj_bytes(data)})
    ns["make_user_dir"] = rec_fn(
        "make_user_dir",
        lambda path, parents=False, exist_ok=False, mode=None:
        {"path": str(path), "parents": parents, "exist_ok": exist_ok, "mode": mode})
    ns["get_image_cache_dir"] = dir_getter("get_image_cache_dir")
    ns["get_audio_cache_dir"] = dir_getter("get_audio_cache_dir")
    ns["get_video_cache_dir"] = dir_getter("get_video_cache_dir")
    ns["get_document_cache_dir"] = dir_getter("get_document_cache_dir")
    ns["_tenv"] = rec_fn("tenv", lambda name, default="": {"name": name, "default": default})
    ns["get_hermes_home"] = dir_getter("get_hermes_home")
    ns["_terminal_sentinel_start"] = rec_fn("terminal_sentinel_start", lambda text: {"text": text})
    ns["_blank_spans"] = rec_fn("blank_spans", lambda text, spans: {"text": text, "spans": [list(s) for s in spans]})
    ns["BasePlatformAdapter"] = make_adapter_double()
    install_import_stubs(inp)


def data_bytes(inp):
    return inp.get("data", "").encode("utf-8")


def project_ret(r):
    if isinstance(r, PurePosixPath):
        return str(r)
    if isinstance(r, tuple):
        return [project_ret(x) for x in r]
    if isinstance(r, list):
        return [project_ret(x) for x in r]
    if r is not None and type(r).__name__ == "CachedMedia":
        return asdict(r)
    return r


# ── drive the LIVE target for one case ──
def drive(ns, func, inp):
    CUR["state_out"] = {}
    if func == "_secure_media_cache_dir":
        return {"ret": ns["_secure_media_cache_dir"](RecPath(inp.get("cache_dir", "/cache")))}
    if func == "_write_cache_file":
        r = ns["_write_cache_file"](RecPath(inp.get("cache_dir", "/cache")),
                                    inp.get("prefix", "img"), inp.get("ext", ".jpg"), data_bytes(inp))
        return {"ret": r}
    if func == "cache_image_from_bytes":
        return {"ret": ns["cache_image_from_bytes"](data_bytes(inp), inp.get("ext", ".jpg"))}
    if func == "cache_image_from_bytes_async":
        return {"ret": _asyncio.run(ns["cache_image_from_bytes_async"](data_bytes(inp), inp.get("ext", ".jpg")))}
    if func == "cache_audio_from_bytes":
        return {"ret": ns["cache_audio_from_bytes"](data_bytes(inp), inp.get("ext", ".ogg"))}
    if func == "cache_audio_from_bytes_async":
        return {"ret": _asyncio.run(ns["cache_audio_from_bytes_async"](data_bytes(inp), inp.get("ext", ".ogg")))}
    if func == "cache_video_from_bytes":
        return {"ret": ns["cache_video_from_bytes"](data_bytes(inp), inp.get("ext", ".mp4"))}
    if func == "cache_video_from_bytes_async":
        return {"ret": _asyncio.run(ns["cache_video_from_bytes_async"](data_bytes(inp), inp.get("ext", ".mp4")))}
    if func == "cache_document_from_bytes":
        return {"ret": ns["cache_document_from_bytes"](data_bytes(inp), inp.get("filename", ""))}
    if func == "cache_document_from_bytes_async":
        return {"ret": _asyncio.run(ns["cache_document_from_bytes_async"](data_bytes(inp), inp.get("filename", "")))}
    if func == "_resolve_media_ext":
        return {"ret": ns["_resolve_media_ext"](inp.get("filename", ""), inp.get("mime_type", ""))}
    if func == "cache_media_bytes":
        r = ns["cache_media_bytes"](data_bytes(inp), filename=inp.get("filename", ""),
                                    mime_type=inp.get("mime_type", ""), default_kind=inp.get("default_kind"))
        return {"ret": project_ret(r)}
    if func == "cache_media_bytes_async":
        r = _asyncio.run(ns["cache_media_bytes_async"](data_bytes(inp), filename=inp.get("filename", ""),
                                                       mime_type=inp.get("mime_type", ""), default_kind=inp.get("default_kind")))
        return {"ret": project_ret(r)}
    if func == "_sqlite_files":
        return {"ret": list(ns["_sqlite_files"](inp.get("name", "")))}
    if func == "_docker_env_active":
        return {"ret": ns["_docker_env_active"]()}
    if func == "_docker_persistent_active":
        return {"ret": ns["_docker_persistent_active"]()}
    if func == "_cache_dir_container_mounts":
        return {"ret": [[str(h), str(c)] for h, c in ns["_cache_dir_container_mounts"]()]}
    if func == "_media_delivery_recency_seconds":
        return {"ret": ns["_media_delivery_recency_seconds"]()}
    if func == "_resolve_path":
        r = ns["_resolve_path"](RecPath(inp.get("path", "")), strict=inp.get("strict", False),
                                expand=inp.get("expand", False))
        return {"ret": str(r) if r is not None else None}
    if func == "_parse_docker_volume_mounts":
        return {"ret": [[str(h), str(c)] for h, c in ns["_parse_docker_volume_mounts"]()]}
    if func == "_profile_dirs":
        return {"ret": [str(p) for p in ns["_profile_dirs"]()]}
    if func == "_credential_home_roots":
        return {"ret": [str(p) for p in ns["_credential_home_roots"]()]}
    if func == "_existing_regular_file":
        return {"ret": ns["_existing_regular_file"](inp.get("raw", ""))}
    if func == "_mask_media_scan_text":
        return {"ret": ns["_mask_media_scan_text"](inp.get("text", ""))}
    if func == "_session_task_is_stale":
        st = inp.get("self") or {}
        self = types.SimpleNamespace()
        if "_session_tasks" in st:
            tin = st["_session_tasks"]
            if tin is None:
                self._session_tasks = None
            else:
                tasks = {}
                for k, t in tin.items():
                    if t is None:
                        tasks[k] = None
                    elif isinstance(t, dict) and "done" in t:
                        tasks[k] = types.SimpleNamespace(
                            done=rec_fn("task_done", lambda: {}) if t["done"] is not None else None)
                    else:
                        tasks[k] = types.SimpleNamespace()
                self._session_tasks = tasks
        self._session_task_is_stale = types.MethodType(ns["_session_task_is_stale"], self)
        CUR["state_out"]["self"] = copy.deepcopy(st)   # read-only flow: echo input
        return {"ret": self._session_task_is_stale(inp.get("session_key", ""))}
    if func == "truncate_message":
        len_fn = (lambda s: len(s)) if inp.get("len_fn") else None
        r = ns["truncate_message"](inp.get("content", ""), inp.get("max_length", 4096), len_fn)
        return {"ret": r}
    if func == "supports_exec_approval_buttons":
        base = ns["BasePlatformAdapter"]
        if inp.get("override"):
            cls = type("AdapterCls", (base,), {"_send_exec_approval_prompt": lambda *a, **k: None})
        else:
            cls = type("AdapterCls", (base,), {})
        return {"ret": ns["supports_exec_approval_buttons"](cls)}
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
        install_stubs(ns, inp)
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
    print("ORACLE platforms_base_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())