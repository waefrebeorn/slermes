#!/usr/bin/env python3
"""Oracle for the anthropic_adapter FLOW port (src/agent/anthropic_adapter_flow2.c).

Drives the LIVE quarry defs of 12 UNCLAIMED agent/anthropic_adapter.py functions
with SCRIPTED seams: every foreign collaborator (endpoint predicates, httpx.Timeout,
version/credential helpers, claude_family_version, custom-provider headers,
logger.debug, the claimed _common_betas_for_base_url / _model_matches /
_client_timeout / _is_pre_thinking_haiku siblings) is a recording stub fed the
SAME `io` scripts the C harness feeds aa2_seam.  Compares the ordered decision
trace ({ev,args,ret}), the canonical return, and the out-state (in-place
mutators only).  The PORTED functions are AST-extracted from the quarry under a
registered sys.modules name; a function that is both ported and seamed is kept
as the original callable (ORIG) with __globals__ = the live namespace, so its
internal calls resolve to the installed seam stubs (composition behaviour).

Fail-closed: 0 cases => exit 1; MISSING seam script => RuntimeError("no script
for seam: <name>"); a scripted null is a real None; per-case BaseException catch
(SystemExit / KeyboardInterrupt / GeneratorExit included).  Exception classes
resolve through getattr(builtins, ...) -- the FULL builtin hierarchy, including
the Unicode*Error five-arg constructors -- and scripted raises round-trip bare
(identity match) so multi-arg constructors (KeyError) are not mangled.  The
`except Exception` boundary in _custom_provider_extra_headers is modelled on the
C side by the Exception-vs-BaseException split.

Summary: 'ORACLE anthropic_adapter_flow2: N cases, M mismatches'.
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "anthropic_adapter.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_HERE.parents[2]))
os.environ.setdefault("USERPROFILE", str(_HERE.parents[2]))
os.environ.setdefault("LOCALAPPDATA", str(_HERE.parents[2] / "AppData" / "Local"))

SRC = str(QUARRY / "agent" / "anthropic_adapter.py")
NAME = "anthropic_adapter_flow2"

PORTED = [
    "_model_matches", "_beta_header", "_normalize_to_mcp_wire",
    "_apply_oauth_prose_aliases", "_attribution_headers", "_client_timeout",
    "_auth_style", "_thinking_kwargs", "_is_pre_thinking_haiku",
    "_base_client_kwargs", "_custom_provider_extra_headers",
    "buffer_anthropic_tool_input",
]
WANTED = set(PORTED)
import __future__
_FLAGS = __future__.annotations.compiler_flag


def _compile(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                   flags=_FLAGS, dont_inherit=True)


# ── AST extraction ───────────────────────────────────────────────────────
_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_TEXT)
DEF_CODE = {}
for _n in _TREE.body:
    if isinstance(_n, ast.FunctionDef) and _n.name in WANTED:
        DEF_CODE[_n.name] = _compile(_n)
_missing = set(PORTED) - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


# ── canonical projection ─────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {str(k): norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


# ── per-case context ─────────────────────────────────────────────────────
CUR = {"scripts": {}, "trace": [], "raised": [], "idx": {}, "stwin": {}}


def build_exc(spec):
    """Full builtin exception hierarchy via getattr(builtins, ...)."""
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(str(kind), (Exception,), {})
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


def raise_spec(e):
    for exc, spec in CUR["raised"]:
        if e is exc:
            return spec          # scripted raise round-trips bare
    return {"__raise__": type(e).__name__, "msg": str(e)}


def seam(name, args):
    args = {k: norm(v) for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: " + name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = copy.deepcopy(r)         # live mutation cannot corrupt script/trace
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and isinstance(r.get("__raise__"), str):
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


# ── seam stubs ───────────────────────────────────────────────────────────
class SeamFn:
    def __init__(self, ev, argspec):
        self.ev = ev
        self.argspec = argspec

    def __call__(self, *a, **kw):
        return seam(self.ev, self.argspec(a, kw))


def _pos0(a, k):
    return {"arg": a[0]} if a else {}


class VersionInfoSeam:
    """get_version_info() -> object with .base_version; records the raw dict."""
    def __call__(self, *a, **kw):
        r = seam("get_version_info", {})
        if isinstance(r, dict):
            return types.SimpleNamespace(**r)
        return r


class ClaudeFamilyVersionSeam:
    """claude_family_version() -> tuple-of-tuples so `parsed[1] < (5, 5)` works;
    the trace still records the raw JSON list the C harness emits."""
    def __call__(self, *a, **kw):
        r = seam("claude_family_version", {"model": a[0]})
        if isinstance(r, list):
            return tuple(tuple(x) if isinstance(x, list) else x for x in r)
        return r


SEAM_SPECS = {
    # global name in live module -> (ev name, positional-arg name mapper)
    "get_version_info": ("get_version_info", lambda a, k: {}),
    "_normalize_base_url_text": ("normalize_base_url_text", lambda a, k: {"base_url": a[0]}),
    "_is_azure_anthropic_endpoint": ("is_azure_anthropic_endpoint", lambda a, k: {"base_url": a[0]}),
    "_is_kimi_coding_endpoint": ("is_kimi_coding_endpoint", lambda a, k: {"base_url": a[0]}),
    "_requires_bearer_auth": ("requires_bearer_auth", lambda a, k: {"base_url": a[0]}),
    "_is_third_party_anthropic_endpoint": ("is_third_party_anthropic_endpoint", lambda a, k: {"base_url": a[0]}),
    "_is_oauth_token": ("is_oauth_token", lambda a, k: {"api_key": a[0]}),
    "_common_betas_for_base_url": ("common_betas_for_base_url", lambda a, k: {"base_url": a[0]}),
    # ported names that are ALSO seamed when called from a sibling function
    "_client_timeout": ("client_timeout", lambda a, k: {"timeout": a[0] if a else k.get("timeout")}),
    "_model_matches": ("model_matches", lambda a, k: {"model": a[0]}),
    "_is_pre_thinking_haiku": ("is_pre_thinking_haiku", lambda a, k: {"model": a[0]}),
    # thinking-policy predicates (not ported; called by _thinking_kwargs)
    "_accepts_thinking_disable": ("accepts_thinking_disable", lambda a, k: {"model": a[0]}),
    "_supports_adaptive_thinking": ("supports_adaptive_thinking", lambda a, k: {"model": a[0]}),
    "_supports_xhigh_effort": ("supports_xhigh_effort", lambda a, k: {"model": a[0]}),
}


def install_stubs():
    agent = types.ModuleType("agent")
    agent.__path__ = []
    sys.modules["agent"] = agent

    hc = types.ModuleType("hermes_cli")
    hc.__path__ = []
    sys.modules["hermes_cli"] = hc

    httpx = types.ModuleType("httpx")
    httpx.Timeout = SeamFn("httpx.Timeout",
                           lambda a, k: {"timeout": k["timeout"], "connect": k["connect"]})
    sys.modules["httpx"] = httpx

    atp = types.ModuleType("agent.anthropic_thinking_policy")
    atp.claude_family_version = ClaudeFamilyVersionSeam()
    sys.modules["agent.anthropic_thinking_policy"] = atp
    agent.anthropic_thinking_policy = atp

    cfg = types.ModuleType("hermes_cli.config")
    cfg.get_custom_provider_extra_headers = SeamFn(
        "get_custom_provider_extra_headers", lambda a, k: {"base_url": a[0]})
    sys.modules["hermes_cli.config"] = cfg
    hc.config = cfg


# ── base namespace (registered module) ───────────────────────────────────
MOD = types.ModuleType("aa2_live_adapter")
sys.modules["aa2_live_adapter"] = MOD
NS = MOD.__dict__
NS.update({
    "__name__": "aa2_live_adapter",
    "__builtins__": builtins,
    "json": json, "copy": copy, "re": __import__("re"), "math": __import__("math"),
    "logging": __import__("logging"), "os": __import__("os"),
    "Any": object, "Dict": dict, "List": list, "Optional": object, "Tuple": tuple,
    "logger": types.SimpleNamespace(
        debug=SeamFn("logger.debug", lambda a, k: {"msg": a[0] if a else ""})),
})

# Seed module-level constants by exec'ing the source Assign/AnnAssign nodes in
# order (they carry implicit string concatenation, frozensets and compiled
# regexes that literal_eval cannot handle).  Late-declared names referenced by
# earlier aliases are resolved by source order.
for _n in _TREE.body:
    if isinstance(_n, (ast.Assign, ast.AnnAssign)):
        try:
            exec(_compile(_n), NS)
        except Exception:
            pass
# Install claimed/foreign seam stubs (call-time global lookup) AFTER exec.
for _gl, (ev, argspec) in SEAM_SPECS.items():
    NS[_gl] = SeamFn(ev, argspec)
NS["get_version_info"] = VersionInfoSeam()
# The source Assign `logger = logging.getLogger(__name__)` ran above; restore
# the recording logger stub so the except-path logger.debug is traced.
NS["logger"] = types.SimpleNamespace(
    debug=SeamFn("logger.debug", lambda a, k: {"msg": a[0] if a else ""}))
# Exec the live function defs, keeping the ORIGINAL callables for direct drives.
for _nm, _code in DEF_CODE.items():
    exec(_code, NS)
ORIG = {nm: NS[nm] for nm in PORTED}
# Re-install the seamed ported names so sibling calls go to the recorder.  The
# ORIG callables keep __globals__ = NS, so their own bodies see the same seams.
for _gl in ("_client_timeout", "_model_matches", "_is_pre_thinking_haiku"):
    NS[_gl] = SeamFn(*SEAM_SPECS[_gl])
install_stubs()
# Sanity: constants the ported functions rely on must have been seeded.
for _c in ("_MCP_TOOL_PREFIX", "_OAUTH_PROSE_ALIAS_PATTERNS", "THINKING_BUDGET",
           "ADAPTIVE_EFFORT_MAP", "_TOOL_STREAMING_BETA"):
    if _c not in NS:
        print(f"ORACLE {NAME}: FATAL missing constant {_c}", file=sys.stderr)
        sys.exit(1)


def build_ns(rec):
    inp = rec["in"]
    CUR["scripts"] = inp.get("io") or {}
    CUR["trace"] = []
    CUR["raised"] = []
    CUR["idx"] = {}
    CUR["stwin"] = {}
    return NS


# ── live driver ──────────────────────────────────────────────────────────
def drive(rec, ns):
    func = rec["func"]
    g = (rec["in"]).get
    F = ORIG[func]
    if func == "_model_matches":
        return F(g("model"), g("substrings"))
    if func == "_beta_header":
        return F(g("betas"))
    if func == "_normalize_to_mcp_wire":
        return F(g("name"))
    if func == "_apply_oauth_prose_aliases":
        return F(g("text", ""))
    if func == "_attribution_headers":
        return F()
    if func == "_client_timeout":
        return F(g("timeout"))
    if func == "_auth_style":
        return F(g("api_key"), g("base_url"), g("normalized_base_url"))
    if func == "_thinking_kwargs":
        return F(g("reasoning_config"), g("model"), g("effective_max_tokens"))
    if func == "_is_pre_thinking_haiku":
        return F(g("model"))
    if func == "_base_client_kwargs":
        return F(g("base_url"), g("timeout"))
    if func == "_custom_provider_extra_headers":
        return F(g("base_url"))
    if func == "buffer_anthropic_tool_input":
        kw = copy.deepcopy(g("api_kwargs"))
        CUR["stwin"] = {"api_kwargs": kw}
        F(kw, g("base_url"))
        return None
    raise KeyError("unknown func " + str(func))


def run_case(rec):
    ns = build_ns(rec)
    try:
        ret = drive(rec, ns)
        return {"ret": norm(ret), "trace": CUR["trace"], "state": norm(CUR["stwin"])}
    except BaseException as e:
        return {"ret": raise_spec(e), "trace": CUR["trace"], "state": norm(CUR["stwin"])}


# ── main ─────────────────────────────────────────────────────────────────
def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        try:
            out = run_case(rec)
        except BaseException as e:
            out = {"ret": {"__raise__": type(e).__name__, "msg": str(e)},
                   "trace": [], "state": {}}
        got = rec.get("out")
        if norm(got) != norm(out):
            mism += 1
            if shown < 12:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test: drive every fixture, flag binding/seeding failures ────────
def selftest(fixtures):
    bad = 0
    seam_names = set(SEAM_SPECS) | {"httpx.Timeout", "logger.debug",
                                   "claude_family_version", "get_custom_provider_extra_headers",
                                   "model_matches", "accepts_thinking_disable",
                                   "supports_adaptive_thinking", "supports_xhigh_effort"}
    ported_names = set(PORTED)
    seen = set()
    for i, rec in enumerate(fixtures):
        seen.add(rec["func"])
        try:
            out = run_case(rec)
            spec = out["ret"]
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("msg", "")
                if any(("'" + nm + "'") in msg for nm in (ported_names | seam_names)):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError naming ported/seam: {msg}")
            if isinstance(spec, dict) and spec.get("__raise__") == "NameError":
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: NameError (seeding gap): {spec.get('msg')}")
        except BaseException as e:
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    missing = set(PORTED) - seen
    if missing:
        bad += 1
        print(f"SELFTEST: funcs with no fixture: {sorted(missing)}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "anthropic_adapter_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").split("\n") if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()
