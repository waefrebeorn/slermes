#!/usr/bin/env python3
"""Oracle for the tools/skills_guard.py UNCLAIMED remainder
(src/tools/skills_guard_flow.c).

AST-extracts the 20 target defs (exec'ing the module's real constants, dataclasses
and pattern builders in source order so THREAT_PATTERNS / _COMPILED_THREAT_PATTERNS
are built byte-exact), then wraps every compiled threat / skill-only pattern and
every internal regex primitive in a recording double. The CLAIMED genuine siblings
(_content_digest, _determine_verdict) and every foreign primitive (re, tokenize,
codecs, bytes.decode, code_views, raise_provider_key_reads, mask_negated_forget,
the filesystem, the clock) ride the SAME universal seam the C harness feeds.
Ported targets run INLINE (real extracted defs) and compose with each other; pure
Path algebra (name / suffix / parent / relative_to / is_relative_to / as_posix) and
the _TRIPLE_QUOTE_OPEN_RE prefix test run REAL on both sides.

Two modes share one drive():
  play   — seams consume the fixture script (the oracle / gate path);
  record — computational seams call the REAL primitive and append the projected
           result to the script queue (the generator path). Filesystem / clock /
           claimed-sibling seams are ALWAYS scripted (the fixture defines them).

Input: stdin JSON lines from t_port_skills_guard_flow.
Summary: ORACLE skills_guard_flow: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import builtins
import contextlib
import copy
import dataclasses
import io
import json
import os
import sys
import types
from dataclasses import asdict
from pathlib import PurePosixPath
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("HOME", os.path.dirname(_REPO))
os.environ.setdefault("USERPROFILE", os.path.dirname(_REPO))
os.environ.setdefault("LOCALAPPDATA", os.path.join(os.path.dirname(_REPO), "AppData", "Local"))
os.environ.setdefault("HERMES_HOME", os.path.join(_REPO, ".testbin", "sg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tools", "skills_guard.py")
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

TARGETS = [
    "_line_at", "_statement_owners", "_demote_inert_path_reference",
    "_unicode_char_name", "_mask_markdown_link_destinations",
    "_mask_prose_link_destinations", "_padded_code_findings",
    "_compute_docstring_lines", "_decoded_views", "_scan_text",
    "_unscanned_text_padded_code", "scan_file", "_community_note",
    "_resolve_trust_level", "should_allow_install", "format_scan_report",
    "_build_summary", "_check_structure", "scan_skill", "content_hash",
]

# Computational seams (recordable from the real primitive). Everything else is
# always scripted (filesystem, clock, claimed siblings).
COMPUTATIONAL = {
    "re.search", "quoted_span.sub", "container.sub", "fence.match",
    "denylist.match", "action.search", "padded.search", "codeword.search",
    "tokenize.generate_tokens", "tokenize.detect_encoding", "codecs.lookup",
    "decode", "code_views", "raise_provider_key_reads", "mask_negated_forget",
}

MODE = "play"
CUR = {"func": None, "in": {}, "trace": [], "io": {}, "pos": {}, "raises": [], "ns": None}
REAL_PATTERNS = []          # real compiled threat + skill-only patterns, by idx
REAL_REGEX = {}             # real internal compiled regexes, by module name
REAL_FOREIGN = {}           # real code_views / raise_provider_key_reads / mask_negated_forget


def rec(ev):
    CUR["trace"].append(ev)


def exc_to_spec(e):
    spec = {"__raise__": type(e).__name__, "message": str(e)}
    if isinstance(e, UnicodeDecodeError):
        spec["object"] = list(e.object)
        spec["start"] = e.start
        spec["end"] = e.end
        spec["reason"] = e.reason
        spec["encoding"] = e.encoding
    return spec


def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    if kind == "KeyError":
        try:
            key = ast.literal_eval(msg)
        except Exception:
            key = msg
        return KeyError(key)
    if kind == "TokenError":
        import tokenize
        return tokenize.TokenError(msg)
    if kind == "UnicodeDecodeError":
        sub = type("UnicodeDecodeError", (builtins.UnicodeDecodeError,),
                   {"__str__": lambda self: msg})
        e = sub.__new__(sub)
        Exception.__init__(e, msg)
        e.object = bytes(spec.get("object", []))
        e.start = spec.get("start", 0)
        e.end = spec.get("end", e.start)
        e.reason = spec.get("reason", "")
        e.encoding = spec.get("encoding", "")
        return e
    if kind in ("UnicodeEncodeError", "UnicodeTranslateError"):
        real = getattr(builtins, kind)
        sub = type(kind, (real,), {"__str__": lambda self: msg})
        e = sub.__new__(sub)
        Exception.__init__(e, msg)
        return e
    cls = getattr(builtins, kind, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(kind, (Exception,), {})(msg)


class Raise:
    def __init__(self, spec, exc=None):
        self.spec = spec
        self.exc = exc if exc is not None else make_exc(spec)


def next_script(name):
    io_ = CUR["io"]
    if name not in io_:
        return Raise({"__raise__": "RuntimeError", "message": "no script for " + name})
    v = io_[name]
    if isinstance(v, list):
        i = CUR["pos"].get(name, 0)
        if i >= len(v):
            return Raise({"__raise__": "RuntimeError", "message": "script exhausted for " + name})
        CUR["pos"][name] = i + 1
        return v[i]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


# ── projection / materialization ─────────────────────────────────────────────
def proj(x):
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return {k: proj(v) for k, v in asdict(x).items()}
    if isinstance(x, PurePosixPath):
        return x.as_posix()
    if isinstance(x, BaseException):
        return str(x)
    if isinstance(x, (set, frozenset)):
        return sorted(proj(v) for v in x)
    if isinstance(x, dict):
        return {k: proj(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [proj(v) for v in x]
    if isinstance(x, (bytearray, bytes)):
        return list(x)
    return x


class MatchDouble:
    def __init__(self, d):
        self.d = d

    def start(self):
        return self.d["start"]

    def end(self):
        return self.d["end"]


class TokenDouble:
    def __init__(self, d):
        self.type = d["type"]
        self.string = d["string"]
        self.start = tuple(d["start"])
        self.end = tuple(d["end"])


def materialize(name, r):
    if name == "padded.search":
        return MatchDouble(r) if isinstance(r, dict) else r
    if name == "tokenize.generate_tokens":
        return [TokenDouble(t) for t in r] if isinstance(r, list) else r
    if name == "codecs.lookup":
        return SimpleNamespace(name=r["name"]) if isinstance(r, dict) else r
    if name == "code_views":
        if isinstance(r, list):
            return [r[0], [bytearray(b) for b in r[1]]]
        return r
    if name == "datetime.now":
        return SimpleNamespace(isoformat=lambda: r)
    return r


def seam(name, args):
    if MODE == "record" and name in COMPUTATIONAL:
        q = CUR["io"].setdefault(name, [])
        try:
            ret = REAL[name](args)
        except BaseException as e:
            spec = exc_to_spec(e)
            q.append(spec)
            rec({"ev": name, "args": args, "ret": spec})
            CUR["raises"].append(Raise(spec, exc=e))
            raise
        q.append(ret)
        rec({"ev": name, "args": args, "ret": ret})
        return materialize(name, ret)
    r = next_script(name)
    if isinstance(r, Raise):
        CUR["raises"].append(r)
        rec({"ev": name, "args": args, "ret": r.spec})
        raise r.exc
    rec({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = make_exc(r)
        CUR["raises"].append(Raise(r))
        raise e
    return materialize(name, copy.deepcopy(r) if isinstance(r, (dict, list)) else r)


# ── REAL computational primitives (record mode) ──────────────────────────────
def _real_re_search(args):
    pat = REAL_PATTERNS[args["idx"]]
    return bool(pat.search(args["line"]))


def _real_sub(key):
    def f(args):
        return REAL_REGEX[key].sub("", args["line"])
    return f


def _real_fence(args):
    m = REAL_REGEX["_FENCE_LINE"].match(args["line"])
    return {"marker": m["marker"], "info": m["info"]} if m else None


def _real_match_bool(key):
    def f(args):
        return bool(REAL_REGEX[key].match(args["line"]))
    return f


def _real_search_bool(key):
    def f(args):
        return bool(REAL_REGEX[key].search(args["line"]))
    return f


def _real_padded(args):
    m = REAL_REGEX["_PADDED_CODE"].search(args["line"])
    return {"start": m.start(), "end": m.end()} if m else None


def _real_gen_tokens(args):
    import tokenize
    toks = []
    for t in tokenize.generate_tokens(io.StringIO(args["text"]).readline):
        toks.append({"type": t.type, "string": t.string,
                     "start": list(t.start), "end": list(t.end)})
    return toks


def _real_detect_encoding(args):
    import tokenize
    codec, lines = tokenize.detect_encoding(io.BytesIO(bytes(args["raw"])).readline)
    return [codec, [list(l) for l in lines]]


def _real_codecs_lookup(args):
    import codecs
    return {"name": codecs.lookup(args["codec"]).name}


def _real_decode(args):
    return bytes(args["raw"]).decode(args["encoding"], args["errors"])


def _real_code_views(args):
    r = REAL_FOREIGN["code_views"](args["lines"])
    if r is None:
        return None
    view, in_text = r
    return [view, [list(b) for b in in_text]]


def _real_raise_provider_key_reads(args):
    ns = CUR["ns"]
    Finding = ns["Finding"]
    findings = [Finding(**d) for d in args["findings"]]
    in_text = [bytearray(b) for b in args["in_text"]] if args["in_text"] is not None else None
    REAL_FOREIGN["raise_provider_key_reads"](findings, args["lines"], in_text)
    return [asdict(f) for f in findings]


def _real_mask_negated_forget(args):
    return REAL_FOREIGN["mask_negated_forget"](args["text"])


REAL = {
    "re.search": _real_re_search,
    "quoted_span.sub": _real_sub("_QUOTED_SPAN_RE"),
    "container.sub": _real_sub("_CONTAINER_PREFIX"),
    "fence.match": _real_fence,
    "denylist.match": _real_match_bool("_DENYLIST_OWNER_RE"),
    "action.search": _real_search_bool("_ACTION_ON_LINE_RE"),
    "padded.search": _real_padded,
    "codeword.search": _real_search_bool("_CODE_WORD"),
    "tokenize.generate_tokens": _real_gen_tokens,
    "tokenize.detect_encoding": _real_detect_encoding,
    "codecs.lookup": _real_codecs_lookup,
    "decode": _real_decode,
    "code_views": _real_code_views,
    "raise_provider_key_reads": _real_raise_provider_key_reads,
    "mask_negated_forget": _real_mask_negated_forget,
}


# ── recording doubles ────────────────────────────────────────────────────────
class SeamFn:
    def __init__(self, name, argfn=None, mat=None):
        self.name = name
        self.argfn = argfn
        self.mat = mat

    def __call__(self, *a, **k):
        args = self.argfn(a, k) if self.argfn else {}
        r = seam(self.name, args)
        return self.mat(r) if self.mat else r


class PatternDouble:
    def __init__(self, idx, pid):
        self.idx = idx
        self.pid = pid

    def search(self, line):
        return seam("re.search", {"idx": self.idx, "pid": self.pid, "line": line})


class RegexDouble:
    def __init__(self, name, kind):
        self.name = name
        self.kind = kind

    def sub(self, repl, string):
        return seam(self.name, {"line": string})

    def match(self, string):
        return seam(self.name, {"line": string})

    def search(self, string):
        return seam(self.name, {"line": string})


class BytesDouble(bytes):
    def decode(self, encoding="utf-8", errors="strict"):
        return seam("decode", {"raw": list(self), "encoding": encoding, "errors": errors})


def _raise_provider_key_reads(findings, lines, in_text=None):
    args = {"findings": proj(findings), "lines": lines, "in_text": proj(in_text)}
    r = seam("raise_provider_key_reads", args)
    if isinstance(r, list):
        Finding = CUR["ns"]["Finding"]
        findings[:] = [Finding(**d) for d in r]
    return None


# ── Path filesystem monkeypatch (class-level, reads CUR dynamically) ─────────
def _patch_path():
    def bool_method(op):
        return lambda self: seam("path." + op, {"path": self.as_posix()})
    PurePosixPath.is_dir = bool_method("is_dir")
    PurePosixPath.is_file = bool_method("is_file")
    PurePosixPath.is_symlink = bool_method("is_symlink")
    PurePosixPath.exists = bool_method("exists")

    def _resolve(self):
        r = seam("path.resolve", {"path": self.as_posix()})
        return PurePosixPath(r) if isinstance(r, str) else r
    PurePosixPath.resolve = _resolve

    def _rglob(self, pattern):
        r = seam("path.rglob", {"path": self.as_posix(), "pattern": pattern})
        return [PurePosixPath(x) for x in r] if isinstance(r, list) else r
    PurePosixPath.rglob = _rglob

    def _stat(self):
        r = seam("path.stat", {"path": self.as_posix()})
        return SimpleNamespace(st_size=r["st_size"], st_mode=r["st_mode"]) if isinstance(r, dict) else r
    PurePosixPath.stat = _stat

    def _read_bytes(self):
        r = seam("path.read_bytes", {"path": self.as_posix()})
        return BytesDouble(bytes(r)) if isinstance(r, list) else r
    PurePosixPath.read_bytes = _read_bytes


# ── extraction ───────────────────────────────────────────────────────────────
def extract():
    import re as _re
    import tokenize as _tokenize
    from tools.skills_guard_credentials import (
        SKILL_ONLY_PATTERNS as _SKILL_ONLY, code_views as _code_views,
        raise_provider_key_reads as _rpk)
    from tools.threat_patterns import (
        FORGET_RULES_RAW_RE as _FORGET_RAW, mask_negated_forget as _mnf)
    REAL_FOREIGN.update(code_views=_code_views, raise_provider_key_reads=_rpk,
                        mask_negated_forget=_mnf)

    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("skills_guard_flow_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__

    tok_double = SimpleNamespace(**{k: getattr(_tokenize, k) for k in dir(_tokenize)
                                    if not k.startswith("__")})

    def _gen_tokens(readline):
        text = "".join(iter(readline, ""))
        return seam("tokenize.generate_tokens", {"text": text})

    def _detect_encoding(readline):
        raw = b"".join(iter(readline, b""))
        return seam("tokenize.detect_encoding", {"raw": list(raw)})
    tok_double.generate_tokens = _gen_tokens
    tok_double.detect_encoding = _detect_encoding

    ns.update({
        "re": _re, "io": io, "json": json, "tokenize": tok_double,
        "codecs": SimpleNamespace(lookup=SeamFn("codecs.lookup", lambda a, k: {"codec": a[0]})),
        "hashlib": SimpleNamespace(sha256=lambda b: SimpleNamespace(
            hexdigest=lambda: seam("sha256_hex", {"data": list(b)}))),
        "suppress": contextlib.suppress,
        "asdict": asdict, "dataclass": dataclasses.dataclass, "field": dataclasses.field,
        "datetime": SimpleNamespace(now=lambda tz=None: seam("datetime.now", {})),
        "timezone": SimpleNamespace(utc="UTC"), "UTC": "UTC",
        "Path": PurePosixPath, "List": list, "Tuple": tuple,
        "SKILL_ONLY_PATTERNS": _SKILL_ONLY,
        "code_views": SeamFn("code_views", lambda a, k: {"lines": a[0]}),
        "raise_provider_key_reads": _raise_provider_key_reads,
        "FORGET_RULES_RAW_RE": _FORGET_RAW,
        "mask_negated_forget": SeamFn("mask_negated_forget", lambda a, k: {"text": a[0]}),
    })

    tree = ast.parse(src)
    import __future__
    flags = __future__.annotations.compiler_flag
    # Exec dataclasses, constants, builders and target defs in SOURCE order so the
    # pattern table is built exactly as the live module builds it.
    for node in tree.body:
        if isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.Assign, ast.AnnAssign)):
            if isinstance(node, ast.AnnAssign) and not node.value:
                continue
            m2 = copy.copy(node)
            if isinstance(m2, ast.FunctionDef):
                m2.decorator_list = []
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[m2], type_ignores=[]), SRC, "exec", flags=flags), ns)
    for name in TARGETS:
        assert name in ns, "extraction missed %s" % name

    # Wrap the compiled threat + skill-only patterns in recording doubles.
    compiled = ns["_COMPILED_THREAT_PATTERNS"]
    REAL_PATTERNS.clear()
    wrapped = []
    for idx, entry in enumerate(compiled):
        pat, pid, sev, cat, desc = entry
        REAL_PATTERNS.append(pat)
        wrapped.append((PatternDouble(idx, pid), pid, sev, cat, desc))
    ns["_COMPILED_THREAT_PATTERNS"] = wrapped
    skill_wrapped = []
    base = len(compiled)
    for j, entry in enumerate(ns["SKILL_ONLY_PATTERNS"]):
        pat, pid, sev, cat, desc = entry
        REAL_PATTERNS.append(pat)
        skill_wrapped.append((PatternDouble(base + j, pid), pid, sev, cat, desc))
    ns["SKILL_ONLY_PATTERNS"] = skill_wrapped

    # Internal regex primitives -> recording doubles (keep the real ones for REAL).
    for key in ("_QUOTED_SPAN_RE", "_FENCE_LINE", "_CONTAINER_PREFIX",
                "_DENYLIST_OWNER_RE", "_ACTION_ON_LINE_RE", "_PADDED_CODE", "_CODE_WORD"):
        REAL_REGEX[key] = ns[key]
    ns["_QUOTED_SPAN_RE"] = RegexDouble("quoted_span.sub", "sub")
    ns["_CONTAINER_PREFIX"] = RegexDouble("container.sub", "sub")
    ns["_FENCE_LINE"] = RegexDouble("fence.match", "match")
    ns["_DENYLIST_OWNER_RE"] = RegexDouble("denylist.match", "match")
    ns["_ACTION_ON_LINE_RE"] = RegexDouble("action.search", "search")
    ns["_PADDED_CODE"] = RegexDouble("padded.search", "search")
    ns["_CODE_WORD"] = RegexDouble("codeword.search", "search")

    # Claimed genuine siblings -> scripted seams.
    ns["_content_digest"] = SeamFn("content_digest", lambda a, k: {"path": proj(a[0])})
    ns["_determine_verdict"] = SeamFn("determine_verdict", lambda a, k: {"findings": proj(a[0])})

    _patch_path()
    CUR["ns"] = ns
    return ns


# ── input materialization ────────────────────────────────────────────────────
def P(s):
    return PurePosixPath(s) if isinstance(s, str) else s


def mat_finding(ns, d):
    return ns["Finding"](**d)


def mat_result(ns, d):
    d = dict(d)
    d["findings"] = [mat_finding(ns, f) for f in d.get("findings", [])]
    return ns["ScanResult"](**d)


def setup_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["io"] = copy.deepcopy(inp.get("io") or {})
    CUR["pos"] = {}
    CUR["raises"] = []


def drive(ns, func, inp):
    g = inp.get
    if func == "_line_at":
        return ns[func](g("prefix"))
    if func == "_statement_owners":
        return ns[func](g("lines"))
    if func == "_demote_inert_path_reference":
        return ns[func](g("pid"), g("severity"), g("description"),
                        g("line"), g("owner_line"), g("suffix"))
    if func == "_unicode_char_name":
        return ns[func](g("char"))
    if func == "_mask_markdown_link_destinations":
        return ns[func](g("line"))
    if func == "_mask_prose_link_destinations":
        return ns[func](g("lines"))
    if func == "_padded_code_findings":
        return ns[func](g("lines"), g("rel_path"))
    if func == "_compute_docstring_lines":
        return ns[func](g("lines"))
    if func == "_decoded_views":
        return ns[func](BytesDouble(bytes(g("raw"))), g("rel_path"), g("suffix"))
    if func == "_scan_text":
        return ns[func](g("text"), g("rel_path"), g("suffix"), g("skill"))
    if func == "_unscanned_text_padded_code":
        return ns[func](P(g("file_path")), g("rel_path"))
    if func == "scan_file":
        return ns[func](P(g("file_path")), g("rel_path", ""), skill=g("skill", False))
    if func == "_community_note":
        return ns[func](mat_finding(ns, g("f")))
    if func == "_resolve_trust_level":
        return ns[func](g("source"))
    if func == "should_allow_install":
        return ns[func](mat_result(ns, g("result")), g("force", False))
    if func == "format_scan_report":
        return ns[func](mat_result(ns, g("result")))
    if func == "_build_summary":
        findings = [mat_finding(ns, d) for d in g("findings", [])]
        return ns[func](g("name"), g("source"), g("trust"), g("verdict"), findings)
    if func == "_check_structure":
        return ns[func](P(g("skill_dir")))
    if func == "scan_skill":
        files = [P(x) for x in g("files")] if g("files") is not None else None
        return ns[func](P(g("skill_path")), g("source", "community"),
                        trust_level=g("trust_level"), files=files)
    if func == "content_hash":
        return ns[func](P(g("skill_path")))
    raise ValueError("unknown func: %s" % func)


def run_case(ns, func, inp, mode="play"):
    global MODE
    MODE = mode
    setup_case(ns, func, inp)
    try:
        ret = drive(ns, func, inp)
        want_ret = proj(ret)
    except BaseException as e:
        want_ret = None
        for r in CUR["raises"]:
            if e is r.exc:
                want_ret = r.spec
                break
        if want_ret is None:
            want_ret = exc_to_spec(e)
    return {"ret": want_ret, "trace": CUR["trace"], "state": {}}


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


def main():
    ns = extract()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        want = run_case(ns, func, inp, "play")
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE skills_guard_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())