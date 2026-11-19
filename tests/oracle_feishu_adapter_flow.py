#!/usr/bin/env python3
"""Oracle for the Feishu adapter message-normalization FLOW port
(src/gateway/platforms/feishu_adapter_flow.c).

Drives the LIVE plugins/platforms/feishu/adapter.py normalization tree with
SCRIPTED seams: every SEAMED sibling call (and the foreign strip_markdown
import) is a recording stub fed the same scripts the C harness feeds its
universal seam fa_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
mutated accumulator state ("state").

The live defs are AST-extracted from adapter.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; the five frozen dataclasses
exec'd with dataclass/field seeded; logger re-set silent AFTER seeding). Every
exec namespace is a registered sys.modules module (py3.14 dataclasses). The
heavy quarry import chain (aiohttp / lark_oapi / gateway.*) is never imported;
strip_markdown is stubbed on sys.modules["gateway.platforms.helpers"].

Mode table (mirrors the C port): REAL functions run inline (normalize, parse,
the renderer trio, _build_markdown_post_payload, _coerce_required_int,
_strip_markdown_to_plain_text, _walk_nodes, matches and the pure leaves);
SEAMED functions (regex-extracted from the C fa_call sites) are scripted
everywhere they are called, and run real only when they are the case target.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec
bare. Summary: 'ORACLE feishu_adapter_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Dataclasses project via dataclasses.asdict; frozenset -> sorted list;
  tuple -> list; the _walk_nodes generator -> list.
- Attribute doubles (sender/mention) are SimpleNamespace built from JSON
  objects; bot is a real _FeishuBotIdentity; mentions_map is a dict of real
  FeishuMentionRef; media_refs accumulators are lists of FeishuPostMediaRef.
- Integral floats == ints (canon folds). dict comparison is order-insensitive;
  trace event LIST order and accumulator list order are semantic.
- Fixture corpus restrictions are documented in the C port header.
"""
import ast
import copy
import dataclasses
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "feishu", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

TARGETS = [
    "_is_bot_sender", "_sender_identity", "_escape_markdown_text", "_to_boolean",
    "_is_style_enabled", "_wrap_inline_code", "_sanitize_fence_language", "_coerce_int",
    "_to_post_payload", "_load_feishu_payload", "_normalize_feishu_text", "_unique_lines",
    "_extract_mention_ids", "_build_mention_hint", "_strip_edge_self_mentions",
    "_build_markdown_post_rows", "matches", "_walk_nodes", "_render_text_element",
    "_render_code_block_element", "_render_post_element", "_join_nested_posts",
    "_render_nested_post", "parse_feishu_post_payload", "_resolve_post_payload",
    "_resolve_locale_payload", "_build_markdown_post_payload", "_coerce_required_int",
    "_strip_markdown_to_plain_text", "_attachment_placeholder", "_first_non_empty_text",
    "_find_header_title", "_find_first_text", "_first_text_field",
    "_build_media_ref_from_payload", "_collect_text_segments", "_collect_card_lines",
    "_collect_action_labels", "_collect_forward_entries", "_normalize_merge_forward_message",
    "_normalize_share_chat_message", "_normalize_interactive_message", "_build_mentions_map",
    "normalize_feishu_message",
]
DATACLASSES = ["FeishuPostMediaRef", "FeishuMentionRef", "_FeishuBotIdentity",
               "FeishuPostParseResult", "FeishuNormalizedMessage"]


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


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    mod = new_module("fa_oracle_adapter")
    ns = mod.__dict__
    import collections
    import itertools
    import logging
    import threading
    from dataclasses import dataclass, field
    from typing import Any, Dict, List, Literal, Optional, Sequence
    ns.update({"re": re, "json": json, "itertools": itertools, "collections": collections,
               "threading": threading, "logging": logging, "os": os,
               "dataclass": dataclass, "field": field, "Any": Any, "Dict": Dict,
               "List": List, "Literal": Literal, "Optional": Optional, "Sequence": Sequence,
               "OrderedDict": collections.OrderedDict})
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # constants (plain-Name Assign / AnnAssign targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # the five frozen dataclasses (real classes for isinstance / construction)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in DATACLASSES:
            exec_node(src, node, ns, SRC)
    # module-level functions
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, SRC)
    # matches is a method of _FeishuBotIdentity (bind the plain function for audit)
    ns["matches"] = ns["_FeishuBotIdentity"].matches
    ns["logger"] = _SilentLogger()
    required = TARGETS + DATACLASSES + [
        "_MARKDOWN_LINK_RE", "_MARKDOWN_FENCE_OPEN_RE", "_MARKDOWN_FENCE_CLOSE_RE",
        "_MULTISPACE_RE", "_MARKDOWN_SPECIAL_CHARS_RE", "_MENTION_PLACEHOLDER_RE",
        "_MENTION_BOUNDARY_CHARS", "_TRAILING_TERMINAL_PUNCT", "_WHITESPACE_RE",
        "_SUPPORTED_CARD_TEXT_KEYS", "_RICH_BLOCK_TAGS", "_SKIP_TEXT_KEYS",
        "_STATIC_POST_TAGS", "_TEXT_STYLE_WRAPPERS", "_PREFERRED_LOCALES",
        "FALLBACK_POST_TEXT", "FALLBACK_FORWARD_TEXT", "FALLBACK_SHARE_CHAT_TEXT",
        "FALLBACK_INTERACTIVE_TEXT", "FALLBACK_IMAGE_TEXT", "FALLBACK_ATTACHMENT_TEXT",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# ── foreign strip_markdown stub package (in-function import target) ──
def install_helpers_module():
    for name in ("gateway", "gateway.platforms", "gateway.platforms.helpers"):
        if name not in sys.modules:
            new_module(name)
    sys.modules["gateway"].platforms = sys.modules["gateway.platforms"]
    sys.modules["gateway.platforms"].helpers = sys.modules["gateway.platforms.helpers"]


install_helpers_module()


# ── SEAMED names: regex-extracted from the C port's fa_call sites ──
def seam_names_from_c():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow.c",
                encoding="utf-8").read()
    return set(re.findall(r'fa_call\(\s*ctx,\s*"([^"]+)"', csrc))


SEAMED = seam_names_from_c()
REAL_DEFS = {nm: NS[nm] for nm in SEAMED if nm in NS}


# ── canonical projection ──
def canon(v):
    def conv(x):
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return conv(dataclasses.asdict(x))
        if isinstance(x, frozenset):
            return sorted(conv(i) for i in x)
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "Exception": Exception, "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": type("CancelledError", (Exception,), {}),
    "StopIteration": StopIteration, "JSONDecodeError": json.JSONDecodeError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── attribute / dataclass projections for seam args ──
def proj_attr(o):
    if isinstance(o, types.SimpleNamespace):
        return {k: proj_attr(v) for k, v in vars(o).items()}
    return o


def proj_map(mm):
    if mm is None:
        return None
    return {k: dataclasses.asdict(v) for k, v in mm.items()}


def proj_bot(bot):
    return dataclasses.asdict(bot)


def proj_mentions(ms):
    if ms is None:
        return None
    return [proj_attr(m) for m in ms]


ARGMAP = {
    "_to_boolean": lambda value: {"value": value},
    "_is_style_enabled": lambda style, key: {"style": style, "key": key},
    "_wrap_inline_code": lambda text: {"text": text},
    "_sanitize_fence_language": lambda language: {"language": language},
    "_render_text_element": lambda element: {"element": element},
    "_render_code_block_element": lambda element: {"element": element},
    "_escape_markdown_text": lambda text: {"text": text},
    "_normalize_feishu_text": lambda text, mentions_map=None: {"text": text, "mentions_map": proj_map(mentions_map)},
    "_resolve_post_payload": lambda payload: {"payload": payload},
    "_to_post_payload": lambda candidate: {"candidate": candidate},
    "_resolve_locale_payload": lambda payload: {"payload": payload},
    "_build_markdown_post_rows": lambda content: {"content": content},
    "_coerce_int": lambda value, default=None, min_value=0: {"value": value, "default": default, "min_value": min_value},
    "strip_markdown": lambda text: {"text": text},
    "_first_non_empty_text": lambda *values: {"values": list(values)},
    "_find_first_text": lambda payload, *, keys: {"payload": payload, "keys": list(keys)},
    "_first_text_field": lambda payload, *keys, deep=(): {"payload": payload, "keys": list(keys), "deep": list(deep)},
    "_collect_text_segments": lambda value, *, in_rich_block: {"value": value, "in_rich_block": in_rich_block},
    "_unique_lines": lambda lines: {"lines": lines},
    "_collect_forward_entries": lambda payload: {"payload": payload},
    "_find_header_title": lambda payload: {"payload": payload},
    "_collect_action_labels": lambda payload: {"payload": payload},
    "_collect_card_lines": lambda payload: {"payload": payload},
    "_build_media_ref_from_payload": lambda payload, *, resource_type: {"payload": payload, "resource_type": resource_type},
    "_attachment_placeholder": lambda file_name: {"file_name": file_name},
    "_extract_mention_ids": lambda mention: {"mention": proj_attr(mention)},
    "_load_feishu_payload": lambda raw_content: {"raw_content": raw_content},
    "_build_mentions_map": lambda mentions, bot: {"mentions": proj_mentions(mentions), "bot": proj_bot(bot)},
    "_normalize_merge_forward_message": lambda payload: {"payload": payload},
    "_normalize_share_chat_message": lambda payload: {"payload": payload},
    "_normalize_interactive_message": lambda message_type, payload: {"message_type": message_type, "payload": payload},
}
BUILD = {
    "_build_mentions_map": lambda r: {} if r is None else {k: NS["FeishuMentionRef"](**v) for k, v in r.items()},
    "_build_media_ref_from_payload": lambda r: NS["FeishuPostMediaRef"](**r),
    "_extract_mention_ids": lambda r: tuple(r),
}


# ── per-case script backend (mirrors the C fa_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _raise(self, name, args, spec, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": spec})
        self.scripted.append((exc, spec))
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            msg = f"no script for {name}"
            self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                msg = f"script exhausted for {name}"
                self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                rs = used["raise"]
                spec = {"__raise__": rs.get("type", "RuntimeError"), "message": rs.get("message", "")}
                self._raise(name, args, spec, make_exc(rs))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]
    build = BUILD.get(nm)

    def f(*a, **k):
        raw = ctx.call(nm, argfn(*a, **k))
        return build(raw) if build else raw
    return f


def install_recorders(ctx, target):
    for nm in SEAMED:
        if nm in REAL_DEFS:
            NS[nm] = REAL_DEFS[nm]
    for nm in SEAMED:
        if nm == target:
            continue
        if nm == "strip_markdown":
            sys.modules["gateway.platforms.helpers"].strip_markdown = make_recorder(ctx, nm)
        elif nm in NS:
            NS[nm] = make_recorder(ctx, nm)


# ── argument materialization ──
def mat_attr(d):
    if isinstance(d, dict):
        return types.SimpleNamespace(**{k: mat_attr(v) for k, v in d.items()})
    if isinstance(d, list):
        return [mat_attr(i) for i in d]
    return d


def mat_mention(d):
    return mat_attr(d)


def mat_sender(d):
    return mat_attr(d)


def mat_bot(d):
    d = d or {}
    return NS["_FeishuBotIdentity"](open_id=d.get("open_id", ""), user_id=d.get("user_id", ""),
                                    name=d.get("name", ""))


def mat_map(d):
    if d is None:
        return None
    return {k: NS["FeishuMentionRef"](**v) for k, v in d.items()}


def mat_media_list(l):
    return [NS["FeishuPostMediaRef"](**d) for d in (l or [])]


def mat_mentions(l):
    if l is None:
        return None
    return [mat_mention(m) for m in l]


def mat_refs(l):
    return [NS["FeishuMentionRef"](**d) for d in (l or [])]


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


# ── per-case live call (materialize + dispatch; recorders installed by caller) ──
def live_call(func, args, state_out):
    f = NS[func]
    if func == "_is_bot_sender":
        ret = f(mat_sender(args["sender"]))
    elif func == "_sender_identity":
        ret = f(mat_sender(args["sender"]))
    elif func == "_escape_markdown_text":
        ret = f(args["text"])
    elif func == "_to_boolean":
        ret = f(args["value"])
    elif func == "_is_style_enabled":
        ret = f(args["style"], args["key"])
    elif func == "_wrap_inline_code":
        ret = f(args["text"])
    elif func == "_sanitize_fence_language":
        ret = f(args["language"])
    elif func == "_coerce_int":
        ret = f(args["value"], default=args.get("default"), min_value=args.get("min_value", 0))
    elif func == "_to_post_payload":
        ret = f(args["candidate"])
    elif func == "_load_feishu_payload":
        ret = f(args["raw_content"])
    elif func == "_normalize_feishu_text":
        ret = f(args["text"], mat_map(args.get("mentions_map")))
    elif func == "_unique_lines":
        ret = f(args["lines"])
    elif func == "_extract_mention_ids":
        ret = f(mat_mention(args["mention"]))
    elif func == "_build_mention_hint":
        ret = f(mat_refs(args["mentions"]))
    elif func == "_strip_edge_self_mentions":
        ret = f(args["text"], mat_refs(args["mentions"]))
    elif func == "_build_markdown_post_rows":
        ret = f(args["content"])
    elif func == "matches":
        ret = mat_bot(args["bot"]).matches(open_id=args["open_id"], user_id=args["user_id"], name=args["name"])
    elif func == "_walk_nodes":
        ret = list(f(args["value"]))
    elif func == "_render_text_element":
        ret = f(args["element"])
    elif func == "_render_code_block_element":
        ret = f(args["element"])
    elif func in ("_render_post_element", "_join_nested_posts", "_render_nested_post"):
        ik = list(args.get("image_keys") or [])
        mr = mat_media_list(args.get("media_refs"))
        mm = mat_map(args.get("mentions_map"))
        key = {"_render_post_element": "element", "_join_nested_posts": "values",
               "_render_nested_post": "value"}[func]
        state_out.update({"image_keys": ik, "media_refs": mr, "mentions_map": mm})
        ret = f(args[key], ik, mr, mm)
    elif func == "parse_feishu_post_payload":
        mm = mat_map(args.get("mentions_map"))
        state_out.update({"mentions_map": mm})
        ret = f(args["payload"], mentions_map=mm)
    elif func == "_resolve_post_payload":
        ret = f(args["payload"])
    elif func == "_resolve_locale_payload":
        ret = f(args["payload"])
    elif func == "_build_markdown_post_payload":
        ret = f(args["content"])
    elif func == "_coerce_required_int":
        ret = f(args["value"], args["default"], min_value=args.get("min_value", 0))
    elif func == "_strip_markdown_to_plain_text":
        ret = f(args["text"])
    elif func == "_attachment_placeholder":
        ret = f(args["file_name"])
    elif func == "_first_non_empty_text":
        ret = f(*args["values"])
    elif func == "_find_header_title":
        ret = f(args["payload"])
    elif func == "_find_first_text":
        ret = f(args["payload"], keys=tuple(args["keys"]))
    elif func == "_first_text_field":
        ret = f(args["payload"], *args["keys"], deep=tuple(args.get("deep", [])))
    elif func == "_build_media_ref_from_payload":
        ret = f(args["payload"], resource_type=args["resource_type"])
    elif func == "_collect_text_segments":
        ret = f(args["value"], in_rich_block=args["in_rich_block"])
    elif func == "_collect_card_lines":
        ret = f(args["payload"])
    elif func == "_collect_action_labels":
        ret = f(args["payload"])
    elif func == "_collect_forward_entries":
        ret = f(args["payload"])
    elif func == "_normalize_merge_forward_message":
        ret = f(args["payload"])
    elif func == "_normalize_share_chat_message":
        ret = f(args["payload"])
    elif func == "_normalize_interactive_message":
        ret = f(args["message_type"], args["payload"])
    elif func == "_build_mentions_map":
        ret = f(mat_mentions(args.get("mentions")), mat_bot(args["bot"]))
    elif func == "normalize_feishu_message":
        ret = f(message_type=args["message_type"], raw_content=args["raw_content"],
                mentions=mat_mentions(args.get("mentions")), bot=mat_bot(args.get("bot")))
    else:
        raise RuntimeError(f"unknown func {func}")
    return ret


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func)
    state_out = {}
    try:
        ret = live_call(func, args, state_out)
    except BaseException as exc:  # SystemExit included
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_feishu_adapter_flow: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra_argmap = sorted(set(ARGMAP) - SEAMED)
    if extra_argmap:
        print(f"FATAL oracle_feishu_adapter_flow: ARGMAP seams absent from C: {extra_argmap}")
        sys.exit(2)
    missing_def = sorted(nm for nm in SEAMED if nm != "strip_markdown" and nm not in NS)
    if missing_def:
        print(f"FATAL oracle_feishu_adapter_flow: seams without live def: {missing_def}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_feishu_adapter_flow: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE feishu_adapter_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow.jsonl"
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
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    print(f"SELFTEST feishu_adapter_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()