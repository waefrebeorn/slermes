#!/usr/bin/env python3
"""Oracle for the hermes_cli/plugins_install_scan.py UNCLAIMED FLOW port
(src/cli/plugins_install_flow2.c, prefix pi2_).

Drives the LIVE quarry defs (all 21 top-level defs, AST-extracted from the
read-only quarry; no heavy import chain) with plain JSON inputs and compares
the canonical return against the C harness.

Every external / sibling call is SEAMED: a play double consumes the per-case
``io[name]`` queue recorded by gen_plugins_install_flow2_cases.py. Both sides
therefore see byte-identical data; the port under test is the surrounding flow
(severity ranks/remaps, demotion ordering, verdict mapping, string formatting,
set membership and the FULL builtin exception hierarchy).

Fail-closed: 0 cases => exit 1. Summary:
'ORACLE plugins_install_flow2: N cases, M mismatches'.
"""
import __future__
import ast
import builtins
import codecs as _codecs_mod
import io as _io_mod
import json
import subprocess as _sp_mod
import sys
import tokenize as _tok_mod
import types
from datetime import UTC
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "plugins_install_scan.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "plugins_install_scan.py")

NAME = "plugins_install_flow2"

TARGETS = [
    "_walk", "_finding", "_is_main_guard", "_parse_python_source",
    "_main_guard_body_lines", "_declares_non_utf8_source", "_docstring_lines",
    "_docstring_cap", "_filter_findings", "_at_most", "_comment_severity",
    "_file_lines", "_context_severity", "_inert_context_cap",
    "_is_defensive_documentation", "_dangerous_findings_summary",
    "_tracked_bytecode", "_bytecode_findings", "_check_plugin_structure",
    "scan_plugin", "should_allow_plugin_install",
]
WANTED = set(TARGETS)

_SRCTEXT = Path(SRC).read_text(encoding="utf-8")
_FLAGS = __future__.annotations.compiler_flag
_CODE = compile(_SRCTEXT, SRC, "exec", flags=_FLAGS, dont_inherit=True)

CUR = {"io": {}, "pos": {}}


def _seam(name):
    q = CUR["io"].get(name)
    i = CUR["pos"].get(name, 0)
    if not isinstance(q, list) or i >= len(q):
        raise RuntimeError("seam exhausted: " + name)
    CUR["pos"][name] = i + 1
    return q[i]


def _exc_class(name):
    if str(name).startswith("subprocess."):
        return getattr(_sp_mod, str(name).split(".", 1)[1])
    return getattr(builtins, name, RuntimeError)


def _pop(name):
    v = _seam(name)
    if isinstance(v, dict) and "__raise__" in v:
        name = v["__raise__"]
        msg = v.get("msg", "")
        cls = _exc_class(name)
        try:
            obj = cls(msg)
        except Exception:
            try:
                obj = cls()
            except Exception:
                obj = cls.__new__(cls)
        raise obj
    return v


def _pb(name):
    v = _pop(name)
    return bool(v) if v is not None else False


def _ps(name):
    v = _pop(name)
    return v if isinstance(v, str) else ""


# ── live stubs ───────────────────────────────────────────────────────────────
class Finding:
    def __init__(self, pattern_id, severity, category, file, line, match, description):
        self.pattern_id = pattern_id
        self.severity = severity
        self.category = category
        self.file = file
        self.line = line
        self.match = match
        self.description = description


class ScanResult:
    def __init__(self, skill_name, source, trust_level, verdict, findings,
                 scanned_at, summary):
        self.skill_name = skill_name
        self.source = source
        self.trust_level = trust_level
        self.verdict = verdict
        self.findings = findings
        self.scanned_at = scanned_at
        self.summary = summary


SUSPICIOUS_BINARY_EXTENSIONS = {
    '.exe', '.dll', '.so', '.dylib', '.bin', '.dat', '.com', '.msi',
    '.dmg', '.app', '.deb', '.rpm'}


def _determine_verdict(findings):
    sev = {f.severity for f in findings}
    return "dangerous" if "critical" in sev else "caution" if "high" in sev else "safe"


def build_base_ns():
    stub = types.ModuleType("tools.skills_guard")
    stub.Finding = Finding
    stub.ScanResult = ScanResult
    stub.SUSPICIOUS_BINARY_EXTENSIONS = SUSPICIOUS_BINARY_EXTENSIONS
    stub._COMPILED_THREAT_PATTERNS = []
    stub._determine_verdict = _determine_verdict
    stub.format_scan_report = lambda *a, **k: ""
    stub.scan_file = lambda *a, **k: []
    tools = sys.modules.setdefault("tools", types.ModuleType("tools"))
    tools.skills_guard = stub
    sys.modules["tools.skills_guard"] = stub
    ns = {"__builtins__": builtins, "__name__": "pi2_live", "UTC": UTC}
    exec(_CODE, ns)
    missing = WANTED - {k for k in ns if callable(ns[k])}
    if missing:
        print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(missing)}", file=sys.stderr)
        sys.exit(1)
    return ns


class FakeFile:
    def __str__(self):
        return self.spec.get("resolved", "/".join(self._parts))

    def __lt__(self, other):
        return str(self) < str(other)

    def __init__(self, spec):
        self.spec = spec
        self.name = spec.get("name", "")
        self._parts = tuple(spec.get("parts", []))
        self.suffix = spec.get("suffix", "")

    def relative_to(self, root):
        return self

    @property
    def parts(self):
        return self._parts

    def is_file(self):
        return self.spec.get("is_file", True)

    def is_symlink(self):
        return self.spec.get("is_symlink", False)

    def resolve(self):
        if self.spec.get("resolve_raise", False):
            raise OSError("resolve failed")
        return self

    def is_relative_to(self, root):
        return self.spec.get("in_root", True)

    def stat(self):
        return types.SimpleNamespace(st_size=self.spec.get("size", 0))


class FakePath:
    def __init__(self, inp):
        self.inp = inp

    def read_text(self, encoding=None):
        v = _pop("read_text")
        if isinstance(v, list):
            return "\n".join(v)
        return v

    def read_bytes(self):
        v = _pop("read_bytes")
        if isinstance(v, list):
            return bytes(v)
        return v

    def is_dir(self):
        return _pop("is_dir")

    def rglob(self, pat):
        return [FakeFile(e) for e in self.inp.get("entries", [])]

    def resolve(self):
        return FakePath(self.inp)

    @property
    def name(self):
        return self.inp.get("dir_name", "plugin")

    @property
    def suffix(self):
        return self.inp.get("file_suffix", "")


class Obj:
    def __init__(self, d):
        object.__setattr__(self, "_raw", dict(d) if isinstance(d, dict) else {})

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_raw")
        if k in d:
            return d[k]
        raise AttributeError(k)

    def __setattr__(self, k, v):
        object.__getattribute__(self, "_raw")[k] = v


def finding(inp):
    if "finding" not in inp:
        return None
    return Obj(inp["finding"])


# ── namespace overrides per function ─────────────────────────────────────────
class _AstProxy:
    def __getattr__(self, k):
        return getattr(ast, k)


class _TokProxy:
    def __getattr__(self, k):
        return getattr(_tok_mod, k)


class _CodecsProxy:
    def __getattr__(self, k):
        return getattr(_codecs_mod, k)


class _SpProxy:
    def __getattr__(self, k):
        return getattr(_sp_mod, k)


class _DtProxy:
    def __getattr__(self, k):
        return getattr(__import__("datetime"), k)


class _Owner:
    pass


class _Other:
    pass


def _build_ast_node(d):
    if not isinstance(d, dict):
        return None
    t = d.get("type", "")
    if t == "If" or "test" in d:
        return types.SimpleNamespace(test=_build_ast_node(d.get("test")))
    if t == "Compare" or "ops" in d:
        def _op(o):
            if o == "Eq":
                return ast.Eq()
            if o == "NotEq":
                return ast.NotEq()
            return _build_ast_node(o)
        return ast.Compare(left=_build_ast_node(d.get("left")),
                           ops=[_op(o) for o in d.get("ops", [])],
                           comparators=[_build_ast_node(c) for c in d.get("comparators", [])])
    if t == "Name":
        return ast.Name(id=d.get("id", ""), ctx=ast.Load())
    if t == "Constant":
        return ast.Constant(value=d.get("value"))
    if t == "Eq":
        return ast.Eq()
    if t == "NotEq":
        return ast.NotEq()
    return None


def override(ns, func, inp):
    g = inp.get

    if func == "_parse_python_source":
        px = _AstProxy()
        def fake_parse(src, *a, **k):
            return _pop("ast_parse")
        px.parse = fake_parse
        ns["ast"] = px

    elif func == "_main_guard_body_lines":
        ns["_parse_python_source"] = lambda path: _pop("parse_source")
        ns["_is_main_guard"] = lambda node: bool(getattr(node, "is_guard", False))
        spec = inp.get("walk", [])
        class IfNode:
            pass
        nodes = []
        for s in spec:
            nd = IfNode()
            nd.is_guard = bool(s.get("is_guard", False))
            nd.body = [types.SimpleNamespace(lineno=int(b.get("lineno", 0)),
                                             end_lineno=int(b.get("end_lineno", b.get("lineno", 0))))
                       for b in s.get("body", [])]
            nodes.append(nd)
        px = _AstProxy()
        px.If = IfNode
        px.walk = lambda tree: nodes
        ns["ast"] = px

    elif func == "_declares_non_utf8_source":
        tk = _TokProxy()
        tk.detect_encoding = lambda readline: (_pop("detect_encoding"),)
        ns["tokenize"] = tk
        cd = _CodecsProxy()
        def fake_lookup(codec):
            v = _pop("lookup")
            if isinstance(v, dict):
                return types.SimpleNamespace(name=v.get("name", ""))
            return types.SimpleNamespace(name=str(v))
        cd.lookup = fake_lookup
        ns["codecs"] = cd

    elif func == "_docstring_lines":
        ns["_parse_python_source"] = lambda path: _pop("parse_source")
        ns["_declares_non_utf8_source"] = lambda path: bool(_pop("declares"))
        nodes = []
        for s in inp.get("walk", []):
            owner = bool(s.get("owner", False))
            nd = _Owner() if owner else _Other()
            nd.lineno = int(s.get("lineno", 0))
            nd.end_lineno = int(s.get("end_lineno", nd.lineno))
            first = s.get("first")
            if owner and isinstance(first, dict):
                e = ast.Expr(value=ast.Constant(value=first.get("value", "doc")))
                e.lineno = int(first.get("lineno", nd.lineno))
                e.end_lineno = int(first.get("end_lineno", e.lineno))
                nd.body = [e]
            else:
                nd.body = []
            nodes.append(nd)
        px = _AstProxy()
        px.walk = lambda tree: nodes
        ns["ast"] = px
        ns["_DOCSTRING_OWNERS"] = (_Owner,)

    elif func in ("_docstring_cap", "_comment_severity"):
        ns["is_agent_facing"] = lambda f: _pb("is_agent_facing")

    elif func == "_filter_findings":
        ns["_main_guard_body_lines"] = lambda path: _pop("main_guard_lines")
        ns["_docstring_lines"] = lambda path: _pop("docstring_lines")
        ns["_file_lines"] = lambda path: _pop("file_lines")
        ns["logical_line"] = lambda lines, i: _ps("logical_line")
        ns["is_ci_workflow"] = lambda rel: _pb("is_ci")
        ns["is_doc_prose"] = lambda rel: _pb("is_doc_prose")
        ns["is_locale_catalog"] = lambda rel: _pb("locale_catalog")
        ns["_context_severity"] = lambda *a, **k: _ps("context_severity")
        ns["_is_defensive_documentation"] = lambda *a, **k: _pb("is_defensive_documentation")
        ns["_docstring_cap"] = lambda f: _ps("docstring_cap")
        ns["_comment_severity"] = lambda f: _ps("comment_severity")
        ns["is_quoted_injection_phrase"] = lambda *a, **k: _pb("is_quoted_injection_phrase")

    elif func == "_context_severity":
        ns["catalog_cap"] = lambda f: _pop("catalog_cap")
        ns["prose_cap"] = lambda f: _pop("prose_cap")
        ns["is_self_uninstall_doc"] = lambda *a: _pb("is_self_uninstall_doc")
        ns["is_json_prose_value"] = lambda *a: _pb("is_json_prose_value")
        ns["is_test_tree"] = lambda rel: _pb("is_test_tree")
        ns["is_inert_fixture_line"] = lambda *a: _pb("is_inert_fixture_line")
        ns["is_regex_alternation_token"] = lambda *a: _pb("is_regex_alternation_token")
        for nm in ("is_base64_media", "is_loopback_only", "is_loopback_continuation",
                   "is_hex_in_char_class", "is_pip_install_in_prose_literal", "is_own_proc_read",
                   "is_base64_archive_roundtrip", "is_coin_name_only", "is_trusted_installer_pipe",
                   "is_sudo_inert", "is_env_dump_in_safety_clause", "is_repo_root_traversal",
                   "is_benign_lazy_os_import", "is_disclosed_config_note",
                   "is_authorized_keys_prose", "is_public_client_key"):
            ns[nm] = (lambda name: (lambda *a: _pb(name)))(nm)

    elif func == "_inert_context_cap":
        for nm in ("is_base64_media", "is_loopback_only", "is_loopback_continuation",
                   "is_hex_in_char_class", "is_pip_install_in_prose_literal", "is_own_proc_read",
                   "is_base64_archive_roundtrip", "is_coin_name_only", "is_trusted_installer_pipe",
                   "is_sudo_inert", "is_env_dump_in_safety_clause", "is_repo_root_traversal",
                   "is_benign_lazy_os_import", "is_disclosed_config_note",
                   "is_authorized_keys_prose", "is_public_client_key"):
            ns[nm] = (lambda name: (lambda *a: _pb(name)))(nm)

    elif func == "_tracked_bytecode":
        sp = _SpProxy()
        def fake_run(*a, **k):
            v = _pop("subprocess_run")
            if isinstance(v, dict) and "out" in v:
                raw = b"\0".join(bytes(x) if isinstance(x, list) else str(x).encode() for x in v["out"])
                return types.SimpleNamespace(stdout=raw)
            return types.SimpleNamespace(stdout=b"")
        sp.run = fake_run
        ns["subprocess"] = sp

    elif func == "_bytecode_findings":
        ns["_walk"] = lambda d: [(FakeFile(e), e.get("rel", "")) for e in _pop("walk")]
        ns["_tracked_bytecode"] = lambda d: set(_pop("tracked"))

    elif func == "_check_plugin_structure":
        ns["_bytecode_findings"] = lambda d: [Obj(x) for x in _pop("bytecode_findings")]
        ns["_walk"] = lambda d: [(FakeFile(e), e.get("rel", "")) for e in _pop("walk")]

    elif func == "scan_plugin":
        ns["_check_plugin_structure"] = lambda d: [Obj(x) for x in _pop("structure")]
        ns["_walk"] = lambda d: [(FakeFile(e), e.get("rel", "")) for e in _pop("walk")]
        ns["scan_file"] = lambda f, rel_path="": _pop("scan_file")
        ns["_filter_findings"] = lambda *a: [Obj(x) for x in _pop("filter_findings")]
        ns["_determine_verdict"] = lambda findings: _ps("verdict")
        dt = _DtProxy()
        def fake_now(*a, **k):
            return types.SimpleNamespace(isoformat=lambda: _ps("scanned_at"))
        dt.now = fake_now
        ns["datetime"] = dt


def drive(rec, ns, func, inp):
    f = ns[func]
    g = inp.get
    if func == "_walk":
        return [{"name": ff.name, "rel": rel} for ff, rel in ns["_walk"](FakePath(inp))]
    if func == "_finding":
        return f(g("pattern_id", ""), g("severity", ""), g("category", ""),
                 g("file", ""), g("match", ""), g("description", ""))
    if func == "_is_main_guard":
        return f(_build_ast_node(inp.get("node")))
    if func == "_parse_python_source":
        return f(FakePath(inp))
    if func == "_main_guard_body_lines":
        return f(FakePath(inp))
    if func == "_declares_non_utf8_source":
        return f(FakePath(inp))
    if func == "_docstring_lines":
        return f(FakePath(inp))
    if func == "_docstring_cap":
        return f(finding(inp))
    if func == "_filter_findings":
        return f([Obj(x) for x in g("findings", [])], g("rel_path", ""), FakePath(inp))
    if func == "_at_most":
        return f(g("severity", ""), g("cap", "medium"))
    if func == "_comment_severity":
        return f(finding(inp))
    if func == "_file_lines":
        return f(FakePath(inp))
    if func == "_context_severity":
        return f(finding(inp), g("rel_path", ""), g("line", ""), g("joined", ""),
                 g("doc_prose", False), g("is_code", True), g("locale_catalog", False))
    if func == "_inert_context_cap":
        return f(finding(inp), g("rel_path", ""), g("line", ""), g("joined", ""),
                 g("is_code", True), g("doc_prose", False), g("sev", "critical"))
    if func == "_is_defensive_documentation":
        return f(finding(inp), g("rel_path", ""), g("docstring_lines", None))
    if func == "_dangerous_findings_summary":
        return f([Obj(x) for x in g("findings", [])])
    if func == "_tracked_bytecode":
        return f(FakePath(inp))
    if func == "_bytecode_findings":
        return f(FakePath(inp))
    if func == "_check_plugin_structure":
        return f(FakePath(inp))
    if func == "scan_plugin":
        return f(FakePath(inp), g("source", ""))
    if func == "should_allow_plugin_install":
        result = dict(g("result", {}))
        if "findings" in result:
            result["findings"] = [Obj(x) for x in result["findings"]]
        return f(Obj(result), g("force", False))
    raise KeyError("unknown func " + str(func))


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, (bytes, bytearray)):
        return list(v)
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, Obj):
        return norm(object.__getattribute__(v, "_raw"))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if hasattr(v, "__dict__"):
        return norm(vars(v))
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def compute(rec):
    func = rec["func"]
    inp = rec.get("in", {})
    CUR["io"] = inp.get("io", {})
    CUR["pos"] = {}
    ns = build_base_ns()
    override(ns, func, inp)
    try:
        return {"ret": norm(drive(rec, ns, func, inp)), "trace": [], "state": {}}
    except BaseException as e:
        return {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}


def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        out = compute(rec)
        if got != out:
            mism += 1
            if shown < 20:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print("  C=" + json.dumps(got, ensure_ascii=False)[:900])
                print("  P=" + json.dumps(out, ensure_ascii=False)[:900])
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
