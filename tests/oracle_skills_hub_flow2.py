#!/usr/bin/env python3
"""Oracle for the hermes_cli/skills_hub.py UNCLAIMED-remainder FLOW port
(src/cli/skills_hub_flow2.c).

Drives the LIVE skills_hub.py defs (AST-extracted from the quarry, no heavy
import chain) with plain JSON inputs and compares the canonical return, the
(empty) ordered seam trace and the (empty) out-state against the C harness.
Ported defs call each other REAL inside one per-case namespace, so internal
composition (_exact_name_hits -> _slug_key/_identifier_slug,
_opt_int -> _opt_value + _int_or) is exercised on both sides.

Representation contract (both sides, see src/cli/skills_hub_flow2.c):
- tuple -> array, dict keeps insertion order, None -> null, integral floats
  fold to int; an attribute-double projects back to its raw dict.
- object doubles (source rows / bundles / auth / metadata / github) are JSON
  objects materialized as Obj (attribute access + .get); fields named by a
  direct read are present in every fixture (documented domain).
- uncaught exception -> {"__raise__": cls}; _int_or raises+catches ValueError
  internally via the full builtin hierarchy carried by the C port.
- fixture domain: text operands ASCII; _truncate widths integral; _int_or text
  is a string of ASCII digits with optional sign / ASCII whitespace; args are
  strings; .strip() operands ASCII.
Fail-closed: 0 cases => exit 1. Summary: 'ORACLE skills_hub_flow2: N cases, M
mismatches'.
"""
import __future__
import ast
import builtins
import json
import re
import sys
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
if not (QUARRY / "hermes_cli" / "skills_hub.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "skills_hub.py")

NAME = "skills_hub_flow2"

PORTED = [
    "_trust_cell", "_truncate", "_ident_col", "_row", "_slug_key",
    "_identifier_slug", "_exact_name_hits", "_hub_rate_limited",
    "_hub_rejected_credentials", "_upstream_repo", "_opt_value", "_int_or",
    "_opt_int", "_first_positional",
]
WANTED = set(PORTED)

# ── AST extraction (annotations lazy, heavy quarry import chain never run) ──
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
_missing = WANTED - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# Module constants referenced by the ported defs.
_TRUST_STYLE = {"builtin": "bright_cyan", "trusted": "green",
                "community": "yellow", "local": "dim"}


# ── attribute double: attribute access + dict-style .get ────────────────────
class Obj:
    """JSON object as an attribute carrier. Nested dicts wrap lazily so
    getattr(getattr(src, 'auth', None), 'rejected', None) and
    (getattr(bundle, 'metadata', None) or {}).get('upstream_repo') both work.
    """

    def __init__(self, d):
        self._raw = d

    def __getattr__(self, k):
        d = self.__dict__.get("_raw", {})
        if k in d:
            v = d[k]
            return Obj(v) if isinstance(v, dict) else v
        raise AttributeError(k)

    def get(self, k, default=None):
        v = self._raw.get(k, default)
        return Obj(v) if isinstance(v, dict) else v


def mat(v):
    return Obj(v) if isinstance(v, dict) else v


# ── canonical projection ────────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, Obj):
        return norm(v._raw)
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def build_ns():
    ns = {"__builtins__": builtins, "__name__": "sh2_live_skills_hub",
          "re": re, "_TRUST_STYLE": dict(_TRUST_STYLE)}
    for _nm, _code in DEF_CODE.items():
        exec(_code, ns)
    return ns


# ── live driver ─────────────────────────────────────────────────────────────
def drive(rec, ns):
    func = rec["func"]
    inp = rec["in"]
    f = ns[func]
    if func == "_trust_cell":
        if "official_label" in inp:
            return f(inp.get("trust_level"), inp.get("source"), official_label=inp["official_label"])
        return f(inp.get("trust_level"), inp.get("source"))
    if func == "_truncate":
        return f(inp.get("text"), inp.get("width"))
    if func == "_ident_col":
        return f(inp.get("style"))
    if func == "_row":
        return f(mat(inp["r"]), *inp.get("fields", []))
    if func == "_slug_key":
        return f(inp.get("text"))
    if func == "_identifier_slug":
        return f(inp.get("identifier"))
    if func == "_exact_name_hits":
        return f([mat(x) for x in inp.get("results", [])], inp.get("name"))
    if func in ("_hub_rate_limited", "_hub_rejected_credentials"):
        return f([mat(x) for x in inp.get("sources", [])])
    if func == "_upstream_repo":
        return f(mat(inp["bundle"]))
    if func == "_opt_value":
        if "last" in inp:
            return f(inp.get("args"), inp.get("flag"), inp.get("default", ""), last=inp["last"])
        return f(inp.get("args"), inp.get("flag"), inp.get("default", ""))
    if func == "_int_or":
        return f(inp.get("text"), inp.get("default", 0))
    if func == "_opt_int":
        return f(inp.get("args"), inp.get("flag"), inp.get("default", 0))
    if func == "_first_positional":
        return f(inp.get("args"))
    raise KeyError(f"unknown func {func}")


def compute(rec, ns):
    try:
        ret = drive(rec, ns)
        return {"ret": norm(ret), "trace": [], "state": {}}
    except BaseException as e:  # noqa: BLE001 - fail-closed per case
        return {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}


def main():
    n = mism = shown = 0
    ns = build_ns()
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        out = compute(rec, ns)
        if got != out:
            mism += 1
            if shown < 12:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test: drive every fixture, flag drive failures / suspicious raises ─
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    bad = 0
    ns = build_ns()
    for i, rec in enumerate(fixtures):
        try:
            spec = compute(rec, ns)["ret"]
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: AttributeError {spec}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "skills_hub_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()
