#!/usr/bin/env python3
"""Oracle for hermes_cli/auth.py pure URL/key predicates.

Harness families (inputs emitted inline, so the oracle replays the live python
on the SAME values):
  local       {in:<url|null>, ret}      -> is_actual_local_base_url
  normalize   {in:<url|null>, out:<>}   -> normalize_actual_base_url
  placeholder {in:<key|null>, ret}      -> _is_placeholder_shape
  keyprefix   {prov,val,ret}            -> _matches_key_prefix
  openrouter  {in:<key|null>, ret}      -> looks_like_openrouter_key
Seeds: urlparse, dataclass(_finish), the constants (DEFAULT_ACTUAL_BASE_URL,
_PLACEHOLDER_KEY_PREFIXES, KNOWN_PROVIDER_KEY_PREFIXES).
"""
import ast
import json
import sys
from urllib.parse import urlparse

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/auth.py"
DEFAULT_ACTUAL_BASE_URL = "https://api.actual.inc/v1"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"urlparse": urlparse,
          "_PLACEHOLDER_KEY_PREFIXES": ("sk-", "ghp_", "hf_"),
          "DEFAULT_ACTUAL_BASE_URL": DEFAULT_ACTUAL_BASE_URL}
    for n in ast.parse(src).body:
        if isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name) \
                and n.target.id == "KNOWN_PROVIDER_KEY_PREFIXES":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in (
                "is_actual_local_base_url", "normalize_actual_base_url",
                "_is_placeholder_shape", "_matches_key_prefix",
                "looks_like_openrouter_key"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    is_local = ns["is_actual_local_base_url"]
    normalize = ns["normalize_actual_base_url"]
    is_placeholder = ns["_is_placeholder_shape"]
    match_prefix = ns["_matches_key_prefix"]
    looks_or = ns["looks_like_openrouter_key"]

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE hermes_auth: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        fam = c.get("family")
        if fam == "local":
            total += 1
            want = bool(is_local(c.get("in")))
            if c.get("ret") != want:
                mism += 1
                print(f"MISMATCH local {c.get('case')} in={c.get('in')!r} C={c.get('ret')!r} PY={want!r}")
        elif fam == "normalize":
            total += 1
            want = normalize(c.get("in"))
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH normalize {c.get('case')} in={c.get('in')!r} C={c.get('out')!r} PY={want!r}")
        elif fam == "placeholder":
            total += 1
            q = c.get("in")
            want = bool(is_placeholder(q if q is not None else ""))
            if c.get("ret") != want:
                mism += 1
                print(f"MISMATCH placeholder {c.get('case')} in={q!r} C={c.get('ret')!r} PY={want!r}")
        elif fam == "keyprefix":
            total += 1
            want = bool(match_prefix(c.get("prov"), c.get("val")))
            if c.get("ret") != want:
                mism += 1
                print(f"MISMATCH keyprefix {c.get('case')} prov={c.get('prov')!r} val={c.get('val')!r} C={c.get('ret')!r} PY={want!r}")
        elif fam == "openrouter":
            total += 1
            want = bool(looks_or(c.get("in")))
            if c.get("ret") != want:
                mism += 1
                print(f"MISMATCH openrouter {c.get('case')} in={c.get('in')!r} C={c.get('ret')!r} PY={want!r}")
    print(f"ORACLE hermes_auth: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
