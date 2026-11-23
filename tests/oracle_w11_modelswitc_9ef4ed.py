#!/usr/bin/env python3
"""Differential oracle for w11_modelswitc_9ef4ed (tui_gateway/model_switch.py).

LIVE module loaded with sys.path.insert(0, quarry), parent-package bootstrap
and sys.modules registration. The pure composition helpers are driven from
the live module; the final response dict of _apply_model_switch is evaluated
from its real AST return expression.
"""
import ast
import inspect
import json
import os
import sys
import textwrap
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
MOD = "tui_gateway/model_switch"


def _stub(fullname):
    parts = fullname.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            m = types.ModuleType(pn)
            m.__path__ = [os.path.join(Q, *parts[:i])]
            sys.modules[pn] = m
    if fullname not in sys.modules:
        sys.modules[fullname] = types.ModuleType(fullname)
    return sys.modules[fullname]


def _load_live():
    parts = MOD.split("/")
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i])
        if pname in sys.modules:
            continue
        pdir = os.path.join(Q, *parts[:i])
        init = os.path.join(pdir, "__init__.py")
        import importlib.util
        sp = importlib.util.spec_from_file_location(
            pname, init, submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp)
        sys.modules[pname] = pm
        try:
            sp.loader.exec_module(pm)
        except Exception:
            pass
    mc = "tui_gateway.method_ctx"
    if mc not in sys.modules:
        import importlib.util
        sp = importlib.util.spec_from_file_location(
            mc, os.path.join(Q, "tui_gateway", "method_ctx.py"))
        m = importlib.util.module_from_spec(sp)
        sys.modules[mc] = m
        try:
            sp.loader.exec_module(m)
        except Exception:
            pass
    _stub("hermes_cli")
    name = ".".join(parts)
    import importlib.util
    sp = importlib.util.spec_from_file_location(name, os.path.join(Q, MOD + ".py"))
    live = importlib.util.module_from_spec(sp)
    sys.modules[name] = live
    sp.loader.exec_module(live)
    return live


live = _load_live()
live.os = os
live.logger = types.SimpleNamespace(debug=lambda *a, **k: None,
                                    warning=lambda *a, **k: None)

_APPLY_SRC = textwrap.dedent(inspect.getsource(live._apply_model_switch))
_APPLY_TREE = ast.parse(_APPLY_SRC)
_RETURN_NODES = [n for n in ast.walk(_APPLY_TREE)
                 if isinstance(n, ast.Return) and isinstance(n.value, ast.Dict)
                 and any(isinstance(k, ast.Constant) and k.value == "confirm_required"
                         for k in n.value.keys)]
if not _RETURN_NODES:
    print("FATAL oracle_w11_modelswitc_9ef4ed: live return dict not found")
    sys.exit(2)
_RETURN_EXPR = ast.Expression(body=_RETURN_NODES[-1].value)


def _apply_return(new_model, warning, one_turn, persist_global):
    ns = {"result": types.SimpleNamespace(new_model=new_model, warning_message=warning),
          "one_turn": one_turn, "persist_global": persist_global}
    return eval(compile(_RETURN_EXPR, "<live _apply_model_switch>", "eval"), ns)


def _canon(v):
    return json.loads(json.dumps(v, ensure_ascii=False))


GUARDS = _stub("hermes_cli.model_selection_guards")


def drive(rec):
    fn = rec.get("func")
    if fn == "constants":
        return {"LARGE_MODEL_NOTICE_KEY": live.LARGE_MODEL_NOTICE_KEY,
                "_RUNTIME_KEYS": list(live._RUNTIME_KEYS)}

    if fn == "lazy_info_route":
        return _canon(live._lazy_info_route({}, rec.get("override") or {}))

    if fn == "current_runtime":
        agent = types.SimpleNamespace(**(rec.get("agent") or {}))
        return _canon(list(live._current_model_runtime(agent, "")))

    if fn == "result_scope":
        return _apply_return("x", "", bool(rec.get("one_turn")),
                             bool(rec.get("persist_global")))["scope"]

    if fn == "apply_result":
        return _canon(_apply_return(rec.get("new_model", ""), rec.get("warning") or "",
                                     bool(rec.get("one_turn")),
                                     bool(rec.get("persist_global"))))

    if fn == "confirm_message":
        wm = rec.get("warning_message")
        GUARDS.selection_context_for_agent = lambda agent: None
        if wm is None:
            GUARDS.combined_selection_warning = lambda model, **kw: None
        else:
            GUARDS.combined_selection_warning = (
                lambda model, **kw: types.SimpleNamespace(message=wm))
        result = types.SimpleNamespace(
            new_model="m", target_provider="p", base_url="", api_key="",
            model_info=None, warning_message=rec.get("result_warning") or "")
        out = live._expensive_model_confirm(result, "", "", None)
        return _canon(out["warning"]) if out else None

    if fn == "switch_away_provider":
        os.environ["HERMES_TUI_PROVIDER"] = rec.get("env_provider") or ""
        live._env_model_seed = lambda: bool(rec.get("model_seed"))
        live._config_model_target = lambda: ("m", rec.get("config_target"))
        agent = object() if rec.get("has_agent") else None
        return _canon(live._switch_away_provider(
            agent, rec.get("explicit_provider") or "", rec.get("current_provider")))

    return {"error": "unknown func"}


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
            print("MISMATCH w11_modelswitc_9ef4ed case=%d\n  py=%s\n  c =%s" %
                  (cases, json.dumps(_canon(want), ensure_ascii=False),
                   json.dumps(_canon(got), ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w11_modelswitc_9ef4ed: %d cases, %d mismatches" % (cases, mism))
    if cases == 0:
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
