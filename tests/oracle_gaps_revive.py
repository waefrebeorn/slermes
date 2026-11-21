#!/usr/bin/env python3
"""Leaf differential oracle for the re-ported placeholders in
src/gateway/platforms/base_gaps.c and src/cli/cli_gaps.c.

Reads {"func","in","out"} JSON lines from t_port_gaps_revive on stdin. For
every case it AST-extracts the LIVE Python function from the quarry, executes
it against the SAME scripted seam (recording every collaborator call as
{ev,args,ret}), and compares ret / exc / trace / post-state with the C output.

Exit 0 iff cases > 0 and zero mismatches.
"""
import ast
import builtins
import copy
import json
import os
import sys
import types

for _s in (sys.stdout, sys.stdin):
    try:
        if hasattr(_s, "reconfigure"):
            _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

QUARRY = r"C:\Users\eman5\hermes-agent"
import __future__
ANN = __future__.annotations.compiler_flag

SRC = {
    "set_owner_profile": ("gateway/platforms/base.py", "set_owner_profile"),
    "set_platform_event_handler": ("gateway/platforms/base.py", "set_platform_event_handler"),
    "send_path_degraded": ("gateway/platforms/base.py", "send_path_degraded"),
    "consume_detached_handler_exception": ("gateway/platforms/base.py", "_consume_detached_handler_exception"),
    "gateway_trust_env": ("gateway/platforms/base.py", "gateway_trust_env"),
    "warn_unresolved_docker_media": ("gateway/platforms/base.py", "_warn_unresolved_docker_media"),
    "plugin_blueprints": ("cron/blueprint_plugins.py", "plugin_blueprints"),
    "load_settings": ("gateway/bot_loop_guard.py", "load_settings"),
    "cmd_delete": ("hermes_cli/moa_cmd.py", "_cmd_delete"),
}

CUR = {"trace": [], "counts": {}, "inp": {}}


def rec(ev, args, ret):
    # The C seam copies args/ret into the trace at call time; mirror that so
    # later in-place state mutation cannot rewrite recorded history.
    CUR["trace"].append({"ev": ev, "args": copy.deepcopy(args),
                         "ret": copy.deepcopy(ret)})


def consume(name):
    io = CUR["inp"].get("io") or {}
    if name not in io:
        return {"__raise__": "RuntimeError", "message": "unscripted seam: " + name}
    script = io[name]
    if isinstance(script, dict) and script.get("__script__") is True:
        return script.get("ret")
    if isinstance(script, list):
        i = CUR["counts"].get(name, 0)
        CUR["counts"][name] = i + 1
        if not script:
            return None
        return script[min(i, len(script) - 1)]
    return script


def maybe_raise(v):
    if isinstance(v, dict) and "__raise__" in v:
        kind = v.get("__raise__", "Exception")
        cls = getattr(builtins, kind, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
            cls = type(kind, (Exception,), {})
        raise cls(v.get("message", ""))


def norm(v):
    """Project a seam script value to the C harness's JSON representation."""
    if v is None:
        return None
    if isinstance(v, dict) and "__script__" in v:
        return norm(v.get("ret"))
    return v


def stub(name, argmap, raises=True, ret_override=None):
    def f(*a, **k):
        args = argmap(*a, **k)
        sc = consume(name)
        rec(name, args, norm(sc))
        if raises:
            maybe_raise(sc)
        return ret_override(name, sc, a, k) if ret_override else sc
    return f


class RecLogger:
    def _emit(self, level, msg, *args, **kwargs):
        entry = {"msg": msg}
        if args:
            entry["args"] = list(args)
        # exc_info=<exception object> is recorded by the port; a bare True is not.
        ei = kwargs.get("exc_info", None)
        if ei is not None and ei is not True:
            entry["exc_info"] = ei
        rec("log." + level, entry, None)

    def error(self, msg, *a, **k): self._emit("error", msg, *a, **k)
    def warning(self, msg, *a, **k): self._emit("warning", msg, *a, **k)
    def info(self, msg, *a, **k): self._emit("info", msg, *a, **k)
    def debug(self, msg, *a, **k): self._emit("debug", msg, *a, **k)


class RecSelf:
    """Models `self`: attribute assignments are recorded as a `setattr` seam."""
    def __setattr__(self, k, v):
        rec("setattr", {"name": k, "value": v}, None)


_FN_CACHE = {}


def live_fn(func):
    if func in _FN_CACHE:
        return _FN_CACHE[func]
    rel, name = SRC[func]
    path = os.path.join(QUARRY, rel)
    src = open(path, encoding="utf-8").read()
    tree = ast.parse(src)
    node = None
    for n in ast.walk(tree):
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
            node = n
            break
    if node is None:
        raise RuntimeError("live function not found: " + name)
    mm = copy.deepcopy(node)
    mm.decorator_list = []
    mod = ast.Module(body=[mm], type_ignores=[])
    ast.fix_missing_locations(mod)
    ns = {"__name__": "live_" + name}
    exec(compile(mod, path, "exec", ANN), ns)
    fn = ns[name]
    _FN_CACHE[func] = fn
    return fn


def build_ns(func, inp):
    ns = {"__name__": "live_" + func, "logger": RecLogger(),
          "print": stub("print", lambda text: {"text": text}, raises=False)}

    def one(name, argmap, **kw):
        return stub(name, argmap, **kw)

    if func in ("set_owner_profile", "set_platform_event_handler", "send_path_degraded"):
        pass
    if func == "gateway_trust_env":
        ns["_config_section"] = one("config_section", lambda name: {"name": name}, raises=False)
    if func == "consume_detached_handler_exception":
        ns["asyncio"] = types.SimpleNamespace()
    if func == "warn_unresolved_docker_media":
        ns["_docker_env_active"] = one("docker_env_active", lambda: {}, raises=False)
        ns["_log_safe_path"] = one("log_safe_path", lambda raw: {"path": str(raw)}, raises=False,
                                   ret_override=lambda n, sc, a, k: sc)
    if func == "plugin_blueprints":
        ns["discover_plugins"] = one("discover_plugins", lambda: {})
        def _mgr_stub():
            sc = consume("get_plugin_manager")
            rec("get_plugin_manager", {}, norm(sc))
            maybe_raise(sc)
            class Mgr:
                def list_automation_blueprints(self):
                    s = consume("list_automation_blueprints")
                    rec("list_automation_blueprints", {"manager": {"name": "mgr"}}, norm(s))
                    maybe_raise(s)
                    return s
            return Mgr()
        ns["get_plugin_manager"] = _mgr_stub
        # The live body does `from hermes_cli.plugins import ...` inside try;
        # preload a stub module so those names resolve to our recorders.
        pm = types.ModuleType("hermes_cli.plugins")
        pm.discover_plugins = ns["discover_plugins"]
        pm.get_plugin_manager = ns["get_plugin_manager"]
        sys.modules["hermes_cli.plugins"] = pm
    if func == "load_settings":
        ns["load_config_readonly"] = one("load_config_readonly", lambda: {})
        ns["settings_from_config"] = one("settings_from_config", lambda cfg: cfg)
        cm = types.ModuleType("hermes_cli.config")
        cm.load_config_readonly = ns["load_config_readonly"]
        sys.modules["hermes_cli.config"] = cm
        class BotLoopGuardSettings:
            enabled = True
            max_events = 20
            window_seconds = 300.0
            cooldown_seconds = 600.0
            max_hops = 4
            bot_replies_expect_reply = False
            def __init__(self): pass
            def __iter__(self):
                return iter({"enabled": self.enabled, "max_events": self.max_events,
                             "window_seconds": self.window_seconds,
                             "cooldown_seconds": self.cooldown_seconds,
                             "max_hops": self.max_hops,
                             "bot_replies_expect_reply": self.bot_replies_expect_reply}.items())
        ns["BotLoopGuardSettings"] = BotLoopGuardSettings
    if func == "cmd_delete":
        ns["_moa_section"] = one("moa_section", lambda cfg: {"cfg": cfg}, raises=False,
                                  ret_override=lambda n, sc, a, k: inp["cfg"]["moa"])
        def _save(cfg, moa):
            sc = consume("save")
            rec("save", {"cfg": cfg}, norm(sc))
            maybe_raise(sc)
        ns["_save"] = _save
    return ns


def setting_defaults():
    return {"enabled": True, "max_events": 20, "window_seconds": 300.0,
            "cooldown_seconds": 600.0, "max_hops": 4, "bot_replies_expect_reply": False}


def drive(func, inp):
    fn = live_fn(func)
    ns = build_ns(func, inp)
    # Inject globals into the function object itself (AST-compiled with a fresh ns).
    g = fn.__globals__
    for k, v in ns.items():
        g[k] = v
    if func in ("set_owner_profile", "set_platform_event_handler", "send_path_degraded"):
        if func == "set_owner_profile":
            fn(RecSelf(), inp.get("profile", ""))
        elif func == "set_platform_event_handler":
            fn(RecSelf(), inp.get("handler"))
        else:
            return {"ret": fn(RecSelf()), "exc": None}
        return {"ret": None, "exc": None}
    if func == "consume_detached_handler_exception":
        task = inp.get("task") or {}
        class T:
            def __init__(self, d): self._d = d
            def cancelled(self):
                sc = consume("task.cancelled"); rec("task.cancelled", {"task": self._d}, norm(sc))
                maybe_raise(sc); return bool(sc)
            def exception(self):
                sc = consume("task.exception"); rec("task.exception", {"task": self._d}, norm(sc))
                maybe_raise(sc); return sc
        fn(T(task))
        return {"ret": None, "exc": None}
    if func == "gateway_trust_env":
        return {"ret": bool(fn()), "exc": None}
    if func == "warn_unresolved_docker_media":
        fn(inp.get("candidate", ""), inp.get("session_key", ""), inp.get("reason", ""),
           quiet=inp.get("quiet", False))
        return {"ret": None, "exc": None}
    if func == "plugin_blueprints":
        return {"ret": fn(), "exc": None}
    if func == "load_settings":
        s = fn()
        try:
            d = dict(s)
        except TypeError:
            d = setting_defaults()
        return {"ret": d, "exc": None}
    if func == "cmd_delete":
        cfg = inp["cfg"]
        args = types.SimpleNamespace(name=inp.get("name"))
        try:
            fn(cfg, args)
            return {"ret": None, "exc": None, "cfg": cfg}
        except BaseException as e:
            return {"ret": None, "exc": {"__raise__": type(e).__name__, "message": str(e)},
                    "cfg": cfg}
    raise ValueError("unknown func " + func)


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    if isinstance(x, str):
        return ("str", x)
    return ("other", str(x))


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            r = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = r.get("func")
        inp = r.get("in") or {}
        got = r.get("out") or {}
        if func not in SRC:
            continue
        if func == "cmd_delete":
            # cmd_delete mutates cfg in place; the harness echoes post-state in out["state"].
            got = dict(got)
            got["cfg"] = got.pop("state", None)
        cases += 1
        CUR["trace"] = []
        CUR["counts"] = {}
        CUR["inp"] = inp
        inp = copy.deepcopy(inp)
        CUR["inp"] = inp
        try:
            want = drive(func, inp)
        except BaseException as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        want["trace"] = CUR["trace"]
        keys = set(want) | set(got)
        bad = False
        for k in keys:
            if canon(want.get(k)) != canon(got.get(k)):
                bad = True
                break
        if bad:
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE gaps_revive: %d cases, %d mismatches" % (cases, mism))
    return 0 if (cases > 0 and mism == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
