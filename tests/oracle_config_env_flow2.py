#!/usr/bin/env python3
"""Oracle for config_env_flow2.c — hermes_cli/web_routers/config_env.py UNCLAIMED
remainder (LIVE authority: the quarry hermes_cli/web_routers/config_env.py).

Drives the LIVE defs with SCRIPTED I/O. The 17 ported functions are AST-extracted
from the quarry (no heavy hermes_cli.web_routers.config_env import). The CLAIMED
siblings (_api_key_display, _custom_endpoint_id, _config_api_key_is_env_ref) and
every foreign-module collaborator are replaced by recording stubs returning the
SAME scripted results the C harness feeds its universal seam. Internal composition
(_endpoint_row -> _endpoint_api_mode, _detach_main_model_from_provider ->
_model_names_provider, _apply_endpoint_key -> _store_endpoint_key) runs REAL on
both sides. Compares the ordered DECISION SEQUENCE + return value + post-hoc
mutated state with ordered-dict comparison (insertion order is semantic).
Per-case BaseException catch (SystemExit / KeyboardInterrupt / GeneratorExit
escape every except-Exception / except-ValueError site and are compared as raise
specs; scripted raises round-trip bare; live HTTPException raises project to
{"__raise__": "HTTPException", "msg": detail, "status_code": code}).

Input: stdin JSON lines from t_port_config_env_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":...}}
Summary: ORACLE config_env_flow2: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs/constants or zero cases were compared.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam: <name>"};
a seams[name] ARRAY is a per-call queue (last value repeats, empty array -> None); a
single value is a constant; a scripted null is None (NULL in C);
{"__raise__": cls, "msg": ...} scripts an exception.

Fixture domain (kept identical on both sides): ASCII text inputs; POSIX lexical paths;
integral JSON numbers read as int; carrier dicts are JSON objects (non-object carriers
raise the live AttributeError at first touch); body doubles expose exactly the attributes
the live body reads (api_key str or None); find_provider_entry / _api_key_display scripts
are 2-element arrays (tuple projection); env_reference_owners blocks use the
{"__own__": true} identity marker (own is a dict or None; a null block is the None
singleton, identical to own when own is None); in-function imports resolve silently
(import failure outside the domain); _env_write_errors's with-block body projects to a
scripted body_raise spec (null = normal completion).
"""
import ast
import builtins
import copy
import json
import os
import re
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env runner: derive home from __file__ (repo is parent.parent).
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "ce3_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "web_routers", "config_env.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __import__("__future__").annotations.compiler_flag

# func key -> live name (the 17 UNCLAIMED defs).
FUNCS = {
    "provider_card": "_provider_card",
    "endpoint_api_mode": "_endpoint_api_mode",
    "endpoint_row": "_endpoint_row",
    "model_names_provider": "_model_names_provider",
    "resolve_custom_endpoint_entry": "_resolve_custom_endpoint_entry",
    "raw_provider_api_key": "_raw_provider_api_key",
    "pop_legacy_custom_provider": "_pop_legacy_custom_provider",
    "detach_main_model_from_provider": "_detach_main_model_from_provider",
    "drop_stale_addresses": "_drop_stale_addresses",
    "store_endpoint_key": "_store_endpoint_key",
    "apply_endpoint_key": "_apply_endpoint_key",
    "save_env_credential": "_save_env_credential",
    "drop_carried_route_state": "_drop_carried_route_state",
    "no_models_probe_message": "_no_models_probe_message",
    "auto_api_mode": "_auto_api_mode",
    "env_write_errors": "_env_write_errors",
    "record_custom_endpoint_setup": "_record_custom_endpoint_setup",
}
TOP_DEFS = list(FUNCS.values())
# mutation flows: the carrier input key whose post-hoc state is compared
MUT_CARRIER = {
    "pop_legacy_custom_provider": "cfg",
    "detach_main_model_from_provider": "cfg",
    "drop_stale_addresses": "entry",
    "store_endpoint_key": "entry",
    "apply_endpoint_key": "entry",
}
CONST_FILES = [
    (os.path.join(QUARRY, "hermes_cli", "config_providers.py"),
     ["_CAMEL_ALIASES", "ENDPOINT_ADDRESS_FIELDS"]),
    (os.path.join(QUARRY, "hermes_cli", "config.py"), ["_ENV_REF_RE"]),
    (os.path.join(QUARRY, "hermes_cli", "web_routers", "_common.py"),
     ["REDACTED_CREDENTIAL_WRITE_DETAIL"]),
]

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}


# ── the fastapi HTTPException double (Exception subclass; status_code + detail) ──

class HTTPException(Exception):
    def __init__(self, status_code, detail=None):
        self.status_code = status_code
        self.detail = detail
        super().__init__(detail)


# ── exception construction ──

def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls == "HTTPException":
        return HTTPException(spec.get("status_code", 500), msg)
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        if cls in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            # these builtins' __init__ demands 5 args; bypass so str(e) == msg
            e = b.__new__(b)
            Exception.__init__(e, msg)
            return e
        if cls in ("BaseExceptionGroup", "ExceptionGroup"):
            # the group ctor demands (msg, exceptions); BaseExceptionGroup with
            # all-Exception subs AUTO-RETURNS an ExceptionGroup (an Exception!),
            # so a BaseException-only sub keeps the BaseExceptionGroup kind.
            sub = SystemExit("sub") if cls == "BaseExceptionGroup" else Exception("sub")
            return b(msg, [sub])
        return b(msg)  # REAL builtins — the BaseException trio must propagate
    return type(cls, (Exception,), {})(msg)


def project_exc(e):
    for exc, spec in CUR["raised"]:
        if e is exc:
            return spec  # scripted raise round-trips bare (key order included)
    if isinstance(e, HTTPException):
        return {"__raise__": "HTTPException", "msg": e.detail,
                "status_code": e.status_code}
    return {"__raise__": type(e).__name__, "msg": str(e)}


def seam(name, args):
    # deep-copy args NOW: the ported flow may mutate a carrier AFTER the seam call
    # (entry into dashboard_endpoint_key_env, merged into drop_carried_route_state);
    # the trace event must show the call-time snapshot, exactly like the C copy.
    args = json.loads(json.dumps({k: v for k, v in args.items() if v is not None}))
    scripts = CUR["scripts"]
    if name not in scripts:
        # FAIL-CLOSED: missing script -> RuntimeError raise spec (presence test,
        # NOT .get() is None — a scripted null is a valid None result).
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = json.loads(json.dumps(r))  # deep copy: live mutation cannot corrupt script/trace
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


# ── normalization / canonical comparison ──

def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", o)


# ── the module logger double (_log.exception is a seam) ──

LOG_REC = types.SimpleNamespace(
    exception=lambda msg: seam("_log.exception", {"log_msg": msg}),
    info=lambda *a, **k: None,
)


# ── extraction ──

def _exec_node(node, ns, filename):
    exec(compile(ast.Module(body=[node], type_ignores=[]), filename, "exec",
                 flags=ANN_FLAG), ns)


def extract_consts_into(ns, path, names):
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            if not any(t.id in names for t in node.targets):
                continue
        elif isinstance(node, ast.AnnAssign):
            if not (node.value and isinstance(node.target, ast.Name)
                    and node.target.id in names):
                continue
        else:
            continue
        _exec_node(node, ns, path)


def extract():
    mod = types.ModuleType("config_env_flow2_extracted")
    sys.modules["config_env_flow2_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "config_env_flow2_extracted",
        "json": json, "os": os, "sys": sys, "re": re, "types": types,
        "logging": types.SimpleNamespace(getLogger=lambda name: LOG_REC),
        "Any": object, "Optional": object, "Dict": dict, "List": list,
        "Tuple": tuple, "Union": object,
    })
    # cross-module constants FIRST (the module's own _ORIGIN_BOUND_FIELDS reads
    # _CAMEL_ALIASES; the ported defs read ENDPOINT_ADDRESS_FIELDS / _ENV_REF_RE /
    # REDACTED_CREDENTIAL_WRITE_DETAIL at call time).
    for path, names in CONST_FILES:
        extract_consts_into(ns, path, names)

    tree = ast.parse(SRC_TEXT)

    def seed_consts():
        for node in tree.body:
            if isinstance(node, ast.Assign):
                if not all(isinstance(t, ast.Name) for t in node.targets):
                    continue
            elif isinstance(node, ast.AnnAssign):
                if not (node.value and isinstance(node.target, ast.Name)):
                    continue
            else:
                continue
            try:
                _exec_node(node, ns, SRC)
            except Exception:
                pass  # late()/APIRouter assigns: not part of the ported domain

    seed_consts()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_DEFS:
            if node.name == "_env_write_errors":
                # strip @contextlib.contextmanager: drive the RAW generator
                # (next -> throw/close) to model the with-block exit exactly.
                node = copy.deepcopy(node)
                node.decorator_list = []
            _exec_node(node, ns, SRC)
    seed_consts()  # re-seed: the _log Assign must not clobber the recorder

    missing = [n for n in TOP_DEFS if n not in ns]
    if missing:
        print("ORACLE config_env_flow2: FATAL missing defs: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    for k in ("_DESKTOP_API_MODES", "ENDPOINT_ADDRESS_FIELDS", "_ENV_REF_RE",
              "REDACTED_CREDENTIAL_WRITE_DETAIL"):
        if k not in ns:
            print("ORACLE config_env_flow2: FATAL missing constant: %s" % k,
                  file=sys.stderr)
            sys.exit(2)
    return ns


def _tuple2(r):
    # live unpacks `a, b = seam(...)`; a 2-list unpacks identically, but return a
    # tuple so the projection matches the live return shape verbatim.
    return tuple(r) if isinstance(r, list) else r


def install_stubs(ns):
    # the live module's `from fastapi import HTTPException` — the double class
    ns["HTTPException"] = HTTPException
    # ── module-level collaborators (imported at the top of the live module) ──
    ns["find_provider_entry"] = lambda providers, key: _tuple2(
        seam("find_provider_entry", {"providers": providers, "key": key}))
    ns["coerce_provider_id"] = lambda value: seam("coerce_provider_id", {"value": value})
    ns["read_raw_config"] = lambda: seam("read_raw_config", {})
    ns["_canonical_api_mode"] = lambda api_mode: seam("_canonical_api_mode",
                                                      {"api_mode": api_mode})
    ns["remove_env_value"] = lambda key: seam("remove_env_value", {"key": key})
    ns["is_redacted_credential_preview"] = lambda submitted: seam(
        "is_redacted_credential_preview", {"submitted": submitted})
    # ── CLAIMED siblings (seamed, never re-ported) ──
    ns["_api_key_display"] = lambda entry: _tuple2(seam("_api_key_display",
                                                        {"entry": entry}))
    ns["_custom_endpoint_id"] = lambda raw, fallback="custom": seam(
        "_custom_endpoint_id", {"raw": raw})
    ns["_config_api_key_is_env_ref"] = lambda endpoint_id: seam(
        "_config_api_key_is_env_ref", {"endpoint_id": endpoint_id})

    # ── in-function imports via sys.modules stubs (every intermediate package) ──
    hc = types.ModuleType("hermes_cli")
    hc.__path__ = []
    cfg = types.ModuleType("hermes_cli.config")
    cfg.load_env = lambda: seam("load_env", {})
    cfg.read_raw_config = ns["read_raw_config"]
    cl = types.ModuleType("hermes_cli.credential_lifecycle")
    cl.save_provider_env_credential = lambda env_var, value: seam(
        "save_provider_env_credential", {"env_var": env_var, "value": value})
    obs = types.ModuleType("hermes_cli.observability")
    obs.__path__ = []
    sms = types.ModuleType("hermes_cli.observability.shared_metrics_setup")

    def record_api_key_saved(env_var, value, previous, surface, *, connecting=False):
        return seam("record_api_key_saved",
                    {"env_var": env_var, "value": value, "previous": previous,
                     "surface": surface, "connecting": connecting})

    def record_provider_setup_done(surface, provider, *, hermes_home=None,
                                   background=False):
        return seam("record_provider_setup_done",
                    {"surface": surface, "provider": provider,
                     "hermes_home": hermes_home, "background": background})

    sms.record_api_key_saved = record_api_key_saved
    sms.web_setup_surface = lambda: seam("web_setup_surface", {})
    sms.record_provider_setup_done = record_provider_setup_done
    crg = types.ModuleType("hermes_cli.config_route_guard")
    crg.drop_carried_route_state = lambda old, new, config: seam(
        "drop_carried_route_state", {"old": old, "new": new, "config": config})
    rp = types.ModuleType("hermes_cli.runtime_provider")
    rp._detect_api_mode_for_url = lambda base_url: seam(
        "_detect_api_mode_for_url", {"base_url": base_url})
    csr = types.ModuleType("hermes_cli.config_secret_refs")

    def store_secret(env_name, value, base_url="", own_provider=None):
        return seam("store_secret", {"env_name": env_name, "value": value,
                                     "base_url": base_url, "own_provider": own_provider})

    def env_reference_owners(cfg_doc, env_name):
        r = seam("env_reference_owners", {"cfg": cfg_doc, "env_name": env_name})
        if not isinstance(r, list):
            return r  # live `for block in r` raises TypeError; C guard matches
        # materialize the identity marker into the SAME object live computes as own
        sk = CUR["in"].get("stored_key")
        own = (cfg_doc["providers"].get(sk)
               if isinstance(cfg_doc.get("providers"), dict) else None)
        out = []
        for b in r:
            if isinstance(b, dict) and b.get("__own__") is True:
                out.append(own)
            elif b is None and own is None:
                out.append(None)  # the None singleton is identical to itself
            else:
                out.append(b)     # fresh object: never `is` own
        return out

    csr.store_secret = store_secret
    csr.dashboard_endpoint_key_env = lambda entry, endpoint_id: seam(
        "dashboard_endpoint_key_env", {"entry": entry, "endpoint_id": endpoint_id})
    csr.env_reference_owners = env_reference_owners
    hc.config = cfg
    hc.credential_lifecycle = cl
    hc.observability = obs
    obs.shared_metrics_setup = sms
    hc.config_route_guard = crg
    hc.runtime_provider = rp
    hc.config_secret_refs = csr
    sys.modules["hermes_cli"] = hc
    sys.modules["hermes_cli.config"] = cfg
    sys.modules["hermes_cli.credential_lifecycle"] = cl
    sys.modules["hermes_cli.observability"] = obs
    sys.modules["hermes_cli.observability.shared_metrics_setup"] = sms
    sys.modules["hermes_cli.config_route_guard"] = crg
    sys.modules["hermes_cli.runtime_provider"] = rp
    sys.modules["hermes_cli.config_secret_refs"] = csr


# ── drive ──

def drive_env_write_errors(ns, inp):
    g = ns["_env_write_errors"](inp.get("log_msg"))
    next(g)  # run to the yield (the with-block body starts)
    spec = inp.get("body_raise")
    if isinstance(spec, dict) and "__raise__" in spec:
        e = build_exc(spec)
        CUR["raised"].append((e, spec))
        try:
            g.throw(e)
        except BaseException as caught:
            return project_exc(caught)
        return None  # unreachable: every exit path re-raises
    g.close()        # normal body completion
    return None


def drive(ns, func, inp):
    state = None
    try:
        if func == "provider_card":
            d = types.SimpleNamespace(**(inp.get("d") or {}))
            desc = inp.get("description", "")
            url = inp.get("url")
            pw = inp.get("is_password", False)
            adv = inp.get("advanced", False)
            ret = ns["_provider_card"](d, desc, url, is_password=pw, advanced=adv)
        elif func == "endpoint_api_mode":
            ret = ns["_endpoint_api_mode"](inp.get("entry"))
        elif func == "endpoint_row":
            eid = inp.get("endpoint_id")
            name = inp.get("name")
            base_url = inp.get("base_url")
            model = inp.get("model")
            models = inp.get("models")
            ctx_len = inp.get("context_length")
            disc = inp.get("discover_models", False)
            key_entry = inp.get("key_entry")
            is_cur = inp.get("is_current", False)
            source = inp.get("source")
            ret = ns["_endpoint_row"](eid, name, base_url, model, models, ctx_len,
                                      disc, key_entry, is_cur, source)
        elif func == "model_names_provider":
            ret = ns["_model_names_provider"](inp.get("model_cfg"),
                                              inp.get("provider_key"),
                                              inp.get("entry"))
        elif func == "resolve_custom_endpoint_entry":
            ret = ns["_resolve_custom_endpoint_entry"](inp.get("providers"),
                                                       inp.get("endpoint_id"))
        elif func == "raw_provider_api_key":
            ret = ns["_raw_provider_api_key"](inp.get("endpoint_id"))
        elif func in MUT_CARRIER:
            carrier = copy.deepcopy(inp.get(MUT_CARRIER[func]))
            state = carrier  # post-hoc: live mutation (even partial, on raise) is visible
            if func == "pop_legacy_custom_provider":
                ret = ns["_pop_legacy_custom_provider"](carrier, inp.get("provider_key"))
            elif func == "detach_main_model_from_provider":
                ret = ns["_detach_main_model_from_provider"](carrier,
                                                             inp.get("provider_key"),
                                                             inp.get("entry"))
            elif func == "drop_stale_addresses":
                ret = ns["_drop_stale_addresses"](carrier)
            elif func == "store_endpoint_key":
                ret = ns["_store_endpoint_key"](carrier, inp.get("env_var"),
                                                inp.get("key"), inp.get("base_url"),
                                                inp.get("own_provider"))
            elif func == "apply_endpoint_key":
                body = types.SimpleNamespace(**(inp.get("body") or {}))
                ret = ns["_apply_endpoint_key"](carrier, body, inp.get("endpoint_id"),
                                                inp.get("base_url"),
                                                inp.get("stored_key"))
        elif func == "save_env_credential":
            ret = ns["_save_env_credential"](inp.get("key"), inp.get("value"),
                                             inp.get("provider_setup", False))
        elif func == "drop_carried_route_state":
            existing = copy.deepcopy(inp.get("existing"))
            merged = copy.deepcopy(inp.get("merged"))
            ret = ns["_drop_carried_route_state"](existing, merged)
        elif func == "no_models_probe_message":
            # faithful resp double: a dict becomes a SimpleNamespace carrying exactly
            # its keys (headers present iff the key is); null/scalar passes VERBATIM so
            # live resp.headers raises the real NoneType/str AttributeError (the C
            # guard models the same first-touch). Domain: dict-with-headers or null.
            r_in = inp.get("resp")
            if isinstance(r_in, dict):
                resp = types.SimpleNamespace(**r_in)
                resp.json = lambda: seam("resp_json", {})
            else:
                resp = r_in
            ret = ns["_no_models_probe_message"](resp, inp.get("base_url"))
        elif func == "auto_api_mode":
            ret = ns["_auto_api_mode"](inp.get("base_url"))
        elif func == "env_write_errors":
            ret = drive_env_write_errors(ns, inp)
        elif func == "record_custom_endpoint_setup":
            ret = ns["_record_custom_endpoint_setup"](inp.get("home"))
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:  # SystemExit included — fail-closed per case
        ret = project_exc(e)
    return ret, state


def main():
    ns = extract()
    install_stubs(ns)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        if func not in FUNCS:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        ret, state = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"], "state": norm(state)}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE config_env_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE config_env_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()