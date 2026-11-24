#!/usr/bin/env python3
"""Differential oracle for plugins/web/xai/provider.py (gate w19_provider_3254dd).

Live code is extracted with ast.parse and exec'd into a stub-filled globals dict: every
module-level def/class/assign is re-executed, so the compared behaviour is the real source.
"""
import ast
import json
import logging
import re
import sys
import types
import __future__

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = "C:/Users/eman5/slermes"
Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/plugins/web/xai/provider.py"
GATE = "w19_provider_3254dd"
_ANN = __future__.annotations.compiler_flag

LOG = logging.getLogger("prov_oracle")
LOG.addHandler(logging.NullHandler())
LOG.propagate = False


def title_hit(title, url, description, position):
    return {"title": title, "url": url, "description": description, "position": position}


def search_ok(rows):
    return {"success": True, "data": {"web": rows}}


def search_fail(error):
    return {"success": False, "error": error}


def setup_schema(*a, **k):
    return {}


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


CUR = {"cfg": {}}


def load_config():
    return CUR["cfg"]


_mk("hermes_cli")
_mk("hermes_cli.config", load_config=load_config)
_mk("tools")
_mk("tools.xai_http", has_xai_credentials=lambda: False,
    hermes_xai_user_agent=lambda: "oracle",
    resolve_xai_http_credentials=lambda **k: {})
_mk("plugins")
_mk("plugins.web")
_mk("plugins.web._common", BaseWebSearchProvider=object, search_fail=search_fail,
    search_ok=search_ok, setup_schema=setup_schema, title_hit=title_hit)


def extract():
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    g = {"__name__": "prov_live", "json": json, "re": re, "logging": logging, "logger": LOG,
         "Any": object, "Dict": dict, "List": list, "Optional": object,
         "_fail": search_fail, "search_ok": search_ok, "setup_schema": setup_schema,
         "_row": title_hit, "BaseWebSearchProvider": object,
         "has_xai_credentials": lambda: False,
         "hermes_xai_user_agent": lambda: "oracle",
         "resolve_xai_http_credentials": lambda **k: {},
         "logger2": LOG}
    g["__builtins__"] = __builtins__
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.ClassDef, ast.Assign, ast.AnnAssign)):
            mod = ast.Module(body=[node], type_ignores=[])
            exec(compile(mod, SRC, "exec", flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
CLS = G["XAIWebSearchProvider"]
CASTS = {"int": int, "float": float, "str": str}


def run_case(r):
    f = r.get("func")
    limit = r.get("limit", 5)
    if f == "domain_list":
        return G["_coerce_domain_list"](r.get("value"))
    if f == "coerce":
        return G["_coerce"](CASTS[r.get("cast", "int")], r.get("value"), r.get("default"))
    if f == "tool":
        return CLS._web_search_tool(r.get("cfg") or {})
    if f == "prompt":
        return CLS._build_prompt(r.get("query", ""), limit)
    if f == "collect":
        blocks, anns = CLS._collect_output_text(r.get("resp") or {})
        return [blocks, anns]
    if f == "parse":
        return CLS._try_parse_json_results(r.get("text", ""), limit=limit)
    if f == "annotations":
        return CLS._results_from_annotations(r.get("annotations") or [],
                                             r.get("joined", ""), limit=limit)
    if f == "extract":
        return CLS._extract_results(r.get("resp") or {}, limit=limit)
    if f == "web_config":
        CUR["cfg"] = r.get("cfg")
        return G["_load_xai_web_config"]()
    if f == "clamp":
        return max(1, min(G["_coerce"](int, r.get("limit"), 5), 100))
    return "UNKNOWN_FUNC"


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
