#!/usr/bin/env python3
"""Shadow mirror for config_env_flow2.c — a LITERAL Python transcription of the C
port (helpers, seam dispatch, queue cursors, raise-spec propagation, mutation order),
diffed against the LIVE oracle drive over every fixture, plus a GATE PROXY that pipes
the mirror's {func,in,out} envelopes into the oracle's real main() (stdin JSONL).

Code-only verification (no compile/link): catches C transcription slips the live-only
self-test cannot see. The mirror models libjson node semantics with a MISSING sentinel
(absent key == C NULL pointer; present-null == C JSON_NULL node == Python None).

Run: python tests/shadow_config_env_flow2.py
Expect: SHADOW config_env_flow2: N cases, 0 mismatches  +  gate proxy exit 0.
"""
import copy
import importlib.util
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIXTURE = HERE / "fixtures" / "config_env_flow2.jsonl"
ORACLE = HERE / "oracle_config_env_flow2.py"
PYBIN = r"C:\Users\eman5\hermes-agent\.venv\Scripts\python.exe"

MISSING = object()  # C NULL pointer from json_obj_get (absent key)


# ── ce3 helpers (literal transcription) ──

def jtruth(v):
    if v is MISSING or v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0.0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def is_obj(v):
    return isinstance(v, dict)


def is_arr(v):
    return isinstance(v, list)


def is_str(v):
    return isinstance(v, str)


def get(o, k):
    if isinstance(o, dict) and k is not None and k in o:
        return o[k]
    return MISSING


def xsf(fmt, *args):
    return fmt % args


def str_of(v):
    if v is MISSING or v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, (int, float)):
        d = float(v)
        if d == int(d) and abs(d) < 9.2233720368547758e18:
            return "%.0f" % d
        return "%.17g" % d
    if isinstance(v, str):
        return v
    return ""


def str_or_empty(v):
    return str_of(v) if jtruth(v) else ""


def type_name(v):
    if v is MISSING or v is None:
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, (int, float)):
        d = float(v)
        return "int" if (d == int(d) and abs(d) < 9.2233720368547758e18) else "float"
    if isinstance(v, str):
        return "str"
    if isinstance(v, list):
        return "list"
    if isinstance(v, dict):
        return "dict"
    return "object"


def strip(s):
    return s.strip(" \t\n\r\v\f")


def lower(s):
    return "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in s)


def mk_raise(cls, msg):
    return {"__raise__": cls, "msg": msg}


def http_exc(status, detail):
    return {"__raise__": "HTTPException", "msg": detail, "status_code": status}


def attr_err(v, attr):
    return mk_raise("AttributeError",
                    "'%s' object has no attribute '%s'" % (type_name(v), attr))


def is_raise(r):
    return isinstance(r, dict) and "__raise__" in r


def raise_class(r):
    return r.get("__raise__", "")


def raise_msg(r):
    return r.get("msg", "")


EXC_TREE = {
    "BaseExceptionGroup": "BaseException", "ExceptionGroup": "BaseExceptionGroup",
    "GeneratorExit": "BaseException", "KeyboardInterrupt": "BaseException",
    "SystemExit": "BaseException", "Exception": "BaseException",
    "ArithmeticError": "Exception", "FloatingPointError": "ArithmeticError",
    "OverflowError": "ArithmeticError", "ZeroDivisionError": "ArithmeticError",
    "AssertionError": "Exception", "AttributeError": "Exception",
    "BufferError": "Exception", "EOFError": "Exception", "ImportError": "Exception",
    "ModuleNotFoundError": "ImportError", "LookupError": "Exception",
    "IndexError": "LookupError", "KeyError": "LookupError", "MemoryError": "Exception",
    "NameError": "Exception", "UnboundLocalError": "NameError", "OSError": "Exception",
    "BlockingIOError": "OSError", "ChildProcessError": "OSError",
    "ConnectionError": "OSError", "BrokenPipeError": "ConnectionError",
    "ConnectionAbortedError": "ConnectionError", "ConnectionRefusedError": "ConnectionError",
    "ConnectionResetError": "ConnectionError", "FileExistsError": "OSError",
    "FileNotFoundError": "OSError", "InterruptedError": "OSError",
    "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "PermissionError": "OSError", "ProcessLookupError": "OSError",
    "TimeoutError": "OSError", "ReferenceError": "Exception", "RuntimeError": "Exception",
    "NotImplementedError": "RuntimeError", "RecursionError": "RuntimeError",
    "StopAsyncIteration": "Exception", "StopIteration": "Exception",
    "SyntaxError": "Exception", "IndentationError": "SyntaxError",
    "TabError": "IndentationError", "SystemError": "Exception", "TypeError": "Exception",
    "ValueError": "Exception", "UnicodeError": "ValueError",
    "UnicodeDecodeError": "UnicodeError", "UnicodeEncodeError": "UnicodeError",
    "UnicodeTranslateError": "UnicodeError", "Warning": "Exception",
    "BytesWarning": "Warning", "DeprecationWarning": "Warning",
    "EncodingWarning": "Warning", "FutureWarning": "Warning", "ImportWarning": "Warning",
    "PendingDeprecationWarning": "Warning", "ResourceWarning": "Warning",
    "RuntimeWarning": "Warning", "SyntaxWarning": "Warning", "UserWarning": "Warning",
    "HTTPException": "Exception",
}


def cls_is(c, ancestor):
    if not c:
        return False
    while c:
        if c == ancestor:
            return True
        c = EXC_TREE.get(c)
    return False


ADDRESS_FIELDS = ["base_url", "url", "api", "baseUrl"]
DESKTOP_API_MODES = ["chat_completions", "codex_responses", "anthropic_messages"]
REDACTED_DETAIL = ("Refusing to save a redacted credential preview; "
                   "re-enter the full secret to replace it.")


def env_ref_fullmatch(s):
    if s is None:
        return False
    n = len(s)
    if n < 4:
        return False
    if s[0] != "$" or s[1] != "{" or s[n - 1] != "}":
        return False
    for i in range(2, n - 1):
        if s[i] == "}":
            return False
    return True


def pop(o, k):
    if isinstance(o, dict) and k in o:
        del o[k]


def arr_without(arr, idx):
    return [copy.deepcopy(x) for i, x in enumerate(arr) if i != idx]


def remove_custom_prefix(s):
    return s[7:] if s.startswith("custom:") else s


def set_node(o, k, v):
    o[k] = copy.deepcopy(v) if (v is not MISSING and v is not None) else None


def unpack2(r):
    if not is_arr(r):
        return mk_raise("TypeError",
                        "cannot unpack non-iterable %s object" % type_name(r)), None, None
    n = len(r)
    if n != 2:
        return mk_raise("ValueError", "%s values to unpack (expected 2, got %d)"
                        % ("too many" if n > 2 else "not enough", n)), None, None
    return None, r[0], r[1]


# ── universal seam (harness script_call + ce3_call transcription) ──

class Ctx:
    def __init__(self, inp):
        self.inp = inp
        self.seams = inp.get("seams", {})
        self.trace = []
        self.counters = {}


def call(ctx, name, args):
    rec_args = {k: copy.deepcopy(v) for k, v in args.items()
                if v is not MISSING and v is not None}
    if name not in ctx.seams:
        result = mk_raise("RuntimeError", "no script for seam: %s" % name)
    else:
        sc = ctx.seams[name]
        if isinstance(sc, list):
            i = ctx.counters.get(name, 0)
            n = len(sc)
            result = copy.deepcopy(sc[min(i, n - 1)]) if n > 0 else None
            ctx.counters[name] = i + 1
        else:
            result = copy.deepcopy(sc)
    ctx.trace.append({"ev": name, "args": rec_args,
                      "ret": copy.deepcopy(result) if result is not None else None})
    return result


def arg_node(args, k, v):
    args[k] = None if (v is MISSING or v is None) else v


# ── ported functions (literal C transcription) ──

def m_provider_card(d, description, url, is_password, advanced):
    if not is_obj(d) or get(d, "slug") is MISSING:
        return mk_raise("AttributeError",
                        "'types.SimpleNamespace' object has no attribute 'slug'")
    if get(d, "label") is MISSING:
        return mk_raise("AttributeError",
                        "'types.SimpleNamespace' object has no attribute 'label'")
    o = {}
    o["provider"] = d["slug"] if is_str(get(d, "slug")) else ""
    o["provider_label"] = d["label"] if is_str(get(d, "label")) else ""
    o["description"] = description if description is not MISSING else ""
    set_node(o, "url", url)
    o["is_password"] = is_password
    o["advanced"] = advanced
    o["category"] = "provider"
    return o


def m_endpoint_api_mode(ctx, entry):
    if not is_obj(entry):
        return attr_err(entry, "get")
    am = get(entry, "api_mode")
    tr = get(entry, "transport")
    raw_node = am if jtruth(am) else (tr if jtruth(tr) else MISSING)
    raw = str_or_empty(raw_node)
    r = call(ctx, "_canonical_api_mode", {"api_mode": raw})
    if is_raise(r):
        return r
    if not is_str(r):
        return attr_err(r, "lower")
    mode = lower(r)
    return mode if mode in DESKTOP_API_MODES else ""


def m_endpoint_row(ctx, endpoint_id, name, base_url, model, models, context_length,
                   discover_models, key_entry, is_current, source):
    args = {}
    arg_node(args, "entry", key_entry)
    r = call(ctx, "_api_key_display", args)
    if is_raise(r):
        return r
    bad, has, preview = unpack2(r)
    if bad:
        return bad
    api_mode = m_endpoint_api_mode(ctx, key_entry)
    if is_raise(api_mode):
        return api_mode
    o = {}
    o["id"] = endpoint_id if endpoint_id is not MISSING else ""
    o["name"] = name if name is not MISSING else ""
    o["base_url"] = base_url if base_url is not MISSING else ""
    o["model"] = model if model is not MISSING else ""
    set_node(o, "models", models)
    o["api_mode"] = api_mode
    set_node(o, "context_length", context_length)
    o["discover_models"] = discover_models
    set_node(o, "has_api_key", has)
    set_node(o, "api_key_preview", preview)
    o["is_current"] = is_current
    o["source"] = source if source is not MISSING else ""
    return o


def m_model_names_provider(ctx, model_cfg, provider_key, entry):
    names = []
    r1 = call(ctx, "coerce_provider_id",
              {"value": provider_key if provider_key is not MISSING else ""})
    if is_raise(r1):
        return r1
    if not is_str(r1):
        return attr_err(r1, "lower")
    names.append(lower(r1))
    if is_obj(entry):
        name_val = get(entry, "name")
        args = {}
        arg_node(args, "value", name_val)
        r2 = call(ctx, "coerce_provider_id", args)
        if is_raise(r2):
            return r2
        if jtruth(r2):
            args = {}
            arg_node(args, "value", name_val)
            r3 = call(ctx, "coerce_provider_id", args)
            if is_raise(r3):
                return r3
            if not is_str(r3):
                return attr_err(r3, "lower")
            n2 = lower(r3)
            if n2 != names[0]:
                names.append(n2)
    if not is_obj(model_cfg):
        return attr_err(model_cfg, "get")
    cur = lower(strip(str_or_empty(get(model_cfg, "provider"))))
    bare = remove_custom_prefix(cur)
    return bare in names


def m_resolve_custom_endpoint_entry(ctx, providers, endpoint_id):
    args = {}
    arg_node(args, "providers", providers)
    args["key"] = endpoint_id if endpoint_id is not MISSING else ""
    r1 = call(ctx, "find_provider_entry", args)
    if is_raise(r1):
        return r1
    bad, stored, entry = unpack2(r1)
    if bad:
        return bad
    if entry is not MISSING and entry is not None:
        return [copy.deepcopy(stored) if stored is not MISSING else None,
                copy.deepcopy(entry)]
    r2 = call(ctx, "_custom_endpoint_id",
              {"raw": endpoint_id if endpoint_id is not MISSING else ""})
    if is_raise(r2):
        return r2
    if is_str(r2) and r2 == endpoint_id:
        return [None, None]
    args = {}
    arg_node(args, "providers", providers)
    set_node(args, "key", r2)
    return call(ctx, "find_provider_entry", args)


def m_raw_provider_api_key(ctx, endpoint_id):
    r = call(ctx, "read_raw_config", {})
    if is_raise(r):
        return r
    if not is_obj(r):
        return attr_err(r, "get")
    providers = get(r, "providers")
    args = {}
    arg_node(args, "providers", providers)
    args["key"] = endpoint_id if endpoint_id is not MISSING else ""
    r2 = call(ctx, "find_provider_entry", args)
    if is_raise(r2):
        return r2
    bad, stored, entry = unpack2(r2)
    if bad:
        return bad
    if is_obj(entry):
        ak = get(entry, "api_key")
        if ak is not MISSING and ak is not None:
            return copy.deepcopy(ak)
    return None


def m_pop_legacy_custom_provider(ctx, cfg, provider_key):
    if not is_obj(cfg):
        return attr_err(cfg, "get")
    legacy = get(cfg, "custom_providers")
    if not is_arr(legacy):
        return None
    for i in range(len(legacy)):
        entry = legacy[i]
        if not is_obj(entry):
            continue
        raw = str_or_empty(get(entry, "name"))
        r = call(ctx, "_custom_endpoint_id", {"raw": raw})
        if is_raise(r):
            return r
        match = is_str(r) and r == provider_key
        if match:
            popped = copy.deepcopy(entry)
            cfg["custom_providers"] = arr_without(legacy, i)
            return popped
    return None


def m_detach_main_model_from_provider(ctx, cfg, provider_key, entry):
    if not is_obj(cfg):
        return attr_err(cfg, "get")
    model_cfg = get(cfg, "model")
    if not is_obj(model_cfg):
        return None
    named = m_model_names_provider(ctx, model_cfg, provider_key, entry)
    if is_raise(named):
        return named
    if not jtruth(named):
        return None
    for f in ("provider", "base_url", "api_key", "key_env"):
        pop(model_cfg, f)
    return None


def m_drop_stale_addresses(entry):
    if not is_obj(entry):
        return attr_err(entry, "pop")
    for f in ADDRESS_FIELDS:
        if f != "base_url":
            pop(entry, f)
    return None


def m_store_endpoint_key(ctx, entry, env_var, key, base_url, own_provider):
    args = {}
    set_node(args, "env_name", env_var)
    args["value"] = key if key is not MISSING else ""
    args["base_url"] = base_url if base_url is not MISSING else ""
    arg_node(args, "own_provider", own_provider)
    r = call(ctx, "store_secret", args)
    if is_raise(r):
        if cls_is(raise_class(r), "ValueError"):
            return http_exc(400, raise_msg(r))
        return r
    if not is_obj(entry):
        return mk_raise("TypeError",
                        "'%s' object does not support item assignment" % type_name(entry))
    entry["key_env"] = copy.deepcopy(env_var) if (env_var is not MISSING and env_var is not None) else None
    pop(entry, "api_key")
    return None


def m_apply_endpoint_key(ctx, entry, body, endpoint_id, base_url, stored_key):
    args = {}
    arg_node(args, "entry", entry)
    args["endpoint_id"] = endpoint_id if endpoint_id is not MISSING else ""
    env_var = call(ctx, "dashboard_endpoint_key_env", args)
    if is_raise(env_var):
        return env_var
    if not is_obj(body) or get(body, "api_key") is MISSING:
        return mk_raise("AttributeError",
                        "'types.SimpleNamespace' object has no attribute 'api_key'")
    ak_body = get(body, "api_key")
    submitted = None
    if ak_body is not MISSING and ak_body is not None:
        if not is_str(ak_body):
            return attr_err(ak_body, "strip")
        submitted = strip(ak_body)
    ret = None
    if submitted is not None and len(submitted) > 0:
        if env_ref_fullmatch(submitted):
            ret = http_exc(400, REDACTED_DETAIL)
        else:
            r2 = call(ctx, "is_redacted_credential_preview", {"submitted": submitted})
            if is_raise(r2):
                ret = r2
            elif jtruth(r2):
                ret = http_exc(400, REDACTED_DETAIL)
            else:
                ret = m_store_endpoint_key(ctx, entry, env_var, submitted, base_url, stored_key)
    elif submitted is not None:
        raw = call(ctx, "read_raw_config", {})
        if is_raise(raw):
            ret = raw
        elif not is_obj(raw):
            ret = attr_err(raw, "get")
        else:
            providers = get(raw, "providers")
            own = MISSING
            if is_obj(providers) and is_str(stored_key):
                own = get(providers, stored_key)
            own_is_none = own is MISSING or own is None
            args = {}
            arg_node(args, "cfg", raw)
            set_node(args, "env_name", env_var)
            owners = call(ctx, "env_reference_owners", args)
            if is_raise(owners):
                ret = owners
            elif not is_arr(owners):
                ret = mk_raise("TypeError", "'%s' object is not iterable" % type_name(owners))
            else:
                any_distinct = False
                for block in owners:
                    identical = is_obj(block) and block.get("__own__") is True
                    if not identical and own_is_none and (block is MISSING or block is None):
                        identical = True
                    if not identical:
                        any_distinct = True
                        break
                if not any_distinct:
                    args = {}
                    set_node(args, "key", env_var)
                    r4 = call(ctx, "remove_env_value", args)
                    if is_raise(r4):
                        ret = r4
                if ret is None:
                    if not is_obj(entry):
                        ret = attr_err(entry, "pop")
                    else:
                        pop(entry, "key_env")
                        pop(entry, "api_key")
    else:
        if not is_obj(entry):
            ret = attr_err(entry, "get")
        else:
            ak = get(entry, "api_key")
            st = strip(str_or_empty(ak))
            if len(st) > 0:
                r2 = call(ctx, "_config_api_key_is_env_ref",
                          {"endpoint_id": endpoint_id if endpoint_id is not MISSING else ""})
                if is_raise(r2):
                    ret = r2
                elif not jtruth(r2):
                    if not is_str(ak):
                        ret = attr_err(ak, "strip")
                    else:
                        key = strip(ak)
                        ret = m_store_endpoint_key(ctx, entry, env_var, key, base_url, stored_key)
    return ret


def m_save_env_credential(ctx, key, value, provider_setup):
    r1 = call(ctx, "load_env", {})
    if is_raise(r1):
        return r1
    if not is_obj(r1):
        return attr_err(r1, "get")
    prev = get(r1, key)
    previous = copy.deepcopy(prev) if prev is not MISSING else MISSING
    args = {"env_var": key if key is not MISSING else "",
            "value": value if value is not MISSING else ""}
    result = call(ctx, "save_provider_env_credential", args)
    if is_raise(result):
        return result
    surface = call(ctx, "web_setup_surface", {})
    if is_raise(surface):
        return surface
    args = {"env_var": key if key is not MISSING else "",
            "value": value if value is not MISSING else ""}
    arg_node(args, "previous", previous)
    set_node(args, "surface", surface)
    args["connecting"] = provider_setup
    r4 = call(ctx, "record_api_key_saved", args)
    if is_raise(r4):
        return r4
    return result


def m_drop_carried_route_state(ctx, existing, merged):
    if not is_obj(existing):
        return attr_err(existing, "get")
    if not is_obj(merged):
        return attr_err(merged, "get")
    args = {}
    arg_node(args, "old", get(existing, "model"))
    arg_node(args, "new", get(merged, "model"))
    arg_node(args, "config", merged)
    r = call(ctx, "drop_carried_route_state", args)
    if is_raise(r):
        return r
    deleg = get(merged, "delegation")
    keyless_with_url = False
    if is_obj(deleg) and jtruth(get(deleg, "base_url")):
        pl = lower(strip(str_or_empty(get(deleg, "provider"))))
        keyless_with_url = pl in ("", "custom", "auto")
    if not keyless_with_url:
        args = {}
        arg_node(args, "old", get(existing, "delegation"))
        arg_node(args, "new", deleg)
        arg_node(args, "config", merged)
        r = call(ctx, "drop_carried_route_state", args)
        if is_raise(r):
            return r
    old_aux = get(existing, "auxiliary")
    new_aux = get(merged, "auxiliary")
    if is_obj(old_aux) and is_obj(new_aux):
        for task in new_aux:
            args = {}
            arg_node(args, "old", get(old_aux, task))
            arg_node(args, "new", new_aux[task])
            arg_node(args, "config", merged)
            r = call(ctx, "drop_carried_route_state", args)
            if is_raise(r):
                return r
    return None


def m_no_models_probe_message(ctx, resp, base_url):
    if not is_obj(resp):
        return attr_err(resp, "headers")
    headers = get(resp, "headers")
    if not is_obj(headers):
        return attr_err(headers, "get")
    ct_raw = get(headers, "content-type")
    if jtruth(ct_raw) and not is_str(ct_raw):
        return attr_err(ct_raw, "split")
    ct0 = str_or_empty(ct_raw)
    semi = ct0.find(";")
    if semi >= 0:
        ct0 = ct0[:semi]
    content_type = strip(ct0)
    url = (base_url if base_url is not MISSING else "") + "/models"
    payload = call(ctx, "resp_json", {})
    if is_raise(payload):
        if cls_is(raise_class(payload), "ValueError"):
            payload = None
        else:
            return payload
    if not is_obj(payload) and not is_arr(payload):
        kind = content_type if len(content_type) > 0 else "a non-JSON body"
        return ("Connected to %s, but it returned %s instead of a JSON model list "
                "\u2014 the URL may be an SPA catch-all or a wrong base URL, "
                "not an OpenAI-compatible API." % (url, kind))
    data = get(payload, "data") if is_obj(payload) else payload
    if not is_arr(data):
        return ("Connected to %s, but its response is not an OpenAI-compatible "
                "model list." % url)
    return ("Connected to %s, but it advertised no models. "
            "Start a model on that endpoint and try again." % url)


def m_auto_api_mode(ctx, base_url):
    r = call(ctx, "_detect_api_mode_for_url",
             {"base_url": base_url if base_url is not MISSING else ""})
    if is_raise(r):
        return r
    if jtruth(r):
        return r
    return "chat_completions"


def m_env_write_errors(ctx, log_msg, body_raise):
    if not is_raise(body_raise):
        return None
    cls = raise_class(body_raise)
    if cls_is(cls, "HTTPException"):
        return copy.deepcopy(body_raise)
    if cls_is(cls, "ValueError"):
        return http_exc(400, raise_msg(body_raise))
    if cls_is(cls, "Exception") or not cls_is(cls, "BaseException"):
        r = call(ctx, "_log.exception",
                 {"log_msg": log_msg if log_msg is not MISSING else ""})
        if is_raise(r):
            return r
        return http_exc(500, "Internal server error")
    return copy.deepcopy(body_raise)


def m_record_custom_endpoint_setup(ctx, home):
    surface = call(ctx, "web_setup_surface", {})
    if is_raise(surface):
        return surface
    args = {}
    set_node(args, "surface", surface)
    args["provider"] = "custom"
    arg_node(args, "hermes_home", home)
    args["background"] = True
    r = call(ctx, "record_provider_setup_done", args)
    if is_raise(r):
        return r
    return None


# ── harness transcription (input readers + dispatch + state) ──

def S(inp, k):
    v = get(inp, k)
    return v if is_str(v) else MISSING


def J(inp, k):
    return get(inp, k)


def B(inp, k):
    v = get(inp, k)
    return v if isinstance(v, bool) else False


def mirror_drive(func, inp):
    ctx = Ctx(inp)
    state = MISSING
    if func == "provider_card":
        r = m_provider_card(J(inp, "d"), S(inp, "description"), J(inp, "url"),
                            B(inp, "is_password"), B(inp, "advanced"))
    elif func == "endpoint_api_mode":
        r = m_endpoint_api_mode(ctx, J(inp, "entry"))
    elif func == "endpoint_row":
        r = m_endpoint_row(ctx, S(inp, "endpoint_id"), S(inp, "name"), S(inp, "base_url"),
                           S(inp, "model"), J(inp, "models"), J(inp, "context_length"),
                           B(inp, "discover_models"), J(inp, "key_entry"),
                           B(inp, "is_current"), S(inp, "source"))
    elif func == "model_names_provider":
        r = m_model_names_provider(ctx, J(inp, "model_cfg"), S(inp, "provider_key"),
                                   J(inp, "entry"))
    elif func == "resolve_custom_endpoint_entry":
        r = m_resolve_custom_endpoint_entry(ctx, J(inp, "providers"), S(inp, "endpoint_id"))
    elif func == "raw_provider_api_key":
        r = m_raw_provider_api_key(ctx, S(inp, "endpoint_id"))
    elif func == "pop_legacy_custom_provider":
        cfg = copy.deepcopy(J(inp, "cfg")) if J(inp, "cfg") is not MISSING else MISSING
        r = m_pop_legacy_custom_provider(ctx, cfg, S(inp, "provider_key"))
        state = cfg
    elif func == "detach_main_model_from_provider":
        cfg = copy.deepcopy(J(inp, "cfg")) if J(inp, "cfg") is not MISSING else MISSING
        r = m_detach_main_model_from_provider(ctx, cfg, S(inp, "provider_key"), J(inp, "entry"))
        state = cfg
    elif func == "drop_stale_addresses":
        entry = copy.deepcopy(J(inp, "entry")) if J(inp, "entry") is not MISSING else MISSING
        r = m_drop_stale_addresses(entry)
        state = entry
    elif func == "store_endpoint_key":
        entry = copy.deepcopy(J(inp, "entry")) if J(inp, "entry") is not MISSING else MISSING
        r = m_store_endpoint_key(ctx, entry, J(inp, "env_var"), S(inp, "key"),
                                 S(inp, "base_url"), J(inp, "own_provider"))
        state = entry
    elif func == "apply_endpoint_key":
        entry = copy.deepcopy(J(inp, "entry")) if J(inp, "entry") is not MISSING else MISSING
        r = m_apply_endpoint_key(ctx, entry, J(inp, "body"), S(inp, "endpoint_id"),
                                 S(inp, "base_url"), J(inp, "stored_key"))
        state = entry
    elif func == "save_env_credential":
        r = m_save_env_credential(ctx, S(inp, "key"), S(inp, "value"), B(inp, "provider_setup"))
    elif func == "drop_carried_route_state":
        r = m_drop_carried_route_state(ctx, J(inp, "existing"), J(inp, "merged"))
    elif func == "no_models_probe_message":
        r = m_no_models_probe_message(ctx, J(inp, "resp"), S(inp, "base_url"))
    elif func == "auto_api_mode":
        r = m_auto_api_mode(ctx, S(inp, "base_url"))
    elif func == "env_write_errors":
        r = m_env_write_errors(ctx, S(inp, "log_msg"), J(inp, "body_raise"))
    elif func == "record_custom_endpoint_setup":
        r = m_record_custom_endpoint_setup(ctx, J(inp, "home"))
    else:
        raise ValueError("unknown func: %s" % func)
    return {"ret": None if r is MISSING else r,
            "trace": ctx.trace,
            "state": None if state is MISSING else state}


# ── diff vs the live oracle drive + gate proxy ──

def load_oracle():
    spec = importlib.util.spec_from_file_location("oracle_ce3_shadow", str(ORACLE))
    oc = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(oc)
    ns = oc.extract()
    oc.install_stubs(ns)
    return oc, ns


def main():
    oc, ns = load_oracle()
    cases = [json.loads(l) for l in open(FIXTURE, encoding="utf-8") if l.strip()]
    mism = 0
    envelopes = []
    for i, c in enumerate(cases):
        func, inp = c["func"], c["in"]
        m_out = mirror_drive(func, inp)
        oc.CUR["in"] = inp
        oc.CUR["trace"] = []
        oc.CUR["scripts"] = inp.get("seams", {})
        oc.CUR["idx"] = {}
        oc.CUR["raised"] = []
        ret, state = oc.drive(ns, func, inp)
        want = {"ret": oc.norm(ret), "trace": oc.CUR["trace"], "state": oc.norm(state)}
        if oc.canon(m_out) != oc.canon(want):
            mism += 1
            if mism <= 8:
                print("SHADOW MISMATCH #%d %s" % (i, func))
                print("  C-mirror: %s" % json.dumps(m_out, ensure_ascii=False, default=str))
                print("  PY-live : %s" % json.dumps(want, ensure_ascii=False, default=str))
        envelopes.append({"func": func, "in": inp, "out": m_out})
    print("SHADOW config_env_flow2: %d cases, %d mismatches" % (len(cases), mism))
    # GATE PROXY: the mirror's envelopes through the oracle's REAL main()
    proxy = HERE.parent / ".testbin" / "ce3_shadow_proxy.jsonl"
    proxy.parent.mkdir(exist_ok=True)
    with open(proxy, "w", encoding="utf-8", newline="\n") as f:
        for e in envelopes:
            f.write(json.dumps(e, ensure_ascii=True) + "\n")
    p = subprocess.run([PYBIN, str(ORACLE)], stdin=open(proxy, encoding="utf-8"),
                       capture_output=True, text=True, encoding="utf-8")
    print("GATE PROXY rc=%d: %s" % (p.returncode, p.stdout.strip().splitlines()[-1]
                                    if p.stdout.strip() else "(no output)"))
    if p.returncode != 0:
        print(p.stdout[-3000:])
        print(p.stderr[-2000:])
    sys.exit(1 if (mism or p.returncode) else 0)


if __name__ == "__main__":
    main()