#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port
(src/gateway/platforms/slack_adapter_flow.c).

Drives the LIVE plugins/platforms/slack/adapter.py module-level helpers and
decision chains with SCRIPTED seams: every injected callable (t,
is_gateway_known_command, get_scoped_secret, resolve_proxy_url,
safe_proxy_url_for_log, is_host_excluded_by_no_proxy, get_hermes_home,
tokens_file.exists / read_text, warn_if_credential_file_broadly_readable) and
the logger are recording stubs fed the same scripts the C harness feeds its
universal seam. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
mutated state ("state", _trim_slack_dm_cache's dm_cache).

The live defs are AST-extracted from adapter.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; logger re-set to the recording
double AFTER seeding). The heavy quarry import chain is never imported; the
in-function imports (hermes_cli.commands, hermes_constants, utils) are
sys.modules stubs carrying the per-case recorders. Every exec namespace is a
registered sys.modules module (py3.14).

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE slack_adapter_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Plain JSON numbers are ints; JSON booleans are bools. Non-integral floats
  appear only as float() RESULTS (dedup TTL).
- Object doubles (non-dict attribute carriers) travel as {"__obj__": {attrs}};
  a plain JSON object IS a Python dict (_slack_response_payload).
- logger seams record {"msg": <format literal>, "args": [<raw args>]} — args
  are recorded VERBATIM (%r/%s apply only at emission).
- Path modeling is POSIX: get_hermes_home returns a POSIX path string; the
  HomeDouble/FileDouble project the same lexical POSIX join and __str__.
- read_text(encoding="utf-8-sig") strips ONE leading U+FEFF BOM AFTER the
  seam call (the trace ret carries the raw scripted text).
- Fixture corpus (ASCII scope): strip/lower fold ASCII whitespace only;
  int()/float() string parses accept ASCII digits/sign/underscore/exponent;
  str() of non-strings covers str/int/bool/None only; float() raw strings
  parse finite or nan (inf results excluded); malformed saved-token JSON is
  limited to "" and whitespace-only text; _disp_width uses the full Unicode
  EAW table on both sides.
- BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) PASS THROUGH every except-Exception site.
"""
import ast
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "slack", "adapter.py")

_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


# ── adapter.py namespace (live defs + seeded constants) ──
TARGETS = [
    "_slack_unfurl_kwargs", "_is_table_row", "_disp_width", "_pad",
    "_split_table_row", "_align_table", "_wrap_markdown_tables",
    "_collect_slack_block_mentions", "_slack_mention_detection_text",
    "_slack_permalink_path", "_str_or_empty", "_int_or_zero", "_first_truthy",
    "_slack_str_field", "_render_slack_inline_element", "_render_inline_elements",
    "_unescape_slack_entities", "_resolve_slack_audio_ext", "_is_slack_voice_clip",
    "_attachment_label", "_standalone_post_kwargs", "_slack_response_payload",
    "_model_picker_expired_notice", "_rewrite_known_bang_command",
    "_slack_dedup_ttl_seconds", "_resolve_slack_proxy_url",
    "_trim_slack_dm_cache", "_load_slack_bot_tokens",
]
CONSTS = [
    "_BOOL_WORDS", "_INLINE_ENTITY_FORMATS", "_SLACK_AUDIO_MIME_TO_EXT",
    "_SLACK_STT_SUPPORTED_EXTS", "_TABLE_SEPARATOR_RE", "_SLACK_HTML_ENTITY_RE",
    "_SLACK_HTML_ENTITIES", "_SLACK_PROXY_HOSTS", "_SLACK_DM_CACHE_MAX",
    "_slack_dm_cache", "_SLACK_TABLE_MAX_CHARS", "_SLACK_ERROR_BODY_LIMIT_BYTES",
]


def extract_adapter():
    mod = new_module("sa_oracle_adapter")
    ns = mod.__dict__
    import asyncio
    import contextvars
    import logging
    import time
    import unicodedata
    ns.update({"asyncio": asyncio, "contextvars": contextvars, "logging": logging,
               "os": os, "re": re, "json": json, "time": time,
               "unicodedata": unicodedata, "Path": Path, "_Path": Path,
               "Any": Any, "Optional": Optional, "__file__": A_SRC})
    src = open(A_SRC, encoding="utf-8").read()
    seed_constants(src, ns, A_SRC)
    for node in ast.parse(src).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, A_SRC)
    missing = [k for k in TARGETS + CONSTS if k not in ns]
    if missing:
        print(f"FATAL oracle_slack_adapter_flow: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_adapter()


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, bytes):
            return x.decode("latin-1")
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, Path):
            return x.as_posix()
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "OSError": OSError, "TimeoutError": TimeoutError, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C sa_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# ── attribute / path doubles ──
def pyval(v):
    if isinstance(v, dict) and v.get("__float__") is True:
        return float(v.get("v", 0))
    return v


def posix_join(home, name):
    h = home.rstrip("/")
    if not h:
        return "/" + name if home.startswith("/") else name
    return h + "/" + name


class FileDouble:
    def __init__(self, path, ctx):
        self.path = path
        self.ctx = ctx

    def exists(self):
        return self.ctx.call("tokens_file.exists", {"path": self.path})

    def read_text(self, encoding=None):
        raw = self.ctx.call("tokens_file.read_text",
                            {"path": self.path, "encoding": encoding})
        if isinstance(raw, str) and raw.startswith("\ufeff"):
            raw = raw[1:]
        return raw

    def __str__(self):
        return self.path


class HomeDouble:
    def __init__(self, path, ctx):
        self.path = path
        self.ctx = ctx

    def __truediv__(self, other):
        return FileDouble(posix_join(self.path, other), self.ctx)


class LoggerDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def _log(self, level, msg, *args, **kwargs):
        return self.ctx.call("logger." + level, {"msg": msg, "args": list(args)})

    def info(self, msg, *a, **k):
        return self._log("info", msg, *a, **k)

    def warning(self, msg, *a, **k):
        return self._log("warning", msg, *a, **k)

    def debug(self, msg, *a, **k):
        return self._log("debug", msg, *a, **k)

    def error(self, msg, *a, **k):
        return self._log("error", msg, *a, **k)


def materialize_resp(v):
    if isinstance(v, dict) and "__obj__" in v:
        attrs = v["__obj__"] or {}
        return types.SimpleNamespace(**{k: pyval(x) for k, x in attrs.items()})
    return v


# ── per-case stub installation ──
def install_stubs(ctx):
    NS["t"] = rec(ctx, "t", lambda key: {"key": key})
    NS["_get_scoped_secret"] = rec(
        ctx, "get_scoped_secret", lambda key, default="": {"key": key, "default": default})
    NS["resolve_proxy_url"] = rec(ctx, "resolve_proxy_url", lambda: {})
    NS["safe_proxy_url_for_log"] = rec(
        ctx, "safe_proxy_url_for_log", lambda url: {"url": url})
    NS["is_host_excluded_by_no_proxy"] = rec(
        ctx, "is_host_excluded_by_no_proxy", lambda host: {"host": host})
    NS["logger"] = LoggerDouble(ctx)
    # in-function import: hermes_cli.commands.is_gateway_known_command
    pkg = sys.modules.get("hermes_cli")
    if pkg is None:
        pkg = new_module("hermes_cli")
        pkg.__path__ = []
    cmds = new_module("hermes_cli.commands")
    cmds.is_gateway_known_command = rec(
        ctx, "is_gateway_known_command", lambda name: {"name": name})
    pkg.commands = cmds
    # in-function import: hermes_constants.get_hermes_home
    hc = new_module("hermes_constants")
    hc.get_hermes_home = rec(
        ctx, "get_hermes_home", lambda: {},
        build=lambda r: HomeDouble(r, ctx) if isinstance(r, str) else r)
    # in-function import: utils.warn_if_credential_file_broadly_readable
    ut = new_module("utils")
    ut.warn_if_credential_file_broadly_readable = rec(
        ctx, "warn_if_credential_file_broadly_readable",
        lambda path, label=None, log=None: {"path": str(path), "label": label})


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = copy.deepcopy(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    install_stubs(ctx)
    try:
        if func == "_slack_unfurl_kwargs":
            ret = NS[func](args.get("extra"))
        elif func == "_is_table_row":
            ret = NS[func](args["line"])
        elif func == "_disp_width":
            ret = NS[func](args["s"])
        elif func == "_pad":
            ret = NS[func](args["cell"], args["width"])
        elif func == "_split_table_row":
            ret = NS[func](args["line"])
        elif func == "_align_table":
            ret = NS[func](args["rows"])
        elif func == "_wrap_markdown_tables":
            ret = NS[func](args["text"])
        elif func == "_collect_slack_block_mentions":
            ret = NS[func](args.get("blocks"))
        elif func == "_slack_mention_detection_text":
            ret = NS[func](args["event"])
        elif func == "_slack_permalink_path":
            ret = NS[func](args.get("channel_id"), args.get("message_ts"))
        elif func == "_str_or_empty":
            ret = NS[func](pyval(args.get("value")))
        elif func == "_int_or_zero":
            ret = NS[func](pyval(args.get("value")))
        elif func == "_first_truthy":
            ret = NS[func](args["mapping"], tuple(args["keys"]))
        elif func == "_slack_str_field":
            ret = NS[func](args["el"], args["name"])
        elif func == "_render_slack_inline_element":
            ret = NS[func](args["el"])
        elif func == "_render_inline_elements":
            ret = NS[func](args["elements"])
        elif func == "_unescape_slack_entities":
            ret = NS[func](args.get("text"))
        elif func == "_resolve_slack_audio_ext":
            ret = NS[func](args["file_obj"], args.get("mimetype"))
        elif func == "_is_slack_voice_clip":
            ret = NS[func](args["file_obj"])
        elif func == "_attachment_label":
            ret = NS[func](args.get("file_obj"))
        elif func == "_standalone_post_kwargs":
            ret = NS[func](args["chat_id"], args["text"], args["unfurl_kwargs"],
                           args.get("thread_id"))
        elif func == "_slack_response_payload":
            ret = NS[func](materialize_resp(args.get("response")))
        elif func == "_model_picker_expired_notice":
            ret = NS[func]()
        elif func == "_rewrite_known_bang_command":
            ret = NS[func](args["text"])
        elif func == "_slack_dedup_ttl_seconds":
            ret = NS[func]()
        elif func == "_resolve_slack_proxy_url":
            ret = NS[func]()
        elif func == "_trim_slack_dm_cache":
            cache = state.get("dm_cache")
            if not isinstance(cache, dict):
                cache = {}
            state["dm_cache"] = cache
            NS["_slack_dm_cache"] = cache
            NS[func]()
            state["dm_cache"] = NS["_slack_dm_cache"]
            ret = None
        elif func == "_load_slack_bot_tokens":
            ret = NS[func](args["raw_token"], quiet=args["quiet"])
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "t", "is_gateway_known_command", "get_scoped_secret",
    "logger.warning", "logger.info",
    "resolve_proxy_url", "safe_proxy_url_for_log", "is_host_excluded_by_no_proxy",
    "get_hermes_home", "tokens_file.exists", "tokens_file.read_text",
    "warn_if_credential_file_broadly_readable",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "slack_adapter_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_slack_adapter_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    if '"logger." + level' in osrc:
        used |= {"logger.warning", "logger.info"}  # levels the live flows emit
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_slack_adapter_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_slack_adapter_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE slack_adapter_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow.jsonl"
    n = bad = 0
    counts = {}
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        expect = case.get("expect_raise")
        if expect:
            if not (isinstance(r, dict) and r.get("__raise__") == expect):
                bad += 1
                print(f"SELFTEST EXPECT {func} case {n}: wanted raise {expect}, got {r}")
        elif isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    missing = [f for f in TARGETS if f not in counts]
    if missing:
        bad += 1
        print(f"SELFTEST MISSING FUNCS: {missing}")
    thin = [f for f, c in counts.items() if c < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN FUNCS (<5 cases): {thin}")
    print(f"SELFTEST slack_adapter_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()