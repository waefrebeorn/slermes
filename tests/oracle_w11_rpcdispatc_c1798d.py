#!/usr/bin/env python3
"""Differential oracle for w11_rpcdispatc_c1798d (tui_gateway/rpc_dispatch.py).

LIVE module loaded with sys.path.insert(0, quarry), parent-package bootstrap
and sys.modules registration. _handle_admitted_request is driven with
scripted seams that record the same ordered trace the C harness emits.
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
MOD = "tui_gateway/rpc_dispatch"


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
        sp = importlib.util.spec_from_file_location(
            mc, os.path.join(Q, "tui_gateway", "method_ctx.py"))
        m = importlib.util.module_from_spec(sp)
        sys.modules[mc] = m
        try:
            sp.loader.exec_module(m)
        except Exception:
            pass
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name, os.path.join(Q, MOD + ".py"))
    live = importlib.util.module_from_spec(sp)
    sys.modules[name] = live
    sp.loader.exec_module(live)
    return live


live = _load_live()

CB = _stub("hermes_cli.config_backend")


class ProfileUnavailableError(Exception):
    pass


class SessionNotOwned(Exception):
    pass


class ConfigBackendUnavailable(Exception):
    def __init__(self, code=""):
        super().__init__(str(code))
        self.code = code


CB.ConfigBackendUnavailable = ConfigBackendUnavailable
live.ProfileUnavailableError = ProfileUnavailableError
live.SessionNotOwned = SessionNotOwned


def _canon(v):
    return json.loads(json.dumps(v, ensure_ascii=False))


def drive(rec):
    io = rec.get("io") or {}
    req = rec.get("req") or {}
    trace = []
    state = {}

    def event(name, args):
        trace.append({"name": name, "args": args})

    norm = io.get("normalize") or {}

    def normalize(r):
        event("normalize", {"req": copy.deepcopy(r)})
        state.update(norm)
        # A dict is an error response; otherwise _normalize_request returns
        # the (rid, method, params) tuple. The fixture encodes the tuple as
        # an object with rid/method/params keys.
        if "__response__" in norm:
            return copy.deepcopy(norm["__response__"])
        return (norm.get("rid"), norm.get("method"), norm.get("params"))

    live._normalize_request = normalize

    methods_spec = io.get("methods") or {}

    def make_handler(spec):
        def h(rid, params):
            event("handler", {"rid": rid, "method": state.get("method"),
                              "params": copy.deepcopy(params)})
            if "__raise__" in spec:
                kind = spec["__raise__"]
                if kind == "ProfileUnavailableError":
                    raise ProfileUnavailableError(spec.get("message", ""))
                if kind == "SessionNotOwned":
                    raise SessionNotOwned(spec.get("message", ""))
                if kind == "ConfigBackendUnavailable":
                    raise ConfigBackendUnavailable(spec.get("code", ""))
            return copy.deepcopy(spec.get("__response__"))
        return h

    live._methods = {m: make_handler(s) for m, s in methods_spec.items()}

    contracts_io = io.get("contracts") or {}
    method_names = contracts_io.get("methods") or []
    tmpl = contracts_io.get("unknown_message", "Unknown method: {method}")
    validate_io = contracts_io.get("validate") or {}

    def unknown_method_message(m):
        event("unknown_method_message", {"method": m})
        return tmpl.format(method=m)

    def validate_params(contract, params):
        event("validate_params", {"method": state.get("method"),
                                  "params": copy.deepcopy(params)})
        return validate_io.get("params"), validate_io.get("problem")

    def check_params_accepted(contract, params):
        event("check_params_accepted", {"method": state.get("method")})

    def check_result(contract, result):
        event("check_result", {"method": state.get("method")})

    live._contracts = types.SimpleNamespace(
        METHODS={m: True for m in method_names},
        unknown_method_message=unknown_method_message,
        validate_params=validate_params,
        check_params_accepted=check_params_accepted,
        check_result=check_result)

    def err(rid, code, msg):
        event("err", {"rid": rid, "code": code, "message": msg})
        return {"__err__": {"rid": rid, "code": code, "message": msg}}

    def sno(rid, exc):
        event("session_not_owned_err", {"rid": rid, "message": str(exc)})
        return {"__sno__": {"rid": rid, "message": str(exc)}}

    live._err = err
    live._session_not_owned_err = sno
    live._current_rpc_method = types.SimpleNamespace(
        set=lambda m: None, reset=lambda t: None)

    try:
        ret = live._handle_admitted_request(req)
    except BaseException as exc:
        ret = {"__raise__": type(exc).__name__, "message": str(exc)}
    return {"ret": _canon(ret), "trace": _canon(trace)}


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
        if _canon(got) != want:
            mism += 1
            print("MISMATCH w11_rpcdispatc_c1798d case=%d\n  py=%s\n  c =%s" %
                  (cases, json.dumps(want, ensure_ascii=False),
                   json.dumps(_canon(got), ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w11_rpcdispatc_c1798d: %d cases, %d mismatches" % (cases, mism))
    if cases == 0:
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
