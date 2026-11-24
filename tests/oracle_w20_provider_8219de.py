#!/usr/bin/env python3
"""Differential oracle for plugins/web/exa/provider.py (gate w20_provider_8219de).

The live module is extracted with ast.parse/exec against the quarry's REAL
plugins.web._common helpers (only the Exa SDK, the env lookup and the interrupt
probe are stubbed), so the compared shapes are the real source's. The SDK is a
scripted SimpleNamespace tree built from each fixture line.
"""
import ast
import json
import logging
import sys
import types
import __future__
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


_mk("tools")
_mk("tools.interrupt", is_interrupted=lambda: False)

import plugins.web._common as W          # noqa: E402 - real shapes
import plugins.web.keyless_mcp as KM     # noqa: E402 - real tier gate

logging.getLogger().addHandler(logging.NullHandler())
logging.getLogger().setLevel(logging.WARNING)

SRC = Q + "/plugins/web/exa/provider.py"
GATE = "w20_provider_8219de"
_ANN = __future__.annotations.compiler_flag

G = {
    "__name__": "exa_provider_live",
    "__builtins__": __builtins__,
    "logging": logging,
    "Any": object, "Dict": dict, "List": list,
    "BaseWebSearchProvider": W.BaseWebSearchProvider,
    "cached_sdk_client": W.cached_sdk_client,
    "document": W.document,
    "exa_crawl_status": W.exa_crawl_status,
    "keyless_extract": W.keyless_extract,
    "keyless_search": W.keyless_search,
    "keyless_variant_schema": W.keyless_variant_schema,
    "provider_env": W.provider_env,
    "run_extract": W.run_extract,
    "run_search": W.run_search,
    "search_ok": W.search_ok,
    "use_keyless": W.use_keyless,
    "vendor_error_entry": W.vendor_error_entry,
    "web_hit": W.web_hit,
    "logger": logging.getLogger("exa_provider_live"),
}


def extract():
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef,
                             ast.Assign, ast.AnnAssign)):
            mod = ast.Module(body=[node], type_ignores=[])
            exec(compile(mod, SRC, "exec", flags=_ANN, dont_inherit=True), G)
    return G


extract()
CLS = G["ExaWebSearchProvider"]


def _obj(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: _obj(x) for k, x in v.items()})
    if isinstance(v, list):
        return [_obj(x) for x in v]
    return v


class FakeClient:
    def __init__(self, line):
        self.line = line
        self.headers = {}

    def search(self, query, num_results=5, contents=None):
        return SimpleNamespace(results=_obj(self.line.get("results") or []))

    def get_contents(self, urls, text=True):
        return SimpleNamespace(results=_obj(self.line.get("results") or []),
                               statuses=_obj(self.line.get("statuses")))


def run_case(r):
    f = r.get("func")
    if f == "search":
        G["use_keyless"] = lambda name, key: False
        G["provider_env"] = lambda name: ""
        G["_get_exa_client"] = lambda: FakeClient(r)
        return CLS().search(r.get("query", "q"), int(r.get("limit", 5)))
    if f == "extract":
        G["use_keyless"] = lambda name, key: False
        G["provider_env"] = lambda name: ""
        G["_get_exa_client"] = lambda: FakeClient(r)
        return CLS().extract(list(r.get("urls") or []))
    if f == "crawl_status":
        err = r.get("error")
        return G["_crawl_status"](SimpleNamespace(**err) if isinstance(err, dict) else err)
    if f == "vendor_error_entry":
        return W.vendor_error_entry(r.get("url") or "", status=r.get("status"),
                                    reason=r.get("reason"), body=r.get("body"))
    if f == "document":
        return W.document(r.get("url") or "", r.get("title") or "", r.get("content") or "")
    if f == "setup_schema":
        return CLS().get_setup_schema()
    if f == "use_keyless":
        tier = r.get("tier")
        KM.provider_tier = lambda name: tier
        KM.keyless_enabled = lambda: bool(r.get("keyless_enabled"))
        return KM.use_keyless("exa", r.get("api_key") or "")
    if f == "missing_key":
        return G["_MISSING_KEY"]
    return "UNKNOWN_FUNC"


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = run_case(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
sys.exit(1 if mism else 0)
