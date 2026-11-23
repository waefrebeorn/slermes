#!/usr/bin/env python3
"""Differential oracle for w11_methodsi18_f41712 (tui_gateway/methods_i18n.py).

LIVE module is imported with sys.path.insert(0, quarry), sys.modules
registration and parent-package bootstrap. External collaborators
(agent.i18n, agent.i18n_layers, hermes_cli.plugins) are registered stubs
driven by each fixture line; _ok/_err are recording stubs so the decision
becomes the same marker dict the C harness emits.
"""
import copy
import importlib.util
import json
import os
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
MOD = "tui_gateway/methods_i18n"


def _stub(fullname):
    """Register an empty module (and its parents) in sys.modules."""
    parts = fullname.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            m = types.ModuleType(pn)
            m.__path__ = [os.path.join(Q, *parts[:i])]
            sys.modules[pn] = m
    if fullname not in sys.modules:
        m = types.ModuleType(fullname)
        sys.modules[fullname] = m
    return sys.modules[fullname]


def _load_live():
    parts = MOD.split("/")
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i])
        if pname in sys.modules:
            continue
        pdir = os.path.join(Q, *parts[:i])
        init = os.path.join(pdir, "__init__.py")
        sp = importlib.util.spec_from_file_location(
            pname, init, submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp)
        sys.modules[pname] = pm
        try:
            sp.loader.exec_module(pm)
        except Exception:
            pass
    # method_ctx is dependency-light; load the real one so decorators behave.
    mc = "tui_gateway.method_ctx"
    if mc not in sys.modules:
        sp = importlib.util.spec_from_file_location(
            mc, os.path.join(Q, "tui_gateway", "method_ctx.py"))
        m = importlib.util.module_from_spec(sp)
        sys.modules[mc] = m
        try:
            sp.loader.exec_module(m)
        except Exception:
            pass
    I18N = _stub("agent.i18n")
    LAYERS = _stub("agent.i18n_layers")
    PLUGINS = _stub("hermes_cli.plugins")
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name, os.path.join(Q, MOD + ".py"))
    live = importlib.util.module_from_spec(sp)
    sys.modules[name] = live
    sp.loader.exec_module(live)
    return live, I18N, LAYERS, PLUGINS


live, I18N, LAYERS, PLUGINS = _load_live()
HANDLERS = dict(live._registry._pending)
I18N_ERR = live._I18N_ERR


def _canon(v):
    return json.loads(json.dumps(v, ensure_ascii=False))


def drive(rec):
    fn_name = rec.get("func")
    if fn_name == "constants":
        return str(I18N_ERR)
    fail = rec.get("fail") or ""
    rid = rec.get("rid", "")
    live._ok = lambda r, p: {"kind": "ok", "rid": r, "payload": p}
    live._err = lambda r, c, m: {"kind": "err", "rid": r, "code": c, "message": m}
    PLUGINS.discover_plugins = lambda: None

    if fn_name == "languages":
        def language_options():
            if fail:
                raise RuntimeError(fail)
            return copy.deepcopy(rec.get("languages") or [])
        I18N.language_options = language_options
        return HANDLERS["i18n.languages"](rid, {})

    if fn_name == "catalog":
        surfaces = tuple(rec.get("surfaces") or [])
        LAYERS.CORE_SURFACE = rec.get("core", "")
        LAYERS.SURFACES = surfaces
        I18N.resolve_language_id = lambda _lang: rec.get("resolve") or ""
        def surface_catalog(lang, surface):
            if fail:
                raise RuntimeError(fail)
            return copy.deepcopy(rec.get("messages"))
        I18N.surface_catalog = surface_catalog
        return HANDLERS["i18n.catalog"](rid, rec.get("params") or {})

    return {"kind": "unknown"}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception:
            continue
        cases += 1
        got = rec.get("out")
        try:
            want = drive(rec)
        except BaseException as exc:
            mism += 1
            print("MISMATCH oracle-drive %s: %s" % (type(exc).__name__, exc), file=sys.stderr)
            continue
        if _canon(got) != _canon(want):
            mism += 1
            print("MISMATCH w11_methodsi18_f41712 case=%d\n  py=%s\n  c =%s" %
                  (cases, json.dumps(_canon(want), ensure_ascii=False),
                   json.dumps(_canon(got), ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w11_methodsi18_f41712: %d cases, %d mismatches" % (cases, mism))
    if cases == 0:
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
