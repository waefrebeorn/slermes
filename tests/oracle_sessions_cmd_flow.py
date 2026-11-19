#!/usr/bin/env python3
"""Oracle for the hermes_cli/sessions_cmd.py session-management FLOW port
(src/cli/port_sessions_cmd_flow.c).

Drives the LIVE sessions_cmd.py defs with SCRIPTED seams: every store/file/
stdin/print dependency is a recording stub fed the same scripts the C harness
feeds scf_call. Compares the DECISION SEQUENCE (ordered "trace" of {ev,args,ret}
seam events), the canonical return ("ret") and the post-call vars(args) window
("state", _export_flat only). PoP-claimed siblings (_confirm_prompt,
_render_jsonl, get_hermes_home) and foreign dependencies ride seams; the
internal unclaimed steps (_sessions_dir, _media_root) and every ported flow
stay REAL via AST extraction so nested decision logic is exercised, matching
the C port's internal transcriptions.

Live defs are AST-extracted from the quarry (annotations kept as strings via
the __future__ flag); the module-level Assign pass seeds _FILTER_ARGS and
_FLAT_EXPORTERS (the handler-dispatch maps reference unextracted defs and are
skipped). PruneCriteria is the REAL frozen/slots dataclass AST-extracted from
persistence/housekeeping/maintenance.py (25 fields, defaults filled by norm).
SessionWriteGuarded / AuthorityRefusal are str-faithful doubles of the
persistence.core.errors classes (the quarry import chain is never imported):
SessionWriteGuarded(session_id, message=None) str() is the message or the
default lease text; AuthorityRefusal(reason) str() is the reason. The heavy
quarry import chain is never imported. Fail-closed: 0 cases => exit 1; any
harness/oracle exception per case rides as {"__raise__": cls} (BaseException,
so SystemExit/KeyboardInterrupt propagate). Summary:
'ORACLE sessions_cmd_flow: N cases, M mismatches'.

Representation contract (both sides, see include/port_sessions_cmd_flow.h):
- tuple/set/frozenset -> array (sets sorted); PruneCriteria -> full 25-field
  dict; record/candidate doubles -> attribute dicts; None -> null; uncaught
  exception -> {"__raise__": cls}.
- scripts: absent key -> None; {"$seq": [...]} consumed per call (exhaustion
  raises RuntimeError("script exhausted: <name>")); {"__raise__": cls, ...}
  raises (trace ret is the WHOLE marker); any other value reused. None-valued
  arg keys dropped on both sides.
- Paths are POSIX strings: os.path == posixpath (os.sep "/"), FakePath doubles
  whose str() is the posix string (str(Path) flips slashes on Windows).
- Integral floats normalize to ints on both sides; f-string number operands
  are integral in fixtures; str.strip() operands are ASCII strings/null.
- _export_flat's args double is a SimpleNamespace; its post-call vars window
  is out.state (partial mutations stay visible when the call raises).
"""
import __future__
import ast
import builtins
import json
import logging
import posixpath
import sys
import types
from dataclasses import dataclass, fields
from pathlib import Path
from typing import Optional

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:  # already-read streams (in-process self-test) keep their encoding
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "sessions_cmd.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "sessions_cmd.py")
MAINT_SRC = str(QUARRY / "persistence" / "housekeeping" / "maintenance.py")

NAME = "sessions_cmd_flow"

PORTED = [
    "_any_filter_args", "_prune_args", "_confirm_or_exit_code", "_not_found",
    "_print_empty_store", "_delete", "_cmd_delete", "_cmd_rename", "_cmd_pin",
    "_print_dry_run_preview", "_note_pinned_skipped", "_output_file_in_dir",
    "_write_output", "_render_only", "_render_html", "_export_flat",
    "_print_recovery_verdict", "_repair_prompts_pin_names",
]
INTERNAL = ["_sessions_dir", "_media_root"]
WANTED = set(PORTED) | set(INTERNAL)
# Module Assigns the ported flows read; the handler-dispatch maps reference
# unextracted defs and are deliberately not seeded.
NEED_ASSIGNS = {"_FILTER_ARGS", "_FLAT_EXPORTERS"}

logging.disable(logging.CRITICAL)

# ── AST extraction ──────────────────────────────────────────────────────────

_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
ASSIGN_NODES = []
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tgts = _node.targets if isinstance(_node, ast.Assign) else [_node.target]
        if any(isinstance(t, ast.Name) and t.id in NEED_ASSIGNS for t in tgts):
            ASSIGN_NODES.append(_node)
_missing = WANTED - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)
if {t.id for n in ASSIGN_NODES for t in (n.targets if isinstance(n, ast.Assign) else [n.target])
        if isinstance(t, ast.Name)} != NEED_ASSIGNS:
    print(f"ORACLE {NAME}: FATAL missing module constants {sorted(NEED_ASSIGNS)}", file=sys.stderr)
    sys.exit(1)

# PruneCriteria: the REAL frozen/slots dataclass (decorator included).
_PC_TEXT = Path(MAINT_SRC).read_text(encoding="utf-8")
_PC_CODE = None
for _node in ast.parse(_PC_TEXT).body:
    if isinstance(_node, ast.ClassDef) and _node.name == "PruneCriteria":
        _start = min([_node.lineno] + [d.lineno for d in _node.decorator_list])
        _seg = "\n".join(_PC_TEXT.splitlines()[_start - 1:_node.end_lineno])
        _PC_CODE = compile(_seg, MAINT_SRC, "exec", flags=_FLAGS, dont_inherit=True)
if _PC_CODE is None:
    print(f"ORACLE {NAME}: FATAL missing PruneCriteria in {MAINT_SRC}", file=sys.stderr)
    sys.exit(1)


class SessionWriteGuarded(Exception):
    """str-faithful double of persistence.core.errors.SessionWriteGuarded."""
    def __init__(self, session_id, message=None):
        self.session_id = session_id
        text = (message if message is not None else
                f"session '{session_id}' (or a delegate child) has an active turn lease or compression lock")
        super().__init__(text)


class AuthorityRefusal(Exception):
    """str-faithful double of persistence.core.errors.AuthorityRefusal."""
    def __init__(self, reason, *, field=None, detail=None):
        self.reason = reason
        self.field = field
        self.detail = detail
        super().__init__(reason)


class FakePath:
    """Path double: str() is the POSIX string (as_posix normalization)."""
    def __init__(self, s):
        self._s = _posix_norm(str(s))

    def __truediv__(self, other):
        return FakePath(self._s + "/" + str(other))

    def __str__(self):
        return self._s

    def __fspath__(self):
        return self._s

    def as_posix(self):
        return self._s

    def __repr__(self):
        return f"FakePath({self._s!r})"


def _posix_norm(s):
    """PurePosixPath normalization for the fixture domain (keeps '..')."""
    if not s:
        return "."
    absolute = s.startswith("/")
    parts = [p for p in s.split("/") if p and p != "."]
    out = "/".join(parts)
    if absolute:
        return "/" + out
    return out or "."


_BASE_NS = {
    "__name__": "scf_live_sessions_cmd",
    "__builtins__": builtins,
    "json": json,
    "posixpath": posixpath,
    "Optional": Optional,
    "dataclass": dataclass,
    "Path": FakePath,
    "SessionWriteGuarded": SessionWriteGuarded,
    "AuthorityRefusal": AuthorityRefusal,
    "PersistenceError": type("PersistenceError", (Exception,), {}),
    "PermanentStoreError": type("PermanentStoreError", (Exception,), {}),
}
# py3.14 dataclass/annotation resolution wants a registered module BEFORE the
# ClassDef/def execs — run everything in the module's own dict.
_MOD = types.ModuleType("scf_live_sessions_cmd")
_MOD.__dict__.update(_BASE_NS)
sys.modules["scf_live_sessions_cmd"] = _MOD
_BASE_NS = _MOD.__dict__
exec(_PC_CODE, _BASE_NS)
PruneCriteria = _BASE_NS["PruneCriteria"]
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
# _FLAT_EXPORTERS binds _render_jsonl at exec time; a placeholder keeps the
# base seed alive — build_ns re-execs the Assign AFTER installing the case's
# recorder so the live table dispatches to the seam exactly like C does.
_BASE_NS["_render_jsonl"] = lambda *a, **k: (_ for _ in ()).throw(
    AssertionError("unscripted _render_jsonl"))
for _node in ASSIGN_NODES:
    exec(_compile_node(_node, SRC), _BASE_NS)
FLAT_EXPORTERS_CODE = [_compile_node(n, SRC) for n in ASSIGN_NODES
                       if any(isinstance(t, ast.Name) and t.id == "_FLAT_EXPORTERS"
                              for t in (n.targets if isinstance(n, ast.Assign) else [n.target]))][0]

# ── canonical representation ────────────────────────────────────────────────


def norm(v):
    if isinstance(v, PruneCriteria):
        return {f.name: norm(getattr(v, f.name)) for f in fields(v)}
    if isinstance(v, FakePath):
        return v.as_posix()
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items()}
    if isinstance(v, Path):
        return v.as_posix()
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    if cls_name == "SessionWriteGuarded":
        return SessionWriteGuarded(marker.get("session_id", ""), marker.get("msg"))
    if cls_name == "AuthorityRefusal":
        return AuthorityRefusal(marker.get("reason", ""))
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams ──────────────────────────────────────────────────────────

ARGMAP = {
    "print": lambda a, k: {"text": a[0] if a else "", "end": k.get("end", "\n"),
                           "flush": True if k.get("flush") else None},
    "confirm_prompt": lambda a, k: {"prompt": a[0]},
    "get_hermes_home": lambda a, k: {},
    "native_inputs_root": lambda a, k: {},
    "sessions_delete": lambda a, k: {"session_id": a[0], **k},
    "after_sessions_removed": lambda a, k: {
        "profile_dir": norm(a[1]), "removed": norm(a[2]),
        "reason": k.get("reason"), "primary": k.get("primary"),
        "sessions_dir": norm(k.get("sessions_dir"))},
    "resolve_id": lambda a, k: {"session_id": a[0]},
    "session_get": lambda a, k: {"session_id": a[0]},
    "set_title": lambda a, k: {"session_id": a[0], "title": a[1]},
    "set_pinned": lambda a, k: {"session_id": a[0], "pinned": a[1]},
    "get_title": lambda a, k: {"session_id": a[0]},
    "describe_filters": lambda a, k: {"filters": a[0]},
    "print_truncated": lambda a, k: {"more": a[0] if a else k.get("more"),
                                     "hint": k.get("hint")},
    "count_prune_matches": lambda a, k: {
        **{kk: vv for kk, vv in k.items() if kk not in ("criteria", "pinned_only")},
        "criteria": norm(k["criteria"]), "pinned_only": k.get("pinned_only")},
    "isdir": lambda a, k: {"path": a[0]},
    "makedirs": lambda a, k: {"path": a[0], "exist_ok": k.get("exist_ok")},
    "stdout_write": lambda a, k: {"text": a[0]},
    "file_write": lambda a, k: {"path": a[0], "text": a[1]},
    "render_sessions_export": lambda a, k: {"sessions": a[0], "fmt": k.get("fmt"),
                                            "only": k.get("only")},
    "export_record_count": lambda a, k: {"sessions": a[0], "only": k.get("only")},
    "generate_html_export": lambda a, k: {"session": a[0]},
    "generate_multi_session_html_export": lambda a, k: {"sessions": a[0]},
    "default_save_filename": lambda a, k: {"session_id": a[0], "fmt": a[1]},
    "render_jsonl": lambda a, k: {"args": {kk: norm(vv) for kk, vv in vars(a[0]).items()},
                                  "sessions": a[1]},
    "collect": lambda a, k: {},
}


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


def convert(name, value):
    if name == "get_hermes_home":
        return FakePath(value) if isinstance(value, str) else value
    if name == "session_get":
        return types.SimpleNamespace(**value) if isinstance(value, dict) else value
    if name in ("export_record_count", "render_jsonl"):
        return tuple(value) if isinstance(value, list) else value
    return value


def recorder(name, scripts):
    def rec(*a, **kw):
        args = {k: v for k, v in ARGMAP[name](a, kw).items() if v is not None}
        script = scripts.next(name)
        if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
            marker = dict(script)
            scripts.trace.append({"ev": name, "args": args, "ret": marker})
            raise make_exc(marker)
        value = convert(name, script)
        # get_hermes_home materializes a FakePath for the live join, but the
        # trace ret is the RAW script string (what the C seam records); the
        # FakePath normalization surfaces only in the sessions_dir join.
        trace_ret = script if name == "get_hermes_home" else norm(value)
        scripts.trace.append({"ev": name, "args": args, "ret": trace_ret})
        return value
    return rec


class _OsD:
    """os double: posixpath semantics (os.sep "/"), scripted isdir/makedirs."""
    def __init__(self, scripts):
        self.sep = "/"
        self.path = types.SimpleNamespace(
            isdir=recorder("isdir", scripts),
            join=posixpath.join,
            basename=posixpath.basename)
        self.makedirs = recorder("makedirs", scripts)


class _SysD:
    def __init__(self, scripts):
        self.stdout = types.SimpleNamespace(write=recorder("stdout_write", scripts))


class _FileD:
    """open() double: the with-block write is ONE file_write effect seam."""
    def __init__(self, path, scripts):
        self._path = path
        self._scripts = scripts

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def write(self, text):
        return recorder("file_write", self._scripts)(self._path, text)


def _fake_open(scripts):
    def op(path, mode="r", *a, **k):
        return _FileD(path, scripts)
    return op


def install_module_stubs(scripts):
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    hc = mod("hermes_cli")
    hc.__path__ = []
    se = mod("hermes_cli.session_export",
             default_save_filename=recorder("default_save_filename", scripts),
             render_sessions_export=recorder("render_sessions_export", scripts),
             export_record_count=recorder("export_record_count", scripts))
    seh = mod("hermes_cli.session_export_html",
              generate_html_export=recorder("generate_html_export", scripts),
              generate_multi_session_html_export=recorder("generate_multi_session_html_export", scripts))
    sf = mod("hermes_cli.session_filters",
             describe_filters=recorder("describe_filters", scripts))
    sr = mod("session_retention",
             after_sessions_removed=recorder("after_sessions_removed", scripts))
    gw = mod("gateway")
    gw.__path__ = []
    gim = mod("gateway.session_ingress_media",
              native_inputs_root=recorder("native_inputs_root", scripts))
    hc.session_export, hc.session_export_html, hc.session_filters = se, seh, sf
    gw.session_ingress_media = gim


# ── per-case namespace ──────────────────────────────────────────────────────


def build_ns(rec):
    inp = rec["in"]
    ns = dict(_BASE_NS)
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    # Re-exec the defs into the PER-CASE namespace so their __globals__ see
    # the case's recorder overrides (a shallow dict copy alone would leave
    # them bound to the shared module dict).
    for _nm, _code in DEF_CODE.items():
        exec(_code, ns)
    ns["_confirm_prompt"] = recorder("confirm_prompt", scripts)
    ns["_render_jsonl"] = recorder("render_jsonl", scripts)
    ns["get_hermes_home"] = recorder("get_hermes_home", scripts)
    ns["print"] = recorder("print", scripts)
    ns["print_truncated"] = recorder("print_truncated", scripts)
    ns["os"] = _OsD(scripts)
    ns["sys"] = _SysD(scripts)
    ns["open"] = _fake_open(scripts)
    install_module_stubs(scripts)
    # Re-bind the exporter table so its renderer slot is THIS case's
    # _render_jsonl recorder (C dispatches kind -> renderer at call time).
    exec(FLAT_EXPORTERS_CODE, ns)
    return ns, trace, scripts


def build_args(fields_dict):
    return types.SimpleNamespace(**(fields_dict or {}))


def build_store(env, scripts):
    return types.SimpleNamespace(
        sessions=types.SimpleNamespace(
            resolve_id=recorder("resolve_id", scripts),
            get=recorder("session_get", scripts),
            set_title=recorder("set_title", scripts),
            set_pinned=recorder("set_pinned", scripts),
            get_title=recorder("get_title", scripts),
            delete=recorder("sessions_delete", scripts)),
        maintenance=types.SimpleNamespace(
            count_prune_matches=recorder("count_prune_matches", scripts)),
        scope=types.SimpleNamespace(profile_dir=env.get("profile_dir")))


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    env = inp.get("env") or {}
    if func == "_any_filter_args":
        return ns[func](build_args(inp.get("args")))
    if func == "_prune_args":
        return ns[func](inp.get("filters"))
    if func == "_confirm_or_exit_code":
        return ns[func](inp.get("prompt", ""),
                        declined=inp.get("declined", "Cancelled."),
                        unattended=inp.get("unattended", "--yes"))
    if func == "_not_found":
        return ns[func](inp.get("session_id"))
    if func == "_print_empty_store":
        return ns[func](inp.get("action", ""), build_args(inp.get("args")))
    if func == "_delete":
        return ns[func](build_store(env, scripts), inp.get("session_id", ""),
                        **(inp.get("fences") or {}))
    if func == "_cmd_delete":
        return ns[func](build_args(inp.get("args")), persistence=build_store(env, scripts))
    if func == "_cmd_rename":
        return ns[func](build_args(inp.get("args")), persistence=build_store(env, scripts))
    if func == "_cmd_pin":
        return ns[func](build_args(inp.get("args")), inp.get("pinning", False),
                        persistence=build_store(env, scripts))
    if func == "_print_dry_run_preview":
        cands = [types.SimpleNamespace(**r) for r in (inp.get("candidates") or [])]
        return ns[func](cands, inp.get("filters"))
    if func == "_note_pinned_skipped":
        return ns[func](build_store(env, scripts), inp.get("filters"), inp.get("action", ""))
    if func == "_output_file_in_dir":
        return ns[func](inp.get("output"), inp.get("default_name", ""))
    if func == "_write_output":
        return ns[func](inp.get("output"), inp.get("text", ""), inp.get("summary", ""))
    if func == "_render_only":
        return ns[func](build_args(inp.get("args")), inp.get("sessions"))
    if func == "_render_html":
        return ns[func](build_args(inp.get("args")), inp.get("sessions"))
    if func == "_export_flat":
        args = build_args(inp.get("args"))
        holder["args"] = args  # partial mutations stay visible if the call raises
        ret = ns[func](inp.get("kind", ""), args, recorder("collect", scripts))
        holder["state"] = {k: norm(v) for k, v in vars(args).items()}
        return ret
    if func == "_print_recovery_verdict":
        return ns[func](inp.get("report"), inp.get("output"), inp.get("allow_partial", False))
    if func == "_repair_prompts_pin_names":
        return ns[func](types.SimpleNamespace(**(inp.get("record") or {})))
    raise KeyError(f"unknown func {func}")


def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                out = {"ret": norm(ret), "trace": trace, "state": holder.get("state", {})}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                state = {}
                if "args" in holder:
                    state = {k: norm(v) for k, v in vars(holder["args"]).items()}
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace, "state": state}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:800]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:800]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
