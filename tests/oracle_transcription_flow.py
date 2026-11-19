#!/usr/bin/env python3
"""Oracle for src/tools/transcription_flow.c — drives the LIVE UNCLAIMED
transcription-surface defs of tools/transcription_common.py +
tools/transcription_chunking.py + tools/transcription_whisper_cpp.py under
IDENTICAL scripted seams.

The task names the target "tools/transcription.py"; that file does not exist in
the quarry — the transcription surface is the tools/transcription_*.py family,
and these three modules are FULLY unclaimed (PoP grep census). PoP annotations
credit the REAL defining modules.

The 19 ported targets + STTResponseError are AST-extracted in source order;
every node is compiled with the PEP 563 future flag so annotations stay strings,
and the exec namespace is a registered sys.modules module (py3.14 needs a real
module alias). The heavy quarry import chain is never imported. Ported
functions call each other REAL (internal composition); every collaborator
(claimed siblings _transcode_audio_for_stt / _find_ffmpeg_binary /
_probe_audio_duration, the unclaimed I/O leaf _run_quiet, os.stat/listdir/
getsize/getenv, pm / pm.downloader, TemporaryDirectory, voice_mode_transcript,
logger) is a recording stub fed the SAME "io" scripts the C harness feeds
tr_call. os.path is posixpath and pathlib is PurePosixPath so paths stay POSIX
on the Windows gate host.

Script shapes (shared with the C universal seam): absent key -> None;
{"$seq": [...]} per-call queue (exhaustion raises RuntimeError("script exhausted:
<name>")); {"__raise__": cls, "msg": m} raises a REAL exception (trace ret is the
WHOLE marker); any other value (incl. arrays) reused. None-valued seam arg keys
dropped on both sides. Import gates in.pm_ok / in.pm_downloader_ok (default
true) fire no seam event; a gated import halts via sys.modules[mod]=None
(ModuleNotFoundError). Comparison: ordered trace + canonical ret + post-call
state ({"verified_files": {...}}; {} on any raise). Integral floats collapse to
ints; tuples -> arrays. Fail-closed: 0 cases => exit 1; any per-case exception
rides as {"__raise__": cls} (BaseException, so SystemExit/KeyboardInterrupt
propagate). Summary: 'ORACLE transcription_flow: N cases, M mismatches'.
"""
import __future__
import ast
import builtins
import json
import math
import os
import posixpath
import re
import shlex
import subprocess
import sys
import types
from pathlib import Path, PurePath, PurePosixPath

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "tools" / "transcription_common.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
NAME = "transcription_flow"

# (module, [funcs]) in composition order: common leaves first.
SOURCES = [
    ("tools/transcription_common.py", [
        "_error_result", "normalize_xai_stt_model", "_ok_result",
        "_lazy_ensure_quietly", "_process_error_detail", "_log_prompt_unsupported",
        "_config_number",
    ]),
    ("tools/transcription_chunking.py", [
        "upload_limit", "_fits", "exceeds_upload_limit", "_too_large",
        "_silence_midpoints", "_cut_points", "_split", "transcribe_oversized",
    ]),
    ("tools/transcription_whisper_cpp.py", [
        "_model_dir", "_file_signature", "whisper_cpp_command",
        "ensure_whisper_cpp_models",
    ]),
]
PORTED = [f for _, fs in SOURCES for f in fs]
CLASSES = ["STTResponseError"]  # tools/transcription_common.py
CONSTS = {
    "tools/transcription_common.py": [
        "MAX_FILE_SIZE", "STT_MODEL_CATALOG", "LEGACY_XAI_STT_MODEL",
        "XAI_STT_MODEL_ENV",
    ],
    "tools/transcription_chunking.py": [
        "_MB", "_PROVIDER_MAX_BYTES", "_PROVIDER_MAX_SECONDS",
        "_OPENAI_MODEL_MAX_SECONDS", "_SEGMENT_HEADROOM",
        "_MAX_SILENCE_LOOKBACK_SECONDS",
    ],
    "tools/transcription_whisper_cpp.py": ["_MODEL_FILES"],
}
# Claimed siblings + unclaimed I/O leaves that stay SEAMED here.
SEAMED_SIBLINGS = [
    "_find_ffmpeg_binary", "_probe_audio_duration", "_run_quiet",
    "_transcode_audio_for_stt",
]
WANTED = set(PORTED)

# ── AST extraction ──────────────────────────────────────────────────────────
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CLASS_CODE = {}
CONST_CODE = []  # (name, code) in exec order
for _rel, _funcs in SOURCES:
    _src = str(QUARRY / _rel)
    _tree = ast.parse(Path(_src).read_text(encoding="utf-8"))
    _want = set(_funcs)
    _consts = set(CONSTS[_rel])
    _found = set()
    for _node in _tree.body:
        if isinstance(_node, ast.FunctionDef) and _node.name in _want:
            DEF_CODE[_node.name] = _compile_node(_node, _src)
            _found.add(_node.name)
        elif isinstance(_node, ast.ClassDef) and _node.name in CLASSES:
            CLASS_CODE[_node.name] = _compile_node(_node, _src)
        elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
            tgts = _node.targets if isinstance(_node, ast.Assign) else [_node.target]
            for _t in tgts:
                if isinstance(_t, ast.Name) and _t.id in _consts:
                    CONST_CODE.append((_t.id, _compile_node(_node, _src)))
    _missing = _want - _found
    if _missing:
        print(f"ORACLE {NAME}: FATAL missing live defs in {_rel}: {sorted(_missing)}",
              file=sys.stderr)
        sys.exit(1)
_missing = (set(CLASSES) - set(CLASS_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live classes: {sorted(_missing)}", file=sys.stderr)
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
        return {norm_key(k): norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def norm_key(k):
    return k.as_posix() if isinstance(k, PurePath) else k


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    # custom quarry classes first: the EXTRACTED class object (identity matters
    # for except ValueError / except Exception catches)
    cls = _BASE_NS.get(cls_name)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    cls = getattr(builtins, cls_name, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    sub = getattr(subprocess, cls_name, None)
    if isinstance(sub, type) and issubclass(sub, BaseException):
        if sub is subprocess.CalledProcessError:
            return sub(1, msg)
        if sub is subprocess.TimeoutExpired:
            return sub(msg, 300)
        return sub(msg)
    return type(cls_name, (Exception,), {})(msg)


# ── scripted seams ──────────────────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        script = self.io.get(name)
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


class TmpDirDouble:
    """TemporaryDirectory CM: one seam event at construction+enter; __exit__
    (ignore_cleanup_errors=True) fires no event and never raises."""

    def __init__(self, work_dir):
        self.work_dir = work_dir

    def __enter__(self):
        return self.work_dir

    def __exit__(self, *exc):
        return False


class DownloadDouble:
    def __init__(self):
        self.run = SeamFn("Download.run")


def mat_bytes(v):
    if isinstance(v, dict) and isinstance(v.get("__bytes__"), str):
        return bytes.fromhex(v["__bytes__"])
    return v


def materialize(name, script):
    if script is None:
        return None
    if name == "_run_quiet":
        return types.SimpleNamespace(**script) if isinstance(script, dict) else script
    if name == "_transcode_audio_for_stt":
        return tuple(script) if isinstance(script, list) else script
    if name == "TemporaryDirectory":
        return TmpDirDouble(script)
    if name == "stat":
        return types.SimpleNamespace(**script) if isinstance(script, dict) else script
    if name == "pm.installed_package":
        if isinstance(script, dict):
            return types.SimpleNamespace(binary=PurePosixPath(script["binary"]))
        return script
    if name == "Source":
        if isinstance(script, dict):
            return types.SimpleNamespace(
                url=script["url"], dest=PurePosixPath(script["dest"]),
                sha256=script["sha256"])
        return script
    if name == "Download":
        return DownloadDouble() if isinstance(script, dict) else script
    if name == "get_hermes_home":
        return PurePosixPath(script) if isinstance(script, str) else script
    return script


# ── seam arg projections (identical field sets to the C args objects) ───────
SEAM_ARGS = {
    "os.getenv": lambda a, k: {"name": a[0], "default": a[1] if len(a) > 1 else None},
    "pm.ensure_import": lambda a, k: {"extra": a[0]},
    "os.path.getsize": lambda a, k: {"path": a[0]},
    "_probe_audio_duration": lambda a, k: {"path": a[0]},
    "_run_quiet": lambda a, k: {"argv": a[0], "timeout": k["timeout"]},
    "_find_ffmpeg_binary": lambda a, k: {},
    "_transcode_audio_for_stt": lambda a, k: {"path": a[0], "work_dir": a[1]},
    "TemporaryDirectory": lambda a, k: {
        "prefix": k["prefix"], "ignore_cleanup_errors": k["ignore_cleanup_errors"]},
    "os.listdir": lambda a, k: {"path": a[0]},
    "is_whisper_hallucination": lambda a, k: {"text": a[0]},
    "get_hermes_home": lambda a, k: {},
    "stat": lambda a, k: {"path": a[0].as_posix()},
    "pm.installed_package": lambda a, k: {
        "name": a[0], "allow_outdated": k["allow_outdated"]},
    "Source": lambda a, k: {"url": a[0], "dest": a[1].as_posix(), "sha256": a[2]},
    "Download": lambda a, k: {
        "sources": [{"url": s.url, "dest": s.dest.as_posix(), "sha256": s.sha256}
                    for s in a[0]],
        "partials_dir": k["partials_dir"].as_posix()},
    "Download.run": lambda a, k: {},
}


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    result = materialize(name, script)
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return result


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        proj = SEAM_ARGS.get(self.name, lambda a, k: {})
        return record(self.name, proj(a, kw))


class CallSeam:
    """A callable PARAMETER (dispatch / cast) riding a named seam."""

    def __init__(self, name, argkey):
        self.name = name
        self.argkey = argkey

    def __call__(self, *a, **kw):
        return record(self.name, {self.argkey: a[0] if a else None})


class LoggerDouble:
    def _log(self, level, msg, args):
        # exception args project to str(exc) (the C port builds the message
        # string from the raise descriptor); Path args to POSIX str.
        proj = [str(x) if isinstance(x, BaseException)
                else x.as_posix() if isinstance(x, PurePath) else x for x in args]
        return record("logger." + level, {"msg": msg, "args": proj})

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args)


_LOGGER = LoggerDouble()


# ── stub modules for lazy imports (singletons dispatch to current scripts) ──
def _mod(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts) + 1):
        nm = ".".join(parts[:i])
        m = sys.modules.get(nm)
        if m is None:
            m = types.ModuleType(nm)
            m.__path__ = []
            sys.modules[nm] = m
        if i > 1:
            setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], m)
    for kk, vv in attrs.items():
        setattr(sys.modules[name], kk, vv)
    return sys.modules[name]


_PM = _mod("pm",
           ensure_import=SeamFn("pm.ensure_import"),
           installed_package=SeamFn("pm.installed_package"))
_DL = _mod("pm.downloader",
           Download=SeamFn("Download"), Source=SeamFn("Source"))
_VMT = _mod("tools.voice_mode_transcript",
            is_whisper_hallucination=SeamFn("is_whisper_hallucination"))


def install_path_patches():
    def stat_stub(self):
        return SeamFn("stat")(self)

    PurePosixPath.stat = stat_stub


# ── base namespace ──────────────────────────────────────────────────────────
_OS_DOUBLE = types.SimpleNamespace(
    getenv=SeamFn("os.getenv"),
    listdir=SeamFn("os.listdir"),
    path=types.SimpleNamespace(
        getsize=SeamFn("os.path.getsize"),
        join=posixpath.join,
        basename=posixpath.basename),
)
_BASE_NS = {
    "__name__": "tr_live_transcription",
    "__builtins__": builtins,
    "json": json, "re": re, "math": math, "shlex": shlex,
    "subprocess": subprocess,
    "os": _OS_DOUBLE,
    "Path": PurePosixPath,
    "TemporaryDirectory": SeamFn("TemporaryDirectory"),
    "get_hermes_home": SeamFn("get_hermes_home"),
    "logger": _LOGGER,
    "Any": object, "Callable": object, "Dict": object, "List": object,
    "Optional": object, "Tuple": object,
}
_MOD = types.ModuleType("tr_live_transcription")
_MOD.__dict__.update(_BASE_NS)
sys.modules["tr_live_transcription"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm, _code in CONST_CODE:
    exec(_code, _BASE_NS)
for _nm in CLASSES:
    exec(CLASS_CODE[_nm], _BASE_NS)
for _nm in PORTED:
    exec(DEF_CODE[_nm], _BASE_NS)
_BASE_NS["logger"] = _LOGGER  # re-set AFTER the seeding loop (Assign lesson)
for _nm in SEAMED_SIBLINGS:
    _BASE_NS[_nm] = SeamFn(_nm)
install_path_patches()


def mat_limit(v):
    return tuple(v) if isinstance(v, list) else v


def mat_exc(d):
    if d is None:
        return None  # live None.stderr raises AttributeError (fail-closed case)
    return subprocess.CalledProcessError(
        d.get("returncode", 1), d.get("cmd", ""),
        output=mat_bytes(d.get("stdout")), stderr=mat_bytes(d.get("stderr")))


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    _SCRIPTS = Scripts(inp.get("io") or {}, trace)
    st = inp.get("state") or {}
    _BASE_NS["_verified_files"] = {
        PurePosixPath(k): tuple(v) for k, v in (st.get("verified_files") or {}).items()}
    _BASE_NS["logger"] = _LOGGER
    # import gates: None in sys.modules HALTS the import (ModuleNotFoundError);
    # the parent attribute is gated too so `from pm import downloader` fails.
    if inp.get("pm_ok", True):
        sys.modules["pm"] = _PM
    else:
        sys.modules["pm"] = None
    if inp.get("pm_downloader_ok", True):
        sys.modules["pm.downloader"] = _DL
        _PM.downloader = _DL
    else:
        sys.modules["pm.downloader"] = None
        _PM.downloader = None
    sys.modules["tools.voice_mode_transcript"] = _VMT
    return _BASE_NS, trace


def drive(rec, ns, inp):
    func = rec["func"]
    if func == "_error_result":
        return ns[func](inp.get("error"), **(inp.get("extra_kw") or {}))
    if func == "normalize_xai_stt_model":
        return ns[func](inp.get("model"))
    if func == "_ok_result":
        return ns[func](inp.get("transcript"), inp.get("provider"))
    if func == "_lazy_ensure_quietly":
        ns[func](inp.get("extra", ""))
        return None
    if func == "_process_error_detail":
        return ns[func](mat_exc(inp.get("exc")))
    if func == "_log_prompt_unsupported":
        ns[func](inp.get("label", ""))
        return None
    if func == "_config_number":
        return ns[func](inp.get("cfg"), inp.get("key", ""), inp.get("default"),
                        CallSeam(inp.get("cast_seam", "cast"), "value"))
    if func == "upload_limit":
        return ns[func](inp.get("provider", ""), inp.get("model"))
    if func == "_fits":
        return ns[func](inp.get("path", ""), mat_limit(inp.get("limit")),
                        inp.get("duration"))
    if func == "exceeds_upload_limit":
        return ns[func](inp.get("path", ""), mat_limit(inp.get("limit")))
    if func == "_too_large":
        return ns[func](inp.get("path", ""), inp.get("max_bytes", 0),
                        inp.get("reason", ""))
    if func == "_silence_midpoints":
        return ns[func](inp.get("ffmpeg", ""), inp.get("path", ""))
    if func == "_cut_points":
        return ns[func](inp.get("duration", 0), inp.get("target", 0),
                        inp.get("silences") or [])
    if func == "_split":
        return ns[func](inp.get("ffmpeg", ""), inp.get("path", ""),
                        inp.get("work_dir", ""), inp.get("cuts") or [])
    if func == "transcribe_oversized":
        return ns[func](inp.get("path", ""), mat_limit(inp.get("limit")),
                        CallSeam(inp.get("dispatch_seam", "dispatch"), "path"))
    if func == "_model_dir":
        return ns[func]()
    if func == "_file_signature":
        return ns[func](PurePosixPath(inp.get("path", "")), inp.get("digest", ""))
    if func == "whisper_cpp_command":
        return ns[func]()
    if func == "ensure_whisper_cpp_models":
        ns[func](inp.get("model_name", ""))
        return None
    raise KeyError(f"unknown func {func}")


def out_state(ns):
    return {"verified_files": {k.as_posix(): norm(v)
                               for k, v in ns["_verified_files"].items()}}


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
                ret = drive(rec, ns, rec["in"])
                out = {"ret": norm(ret), "trace": trace, "state": out_state(ns)}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace, "state": {}}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 12:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test: drive every fixture, flag binding bugs / drive failures ──────
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(90, exit=True)
    bad = 0
    names = set(PORTED) | set(SEAMED_SIBLINGS)
    seen_funcs = set()
    fail_closed = {}
    for i, rec in enumerate(fixtures):
        seen_funcs.add(rec["func"])
        try:
            ns, trace = build_ns(rec)
            try:
                ret = drive(rec, ns, rec["in"])
                spec = norm(ret)
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__, "msg": str(e)}
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("msg", "")
                if any(f"'{nm}'" in msg for nm in names):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError binding bug: {msg}")
            for ev in trace:
                if ev["ev"] not in (rec["in"].get("io") or {}):
                    fail_closed.setdefault(ev["ev"], []).append(i)
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    absent = set(PORTED) - seen_funcs
    if absent:
        bad += 1
        print(f"SELFTEST: funcs with no fixture: {sorted(absent)}")
    if fail_closed:
        print(f"SELFTEST: seams fired with no script (absent -> None): "
              f"{ {k: len(v) for k, v in sorted(fail_closed.items())} }")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "transcription_flow.jsonl"
        with open(fx, encoding="utf-8") as fh:
            recs = [json.loads(l) for l in fh.read().splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()
