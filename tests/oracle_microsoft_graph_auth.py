#!/usr/bin/env python3
"""Oracle for tools/microsoft_graph_auth.py::format_graph_error and ::_fresh_cached.

`format_graph_error` is a module function. `_fresh_cached` is a method on
MicrosoftGraphTokenProvider; the oracle drives it through a fake self carrying
the same attributes the live method reads (self._cached_token, self.skew_seconds),
built from the live CachedAccessToken dataclass. Expiry cases are placed far
enough from now that the live time.time() and the C time(NULL) agree.
"""
import ast
import dataclasses
import json
import sys
import time

SRC = "C:/Users/eman5/hermes-agent/tools/microsoft_graph_auth.py"

# `from __future__ import annotations` keeps the class-body annotations (which
# reference GraphCredentials/httpx) from being evaluated at def time.
_FUTURE = ast.ImportFrom(module="__future__", names=[ast.alias(name="annotations")], level=0)


class _FakeSelf:
    def __init__(self, cached, skew):
        self._cached_token = cached
        self.skew_seconds = skew


def main() -> int:
    ns = {"dataclass": dataclasses.dataclass, "time": time}
    for n in ast.parse(open(SRC, encoding="utf-8").read()).body:
        if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) \
                and n.targets[0].id.startswith("DEFAULT_"):
            exec(compile(ast.fix_missing_locations(ast.Module(body=[_FUTURE, n], type_ignores=[])), SRC, "exec"), ns)
        elif isinstance(n, ast.ClassDef) and n.name in (
                "GraphCredentials", "CachedAccessToken", "MicrosoftGraphTokenProvider"):
            exec(compile(ast.fix_missing_locations(ast.Module(body=[_FUTURE, n], type_ignores=[])), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name == "format_graph_error":
            exec(compile(ast.fix_missing_locations(ast.Module(body=[_FUTURE, n], type_ignores=[])), SRC, "exec"), ns)
    fn = ns["format_graph_error"]
    provider = ns["MicrosoftGraphTokenProvider"]
    cached_cls = ns["CachedAccessToken"]

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        tag = c.get("tag")
        if tag == "fge":
            total += 1
            want = fn(c.get("in"))
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH {c.get('case')}: in={c.get('in')!r} C={got!r} PY={want!r}")
        elif tag == "fc":
            total += 1
            expires_at = c["expires_at"]
            cached = cached_cls("tok", float(expires_at)) if c["cached"] else None
            want = provider._fresh_cached(_FakeSelf(cached, c["skew"])) is not None
            got = bool(c["fresh"])
            if want != got:
                mism += 1
                print(f"MISMATCH {c.get('case')}: cached={c['cached']} "
                      f"expires_at={expires_at} skew={c['skew']} C={got} PY={want}")
    if total == 0:
        print("ORACLE microsoft_graph_auth: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE microsoft_graph_auth: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
