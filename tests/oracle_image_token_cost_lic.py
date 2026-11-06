#!/usr/bin/env python3
"""Oracle for image_token_cost::learned_image_token_cost pure core.

Pure contract: `_table().get(_key(model, base_url), 1500)`. `_key` calls
`from utils import base_url_hostname`, whose import chain pulls ruamel (absent,
poisonous), so the LIVE definitions under test — `_parse_base_url`,
`_hostname_of`, `base_url_hostname` (utils.py) and `_key` (image_token_cost.py)
— are ast-extracted verbatim and exec'd, auto-tracking source changes without
the module's third-party import surface. DEFAULT is read from the live module
(imports only stdlib at top level).

Harness emits: {"tag":"lic","model":..,"base_url":..,"table":..,"out":..}
(table is a JSON dict or null; out is a decimal cost or null).
"""
import ast
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")


def _load_def(path, name, ns):
    src = open(path, encoding="utf-8").read()
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == name:
            mod = ast.Module(body=[n], type_ignores=[])
            exec(compile(ast.fix_missing_locations(mod), path, "exec"), ns)
            return ns[name]
    raise RuntimeError(f"{name} not found in {path}")


ns = {}
_load_def("C:/Users/eman5/hermes-agent/utils.py", "_parse_base_url", ns)
_load_def("C:/Users/eman5/hermes-agent/utils.py", "_hostname_of", ns)
_load_def("C:/Users/eman5/hermes-agent/utils.py", "base_url_hostname", ns)
_load_def("C:/Users/eman5/hermes-agent/agent/image_token_cost.py", "_key", ns)
# _key does `from utils import base_url_hostname` at runtime — route that to the
# ast-extracted def instead of the real (ruamel-poisonous) module.
import types
from urllib.parse import urlparse as _urlparse
# utils.py does `from urllib.parse import urlparse` at module top — provide it
# to the ast-extracted defs.
ns["urlparse"] = _urlparse
sys.modules["utils"] = types.SimpleNamespace(base_url_hostname=ns["base_url_hostname"])

import agent.image_token_cost as ITC  # noqa: E402
DEFAULT = ITC.DEFAULT_IMAGE_TOKEN_COST


def learned(model, base_url, table):
    if not isinstance(table, dict):
        return DEFAULT
    return table.get(ns["_key"](model, base_url), DEFAULT)


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
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
        print("ORACLE image_token_cost_lic: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        if c.get("tag") != "lic":
            continue
        table = json.loads(c["table"]) if c["table"] else None
        want = learned(c["model"], c["base_url"], table)
        c_out = int(c["out"]) if c["out"] != "null" else None
        if want != c_out:
            mism += 1
            print(f"MISMATCH {c.get('case')}: C={c['out']} PY={want} "
                  f"key={ns['_key'](c['model'], c['base_url'])!r}")
    print(f"ORACLE image_token_cost_lic: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
