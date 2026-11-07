#!/usr/bin/env python3
"""Oracle for the five pure helpers of port_config_pure.c.

Harness (t_port_config_pure.c) emits JSONL per family:
  deep_merge: {family, base:<obj>, over:<obj>, out:<obj>}   _deep_merge(base,over)
  by_name:    {family, items:<list|"notalist">, out:<obj|null>}  _items_by_unique_name
  max_turns:  {family, config:<obj>, out:<obj>}             _normalize_max_turns_config
  strip:      {family, key, value, out:<str>}              _check_non_ascii_credential
  group:      {family, slug:<str|null>, out:<str>}         provider_group_for_slug

Ast-extract the live functions from hermes_cli/config.py + models_catalog_static.py.
_check_non_ascii_credential prints to sys.stderr when non-ascii (suppress).
Compare canonically (libjson renders integral doubles as ints).
"""
import ast
import io
import json
import sys
import contextlib

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

CFG = "C:/Users/eman5/hermes-agent/hermes_cli/config.py"
MODELS = "C:/Users/eman5/hermes-agent/hermes_cli/models_catalog_static.py"


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def grab(path, names, seeds=None):
    src = open(path, encoding="utf-8").read()
    ns = {"Any": object, "sys": sys, "Optional": object}
    if seeds:
        ns.update(seeds)
    wanted = set(names)
    body = [n for n in ast.parse(src).body
            if (isinstance(n, ast.FunctionDef) and n.name in wanted)
            or (isinstance(n, ast.Assign) and getattr(n.targets[0], 'id', '') in wanted)]
    exec(compile(ast.Module(body=body, type_ignores=[]), path, "exec"), ns)
    return ns


def _const(name, src, env=None):
    """Eval a module-scope assignment/AnnAssign VALUE (POST-type-annotation
    expression, so the annotation is skipped). Returns the value; env seeds
    names the expression references."""
    for n in ast.parse(src).body:
        tgt = (n.targets[0] if isinstance(n, ast.Assign)
               else (n.target if isinstance(n, ast.AnnAssign) else None))
        if getattr(tgt, "id", "") == name:
            return eval(compile(ast.Expression(n.value), name, "eval"),
                        (env or {}))
    raise KeyError(name)


def build_slug_map(models_src):
    """_SLUG_TO_GROUP is a dictcomp over PROVIDER_GROUPS; eval both."""
    pg = _const("PROVIDER_GROUPS", models_src)
    return _const("_SLUG_TO_GROUP", models_src, {"PROVIDER_GROUPS": pg})


def main() -> int:
    cfg = grab(CFG, ["_deep_merge", "_items_by_unique_name",
                     "_normalize_max_turns_config", "_check_non_ascii_credential"])
    models_src = open(MODELS, encoding="utf-8").read()
    slug_map = build_slug_map(models_src)

    def provider_group_for_slug(slug):
        return slug_map.get(str(slug or "").strip().lower(), "")

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
        total += 1
        fam = c.get("family")
        if fam == "deep_merge":
            want = cfg["_deep_merge"](c.get("base"), c.get("over"))
            got = c.get("out")
            if _canon(want) != _canon(got):
                mism += 1
                print(f"MISMATCH deep_merge {c.get('case')} C={got} PY={want}")
        elif fam == "by_name":
            items = c.get("items")
            if not isinstance(items, list):
                got = c.get("out")
                want = cfg["_items_by_unique_name"](items)
                if _canon(want) != _canon(got):
                    mism += 1; print(f"MISMATCH by_name {c.get('case')}")
            else:
                want = cfg["_items_by_unique_name"](items)
                got = c.get("out")
                if _canon(want) != _canon(got):
                    mism += 1; print(f"MISMATCH by_name {c.get('case')} C={got} PY={want}")
        elif fam == "max_turns":
            want = cfg["_normalize_max_turns_config"](c.get("config"))
            got = c.get("out")
            if _canon(want) != _canon(got):
                mism += 1
                print(f"MISMATCH max_turns {c.get('case')} C={got} PY={want}")
        elif fam == "strip":
            buf = io.StringIO()
            with contextlib.redirect_stderr(buf):
                want = cfg["_check_non_ascii_credential"](c.get("key"), c.get("value"))
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH strip {c.get('case')} C={got!r} PY={want!r}")
        elif fam == "group":
            want = provider_group_for_slug(c.get("slug"))
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH group {c.get('case')} slug={c.get('slug')!r} C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE config_pure: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE config_pure: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
