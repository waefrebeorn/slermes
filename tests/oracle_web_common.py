#!/usr/bin/env python3
"""Oracle for plugins/web/_common.py — web-search result shapes, origin-HTTP
status classifiers and picker schemas.

The harness (t_port_web_common.c) emits one JSON line per case with a "fn"
discriminator; "out" is the serialized C result. The oracle AST-extracts the
real Python functions (the module's httpx / agent imports are not importable
here) and compares parsed JSON.
"""
import ast
import json
import logging
import re
import sys
from types import SimpleNamespace
from typing import Any, Awaitable, Callable, Dict, List, Optional

SRC = "C:/Users/eman5/hermes-agent/plugins/web/_common.py"

KEEP_FUNCS = {
    "search_ok", "search_fail", "web_hit", "title_hit", "document", "page_error",
    "extract_fail", "is_origin_http_error", "origin_http_error", "exa_crawl_status",
    "keenable_origin_error", "with_origin_status", "vendor_error_entry",
    "titled_rows", "setup_schema", "keyless_variant_schema", "_extract_interrupted",
}
KEEP_CONSTS = {
    "_ORIGIN_HTTP_ERROR_RE", "_EXA_CRAWL_STATUS_RE", "_KEENABLE_ORIGIN_MESSAGE_RE",
    "SEARCH_LIMIT_CAP",
}


def build_ns():
    ns = {
        "json": json, "logging": logging, "re": re,
        "Any": Any, "Optional": Optional, "Dict": Dict, "List": List,
        "Callable": Callable, "Awaitable": Awaitable,
    }
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    nodes = []
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in KEEP_FUNCS:
            nodes.append(n)
        elif isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) \
                and n.targets[0].id in KEEP_CONSTS:
            nodes.append(n)
    exec(compile(ast.fix_missing_locations(
        ast.Module(body=nodes, type_ignores=[])), SRC, "exec"), ns)
    return ns


def main() -> int:
    # The harness emits raw UTF-8 (the keyless labels carry U+00B7); the Windows
    # console codepage would otherwise decode it as two characters.
    try:
        sys.stdin.reconfigure(encoding="utf-8")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:                                      # noqa: BLE001
        pass
    ns = build_ns()
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        try:
            if fn == "search_ok":
                want = ns["search_ok"](i.get("results"))
            elif fn == "search_fail":
                want = ns["search_fail"](i.get("error"))
            elif fn == "web_hit":
                want = ns["web_hit"](i["url"], i["title"], i["description"],
                                     i["position"])
            elif fn == "title_hit":
                want = ns["title_hit"](i["title"], i["url"], i["description"],
                                       i["position"])
            elif fn == "document":
                want = ns["document"](i["url"], i["title"], i["content"],
                                      source_url=i.get("source_url"))
            elif fn == "page_error":
                want = ns["page_error"](i["url"], i["error"])
            elif fn == "extract_fail":
                want = ns["extract_fail"](i.get("urls"), i["error"])
            elif fn == "extract_interrupted":
                want = ns["_extract_interrupted"](i.get("urls"))
            elif fn == "is_origin_http_error":
                want = ns["is_origin_http_error"](i.get("error"))
            elif fn == "origin_http_error":
                want = ns["origin_http_error"](i.get("status"), i.get("reason"))
            elif fn == "exa_crawl_status":
                want = ns["exa_crawl_status"](i.get("text"))
            elif fn == "keenable_origin_error":
                want = ns["keenable_origin_error"](i.get("status"), i.get("body"))
            elif fn == "with_origin_status":
                want = ns["with_origin_status"](dict(i.get("entry") or {}),
                                                i.get("status"), i.get("reason"))
            elif fn == "vendor_error_entry":
                want = ns["vendor_error_entry"](
                    i["url"], status=i.get("status"), reason=i.get("reason"),
                    body=i.get("body"))
            elif fn == "titled_rows":
                want = ns["titled_rows"](i.get("rows"), i["description_key"])
            elif fn == "setup_schema":
                want = ns["setup_schema"](
                    i["name"], i["badge"], i["tag"], i.get("key_env"),
                    i.get("prompt"), i.get("url"), **(i.get("extra") or {}))
            elif fn == "keyless_variant_schema":
                want = ns["keyless_variant_schema"](
                    i["display"], i["key_env"], i["key_url"],
                    free_tag=i["free_tag"], paid_tag=i["paid_tag"])
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        got = json.loads(c["out"]) if isinstance(c["out"], str) else c["out"]
        if got != want:
            mism += 1
            if isinstance(got, dict) and isinstance(want, dict):
                keys = set(got) | set(want)
                diff = [k for k in keys if got.get(k) != want.get(k)][:5]
            else:
                diff = []
            print(f"MISMATCH {c.get('case')} ({fn}): C={c['out'][:220]} "
                  f"PY={want}  diff_keys={diff}")
    if total == 0:
        print("ORACLE web_common: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE web_common: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
