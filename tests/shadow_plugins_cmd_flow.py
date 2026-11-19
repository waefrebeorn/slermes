#!/usr/bin/env python3
"""Shadow mirror of src/cli/plugins_cmd_flow.c — a LITERAL Python transcription of
the C algorithms (value-or-raise-spec model, universal seam script semantics,
ordered trace). Produces {func,in,out} envelopes simulating the C harness, piped
into tests/oracle_plugins_cmd_flow.py (the live drive) for the shadow-diff.

Code-only verification: no compiling/linking. The mirror models C json_t* as
Python values (NULL pointer == None; a raise spec == {"__raise__":kind,"msg":...}
returned as a VALUE, propagated/swallowed by each function exactly as the C does).
"""
import copy
import json
import math
import os
import sys
from pathlib import PurePosixPath

HERE = os.path.dirname(os.path.abspath(__file__))
FIX = os.path.join(HERE, "fixtures", "plugins_cmd_flow.jsonl")

EXC_PARENTS = {
    "SystemExit": "BaseException", "KeyboardInterrupt": "BaseException", "GeneratorExit": "BaseException",
    "Exception": "BaseException", "StopIteration": "Exception", "StopAsyncIteration": "Exception",
    "ArithmeticError": "Exception", "FloatingPointError": "ArithmeticError", "OverflowError": "ArithmeticError",
    "ZeroDivisionError": "ArithmeticError", "AssertionError": "Exception", "AttributeError": "Exception",
    "BufferError": "Exception", "EOFError": "Exception", "ImportError": "Exception",
    "ModuleNotFoundError": "ImportError", "LookupError": "Exception", "IndexError": "LookupError",
    "KeyError": "LookupError", "MemoryError": "Exception", "NameError": "Exception",
    "UnboundLocalError": "NameError", "OSError": "Exception", "BlockingIOError": "OSError",
    "ChildProcessError": "OSError", "ConnectionError": "OSError", "BrokenPipeError": "ConnectionError",
    "ConnectionAbortedError": "ConnectionError", "ConnectionRefusedError": "ConnectionError",
    "ConnectionResetError": "ConnectionError", "FileExistsError": "OSError", "FileNotFoundError": "OSError",
    "InterruptedError": "OSError", "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "PermissionError": "OSError", "ProcessLookupError": "OSError", "TimeoutError": "OSError",
    "ReferenceError": "Exception", "RuntimeError": "Exception", "NotImplementedError": "RuntimeError",
    "RecursionError": "RuntimeError", "SyntaxError": "Exception", "IndentationError": "SyntaxError",
    "TabError": "IndentationError", "SystemError": "Exception", "TypeError": "Exception",
    "ValueError": "Exception", "UnicodeError": "ValueError", "UnicodeDecodeError": "UnicodeError",
    "UnicodeEncodeError": "UnicodeError", "UnicodeTranslateError": "UnicodeError", "Warning": "Exception",
    "DeprecationWarning": "Warning", "PendingDeprecationWarning": "Warning", "RuntimeWarning": "Warning",
    "SyntaxWarning": "Warning", "UserWarning": "Warning", "FutureWarning": "Warning", "ImportWarning": "Warning",
    "UnicodeWarning": "Warning", "BytesWarning": "Warning", "ResourceWarning": "Warning",
    "EncodingWarning": "Warning", "PluginOperationError": "Exception",
    "PluginSourceMoved": "PluginOperationError", "PluginScanBlocked": "PluginOperationError",
}


def kind_is_a(k, anc):
    cur = k
    for _ in range(32):
        if cur == anc:
            return True
        if cur == "BaseException":
            return False
        p = EXC_PARENTS.get(cur, "Exception")
        if p == cur:
            return False
        cur = p
    return False


def caught_by_exception(k):
    return kind_is_a(k, "Exception")


def is_oserror(k):
    return kind_is_a(k, "OSError")


def is_valueerror(k):
    return kind_is_a(k, "ValueError")


def is_eoferror(k):
    return kind_is_a(k, "EOFError")


def is_keyboardinterrupt(k):
    return kind_is_a(k, "KeyboardInterrupt")


def is_plugin_operation_error(k):
    return kind_is_a(k, "PluginOperationError")


def is_plugin_scan_blocked(k):
    return kind_is_a(k, "PluginScanBlocked")


def is_raise(r):
    return isinstance(r, dict) and "__raise__" in r


def rkind(r):
    return r.get("__raise__", "") if isinstance(r, dict) else ""


def rmsg(r):
    return r.get("msg", "") if isinstance(r, dict) else ""


def mkraise(kind, msg):
    return {"__raise__": kind, "msg": msg}


def truthy(v):
    if v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def present(v):
    return v is not None


def jstr(v):
    return v if isinstance(v, str) else ""


def is_int(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool) and v == math.floor(v)


def type_name_of(v):
    if v is None:
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, (int, float)):
        return "int" if v == math.floor(v) else "float"
    if isinstance(v, str):
        return "str"
    if isinstance(v, list):
        return "list"
    if isinstance(v, dict):
        return "dict"
    return "object"


def py_str(v):
    if v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, (int, float)):
        if v == math.floor(v) and abs(v) < 1e15:
            return "%.0f" % v
        return repr(v)
    if isinstance(v, str):
        return v
    return ""


def py_str_or_empty(v):
    return py_str(v) if truthy(v) else ""


def strip_ws(s):
    return (s or "").strip()


def lower_ascii(s):
    return (s or "").lower()


def leaf(key):
    return (key or "").rsplit("/", 1)[-1]


def posix_join(a, b):
    return str(PurePosixPath(a or "") / (b or ""))


def path_name(s):
    return PurePosixPath(s or "").name


def path_parent(s):
    return str(PurePosixPath(s or "").parent)


def path_norm(s):
    return str(PurePosixPath(s or ""))


def ss_from(arr):
    return {x for x in (arr or []) if isinstance(x, str)}


def ss_sorted(s):
    return sorted(s)


def ss_intersects(a, b):
    return bool(a & b)


class Ctx:
    def __init__(self, inp):
        self.inp = inp
        self.scripts = inp.get("seams", {})
        self.idx = {}
        self.trace = []

    def call(self, name, args):
        scripts = self.scripts
        if name not in scripts:
            result = mkraise("RuntimeError", "no script for " + name)
        else:
            sc = scripts[name]
            if isinstance(sc, list):
                if not sc:
                    result = mkraise("RuntimeError", "no script for " + name)
                else:
                    i = self.idx.get(name, 0)
                    self.idx[name] = i + 1
                    result = sc[min(i, len(sc) - 1)]
            else:
                result = sc
        dropped = {k: v for k, v in args.items() if v is not None}
        self.trace.append({"ev": name, "args": dropped, "ret": copy.deepcopy(result)})
        if result is None:
            return None
        return copy.deepcopy(result)


def log_warning(ctx, fmt, args_arr):
    return ctx.call("logger_warning", {"fmt": fmt, "args": args_arr})


def console_print(ctx, console, message):
    if not present(console):
        return mkraise("AttributeError", "'NoneType' object has no attribute 'print'")
    return ctx.call("console_print", {"args": [message or ""]})


# ── pure / config leaves ──

def plc_console(ctx):
    return ctx.call("Console", {})


def plc_table(ctx, columns, kwargs):
    ta = copy.deepcopy(kwargs) if isinstance(kwargs, dict) else {}
    table = ctx.call("Table", ta)
    if is_raise(table):
        return table
    for col in (columns or []):
        header = ""
        style = None
        if isinstance(col, list) and len(col) >= 1:
            header = jstr(col[0])
            if len(col) >= 2:
                style = col[1]
        a = {"header": header}
        if style is not None:
            a["style"] = copy.deepcopy(style)
        r = ctx.call("add_column", a)
        if is_raise(r):
            return r
    return table


def plc_is_tty(ctx):
    si = ctx.call("stdin_isatty", {})
    if is_raise(si):
        return si
    if not truthy(si):
        return False
    so = ctx.call("stdout_isatty", {})
    if is_raise(so):
        return so
    return bool(truthy(so))


def plc_fail(ctx, console, message):
    p = console_print(ctx, console, message)
    if is_raise(p):
        return p
    return mkraise("SystemExit", "1")


def plc_ask_yes(ctx, prompt):
    r = ctx.call("reader", {"prompt": prompt or ""})
    if is_raise(r):
        k = rkind(r)
        if is_eoferror(k) or is_keyboardinterrupt(k):
            return False
        return r
    if not isinstance(r, str):
        return mkraise("AttributeError", "'%s' object has no attribute 'strip'" % type_name_of(r))
    low = lower_ascii(strip_ws(r))
    return low in ("y", "yes")


def plc_config_value(ctx, keys, default_v):
    cfg = ctx.call("load_config", {})
    if is_raise(cfg):
        if caught_by_exception(rkind(cfg)):
            return copy.deepcopy(default_v)
        return cfg
    a = {"config": copy.deepcopy(cfg), "keys": copy.deepcopy(keys or []), "default": copy.deepcopy(default_v)}
    r = ctx.call("cfg_get", a)
    if is_raise(r):
        if caught_by_exception(rkind(r)):
            return copy.deepcopy(default_v)
        return r
    return r


def plc_clone_timeout_seconds(ctx):
    keys = ["plugins", "clone_timeout_seconds"]
    value = plc_config_value(ctx, keys, 300)
    if is_raise(value):
        return value
    bad = isinstance(value, bool) or not is_int(value) or (is_int(value) and value < 1)
    if bad:
        w = log_warning(ctx, "plugins.clone_timeout_seconds must be a positive integer; using %ss", [300])
        if is_raise(w):
            return w
        return 300
    if value > 3600:
        w = log_warning(ctx, "plugins.clone_timeout_seconds exceeds %ss; clamping", [3600])
        if is_raise(w):
            return w
        return 3600
    return value


def plc_config_name_set(ctx, keys):
    value = plc_config_value(ctx, keys, [])
    if is_raise(value):
        return value
    s = ss_from(value) if isinstance(value, list) else set()
    return ss_sorted(s)


def plc_config_str(ctx, keys, default_v):
    value = plc_config_value(ctx, keys, default_v)
    if is_raise(value):
        return value
    if truthy(value):
        return value
    return copy.deepcopy(default_v)


def plc_write_config_value(ctx, section, key, value):
    config = ctx.call("load_config", {})
    if is_raise(config):
        return config
    if not isinstance(config, dict):
        return mkraise("AttributeError", "'%s' object has no attribute 'setdefault'" % type_name_of(config))
    sec = config.get(section)
    if sec is None and section not in config:
        sec = {}
        config[section] = sec
    if not isinstance(sec, dict):
        return mkraise("TypeError", "'%s' object does not support item assignment" % type_name_of(sec))
    sec[key] = copy.deepcopy(value)
    r = ctx.call("save_config", {"config": copy.deepcopy(config)})
    if is_raise(r):
        return r
    return None


# ── manifest cluster ──

def plc_native_manifest_file(ctx, plugin_dir):
    r = ctx.call("native_manifest_file", {"plugin_dir": plugin_dir or ""})
    if is_raise(r):
        if is_valueerror(rkind(r)):
            return mkraise("PluginOperationError", rmsg(r))
        return r
    return r


def plc_has_portable_manifest(ctx, plugin_dir):
    pf = posix_join(plugin_dir, "plugin.json")
    e = ctx.call("exists", {"path": pf})
    if is_raise(e):
        return e
    if truthy(e):
        return True
    s = ctx.call("is_symlink", {"path": pf})
    if is_raise(s):
        return s
    return bool(truthy(s))


def plc_load_yaml_manifest(ctx, manifest_file):
    return ctx.call("read_native_manifest", {"manifest_file": manifest_file or ""})


def plc_looks_like_plugin_dir(ctx, target):
    nm = plc_native_manifest_file(ctx, target)
    if is_raise(nm):
        return nm
    if present(nm):
        return True
    pj = posix_join(target, "plugin.json")
    e = ctx.call("exists", {"path": pj})
    if is_raise(e):
        return e
    if truthy(e):
        return True
    init = posix_join(target, "__init__.py")
    i = ctx.call("exists", {"path": init})
    if is_raise(i):
        return i
    return bool(truthy(i))


def plc_read_manifest_info(ctx, d, prefix):
    mf = plc_native_manifest_file(ctx, d)
    if is_raise(mf):
        return mf
    dname = path_name(d)
    if not present(mf):
        hp = plc_has_portable_manifest(ctx, d)
        if is_raise(hp):
            return hp
        if not truthy(hp):
            return None
        r = ctx.call("read_agent_plugin_manifest", {"plugin_dir": d or ""})
        if is_raise(r):
            if caught_by_exception(rkind(r)):
                return None
            return r
        if not isinstance(r, list) or len(r) == 0:
            return None
        manifest = copy.deepcopy(r[0])
        if not isinstance(manifest, dict) or "name" not in manifest:
            return None
        name = py_str(manifest["name"])
    else:
        manifest = plc_load_yaml_manifest(ctx, jstr(mf))
        if is_raise(manifest):
            if caught_by_exception(rkind(manifest)):
                manifest = {}
            else:
                return manifest
        if not isinstance(manifest, dict):
            manifest = {}
        name = py_str(manifest["name"]) if "name" in manifest else dname
    key = ("%s/%s" % (prefix, dname)) if prefix else name
    return [name,
            copy.deepcopy(manifest["version"]) if "version" in manifest else "",
            copy.deepcopy(manifest["description"]) if "description" in manifest else "",
            key]


def plc_missing_env_specs(ctx, manifest):
    if not isinstance(manifest, dict):
        return mkraise("AttributeError", "'%s' object has no attribute 'get'" % type_name_of(manifest))
    re = manifest.get("requires_env")
    lst = re if truthy(re) else None
    specs = []
    if isinstance(lst, list):
        for entry in lst:
            if isinstance(entry, str):
                specs.append({"name": entry})
            elif isinstance(entry, dict):
                if truthy(entry.get("name")):
                    specs.append(copy.deepcopy(entry))
    if not specs:
        return []
    out = []
    for s in specs:
        nm = jstr(s.get("name"))
        v = ctx.call("get_env_value", {"name": nm})
        if is_raise(v):
            return v
        if not truthy(v):
            out.append(copy.deepcopy(s))
    return out


# ── message builders ──

def plc_clone_failure_message(ctx, git_url, git_error):
    stripped = strip_ws(git_error)
    esc = ctx.call("markup_escape", {"text": stripped})
    if is_raise(esc):
        return esc
    return ("Could not download the plugin from %s. Check the address (browse the catalog "
            "with `hermes plugins search`), check your internet connection, or, if the repository "
            "is private, sign in first with `gh auth login` (or set GITHUB_TOKEN in your .env).\n"
            "Details: %s") % (git_url or "", jstr(esc))


def plc_unknown_plugin_message(ctx, name, downloaded_only):
    scope = (" This command only works on downloaded plugins; bundled ones can only be enabled or disabled."
             if downloaded_only else " Bundled plugins can only be enabled or disabled.")
    moved = ctx.call("moved_plugin_hint", {"name": name or ""})
    if is_raise(moved):
        return moved
    if truthy(moved):
        return "[red]No plugin named '%s'.[/red] %s" % (name or "", jstr(moved))
    return ("[red]No plugin named '%s'.[/red] Run `hermes plugins list` to see the exact names "
            "(nested plugins use their full key, e.g. web/firecrawl).%s "
            "To add one: `hermes plugins install <owner/repo>`.") % (name or "", scope)


def preserved_files_note_core(exc_msg, scan_result, merged):
    preserved = ss_from(merged)
    hits = set()
    if present(scan_result) and isinstance(scan_result, dict):
        findings = scan_result.get("findings")
        if isinstance(findings, list):
            for f in findings:
                file = jstr(f.get("file") if isinstance(f, dict) else None)
                if file in preserved:
                    hits.add(file)
    hits = sorted(hits)
    if hits:
        note = ("These findings come from user files preserved from the installed copy: "
                "%s. Move or remove them and retry the update.") % ", ".join(hits)
    else:
        note = "The scanned tree included user files preserved from the installed copy."
    return "%s\n\n%s" % (exc_msg or "", note)


def plc_preserved_files_note(exc, merged):
    msg = (exc or {}).get("message", "")
    sr = (exc or {}).get("scan_result")
    return preserved_files_note_core(msg, sr, merged)


def plc_scan_merged_tree(ctx, plugin_dir, identifier, merged, kwargs):
    a = {"plugin_dir": plugin_dir or "", "identifier": identifier or ""}
    if isinstance(kwargs, dict):
        for k, v in kwargs.items():
            a[k] = copy.deepcopy(v)
    r = ctx.call("_scan_plugin_tree", a)
    if not is_raise(r):
        return r
    if not is_plugin_scan_blocked(rkind(r)):
        return r
    if not truthy(merged):
        return r
    sr = r.get("scan_result")
    note = preserved_files_note_core(rmsg(r), sr, merged)
    return mkraise("PluginScanBlocked", note)


# ── selection algebra ──

def plc_discard_key_and_leaf(names, key):
    s = ss_from(names)
    s.discard(key or "")
    s.discard(leaf(key))
    return ss_sorted(s)


def plc_plugin_aliases(ctx, key, entries):
    legacy = ctx.call("legacy_plugin_keys", {"key": key or ""})
    if is_raise(legacy):
        return legacy
    s = {key or "", leaf(key)}
    if isinstance(legacy, list):
        for x in legacy:
            s.add(jstr(x))
    if present(entries):
        src = entries
    else:
        src = plc_discover_all_plugins(ctx)
        if is_raise(src):
            return src
    if isinstance(src, list):
        for e in src:
            if isinstance(e, list) and len(e) >= 6:
                if jstr(e[5]) == (key or ""):
                    s.add(jstr(e[0]))
    return ss_sorted(s)


def plc_disabled_by_update(ctx, key, aliases):
    home = ctx.call("get_hermes_home", {})
    if is_raise(home):
        return home
    sel = ctx.call("read_home_selection", {"home": jstr(home)})
    if is_raise(sel):
        return sel
    cfg = sel if truthy(sel) else None
    if cfg is not None and not isinstance(cfg, dict):
        return mkraise("AttributeError", "'%s' object has no attribute 'get'" % type_name_of(cfg))
    plugins = cfg.get("plugins") if cfg else None
    pcfg = plugins if truthy(plugins) else None
    if pcfg is not None and not isinstance(pcfg, dict):
        return mkraise("AttributeError", "'%s' object has no attribute 'get'" % type_name_of(pcfg))
    record = pcfg.get("disabled_by_update") if pcfg else None
    is_dict = isinstance(record, dict)
    hit = False
    if is_dict:
        want = {key or ""} | ss_from(aliases)
        hit = any(k in want for k in record)
    return bool(is_dict and hit)


def plc_apply_activation(enabled, disabled, key, aliases, enable):
    en = ss_from(enabled)
    dis = ss_from(disabled)
    al = ss_from(aliases)
    removed = dis if enable else en
    removed.discard(key or "")
    removed.discard(leaf(key))
    removed -= al
    target = en if enable else dis
    target.add(key or "")
    return {"enabled": ss_sorted(en), "disabled": ss_sorted(dis)}


def plc_activate_key(ctx, key, enable, console):
    en = ctx.call("get_enabled_set", {})
    if is_raise(en):
        return en
    dis = ctx.call("get_disabled_set", {})
    if is_raise(dis):
        return dis
    aliases = plc_plugin_aliases(ctx, key, None)
    if is_raise(aliases):
        return aliases
    en_s = ss_from(en)
    dis_s = ss_from(dis)
    al_s = ss_from(aliases)
    target = en_s if enable else dis_s
    other = dis_s if enable else en_s
    skip = False
    if (key or "") in target and not ss_intersects(al_s, other):
        if enable:
            skip = True
        else:
            dbu = plc_disabled_by_update(ctx, key, aliases)
            if is_raise(dbu):
                return dbu
            if not truthy(dbu):
                skip = True
    if skip:
        return False
    a = {"name": key or "", "enable": bool(enable), "aliases": copy.deepcopy(aliases)}
    if present(console):
        a["console"] = copy.deepcopy(console)
    r = ctx.call("set_plugin_enabled", a)
    if is_raise(r):
        return r
    return True


def plc_forget_plugin_config(ctx, aliases):
    config = ctx.call("load_config", {})
    if is_raise(config):
        return config
    if not isinstance(config, dict):
        return mkraise("AttributeError", "'%s' object has no attribute 'get'" % type_name_of(config))
    al = ss_from(aliases)
    changed = False
    result = {}
    plugins_cfg = config.get("plugins")
    if isinstance(plugins_cfg, dict):
        for list_key in ("enabled", "disabled"):
            names = plugins_cfg.get(list_key)
            if isinstance(names, list):
                ns = ss_from(names)
                if ss_intersects(al, ns):
                    plugins_cfg[list_key] = ss_sorted(ns - al)
                    changed = True
        entries = plugins_cfg.get("entries")
        if isinstance(entries, dict):
            if any(k in al for k in entries):
                for alias in al:
                    if alias in entries:
                        del entries[alias]
                changed = True
    memory_cfg = config.get("memory")
    if isinstance(memory_cfg, dict):
        stripped = strip_ws(py_str_or_empty(memory_cfg.get("provider")))
        if stripped in al:
            memory_cfg["provider"] = ""
            changed = True
            result = {"cleared_memory_provider": True}
    if changed:
        r = ctx.call("save_config", {"config": copy.deepcopy(config)})
        if is_raise(r):
            return r
    return result


def plc_plugin_selection_version(ctx):
    home = ctx.call("get_hermes_home", {})
    if is_raise(home):
        return home
    path = posix_join(jstr(home), "config.yaml")
    d = ctx.call("_digest", {"path": path})
    if is_raise(d):
        return d
    if truthy(d):
        return d
    return "missing"


def plc_refuse_recalled_entry(ctx, entry):
    if present(entry) and isinstance(entry, list) and len(entry) >= 5:
        if jstr(entry[3]) != "bundled":
            r = ctx.call("refuse_if_installed_removed", {"name": jstr(entry[0]), "plugin_dir": jstr(entry[4])})
            if is_raise(r):
                return r
    return None


def plc_tri_state_flag(args, yes_attr, no_attr):
    y = (args or {}).get(yes_attr)
    if truthy(y):
        return True
    n = (args or {}).get(no_attr)
    if truthy(n):
        return False
    return None


def plc_category_active_names(ctx):
    m = ctx.call("get_current_memory_provider", {})
    if is_raise(m):
        return m
    return [jstr(m)] if truthy(m) else []


def plc_saved_list_toolset_key(ctx, name):
    cfg = ctx.call("load_config", {})
    if is_raise(cfg):
        return cfg
    if not isinstance(cfg, dict):
        return mkraise("AttributeError", "'%s' object has no attribute 'get'" % type_name_of(cfg))
    saved = cfg.get("platform_toolsets")
    any_list = False
    if isinstance(saved, dict):
        for v in saved.values():
            p = ctx.call("parse_platform_toolsets_value", {"value": copy.deepcopy(v)})
            if is_raise(p):
                return p
            if present(p):
                any_list = True
                break
    if not any_list:
        return None
    return ctx.call("get_plugin_toolset_key", {"name": name or ""})


# ── discovery scan ──

def scan_warn(ctx, path, exc_msg):
    return log_warning(ctx, "Skipping unreadable plugin directory %s: %s", [path or "", exc_msg or ""])


def plc_scan_level(ctx, base, source, skip_names, prefix, depth, seen):
    isd = ctx.call("is_dir", {"path": base})
    if is_raise(isd):
        return isd
    if not truthy(isd):
        return None
    children = ctx.call("iterdir", {"path": base})
    if is_raise(children):
        if is_oserror(rkind(children)):
            w = scan_warn(ctx, base, rmsg(children))
            if is_raise(w):
                return w
            return None
        return children
    kids = sorted({jstr(x) for x in (children or []) if isinstance(x, str)})
    skip = ss_from(skip_names)
    for d in kids:
        dname = path_name(d)
        d_isdir = ctx.call("is_dir", {"path": d})
        if is_raise(d_isdir):
            k = rkind(d_isdir)
            if is_oserror(k) or is_plugin_operation_error(k):
                w = scan_warn(ctx, d, rmsg(d_isdir))
                if is_raise(w):
                    return w
                continue
            return d_isdir
        if not truthy(d_isdir) or (depth == 0 and skip and dname in skip):
            continue
        info = plc_read_manifest_info(ctx, d, prefix)
        if is_raise(info):
            k = rkind(info)
            if is_oserror(k) or is_plugin_operation_error(k):
                w = scan_warn(ctx, d, rmsg(info))
                if is_raise(w):
                    return w
                continue
            return info
        if info is None:
            if depth == 0:
                np = ("%s/%s" % (prefix, dname)) if prefix else dname
                r = plc_scan_level(ctx, d, source, [], np, 1, seen)
                if is_raise(r):
                    return r
            continue
        key = jstr(info[3])
        if key in seen and source == "bundled":
            continue
        src_label = source
        if source == "user":
            gitp = posix_join(d, ".git")
            ge = ctx.call("exists", {"path": gitp})
            if is_raise(ge):
                return ge
            if truthy(ge):
                src_label = "git"
        seen[key] = [copy.deepcopy(info[0]), copy.deepcopy(info[1]), copy.deepcopy(info[2]),
                     src_label, d, key]
    return None


def plc_discover_all_plugins(ctx):
    seen = {}
    # live tuple literal evaluates BOTH dir seams eagerly before any scan
    bundled = ctx.call("get_bundled_plugins_dir", {})
    if is_raise(bundled):
        return bundled
    userdir = ctx.call("_plugins_dir", {})
    if is_raise(userdir):
        return userdir
    skip = ["memory", "context_engine", "computer_use", "model-providers", "config_backends"]
    r = plc_scan_level(ctx, jstr(bundled), "bundled", skip, "", 0, seen)
    if is_raise(r):
        return r
    r = plc_scan_level(ctx, jstr(userdir), "user", [], "", 0, seen)
    if is_raise(r):
        return r
    eps = ctx.call("discover_entrypoint_manifests", {})
    if is_raise(eps):
        return eps
    if isinstance(eps, list):
        for m in eps:
            mname = jstr(m.get("name") if isinstance(m, dict) else None)
            if mname not in seen:
                seen[mname] = [mname, copy.deepcopy(m.get("version")), copy.deepcopy(m.get("description")),
                               "entrypoint", copy.deepcopy(m.get("path")), mname]
    return [copy.deepcopy(v) for v in seen.values()]


def plc_default_on(ctx, dir_path, source):
    isd = ctx.call("is_dir", {"path": dir_path})
    if is_raise(isd):
        return isd
    if not truthy(isd):
        return False
    pname = path_name(dir_path)
    parent = path_parent(dir_path)
    if source != "bundled":
        root = ctx.call("_plugins_dir", {})
        if is_raise(root):
            return root
        root_s = path_norm(jstr(root))
        root_mp = posix_join(root_s, "model-providers")
        name_bad = pname.startswith("_") or pname.startswith(".")
        parent_ok = parent == root_s or parent == root_mp
        if name_bad or not parent_ok:
            return False
        if parent == root_s:
            dk = ctx.call("_declares_model_provider_kind", {"path": dir_path or ""})
            if is_raise(dk):
                return dk
            return bool(truthy(dk))
    mf = plc_native_manifest_file(ctx, dir_path)
    if is_raise(mf):
        return mf
    if not present(mf):
        return False
    manifest = plc_load_yaml_manifest(ctx, jstr(mf))
    if is_raise(manifest):
        if caught_by_exception(rkind(manifest)):
            return False
        return manifest
    if not isinstance(manifest, dict):
        return False
    kind_raw = manifest["kind"] if "kind" in manifest else None
    ks = py_str(kind_raw) if kind_raw is not None else "standalone"
    kind = lower_ascii(strip_ws(ks))
    on = kind == "model-provider"
    if not on and source == "bundled":
        on = kind in ("backend", "platform", "model-provider")
    return on


# ── dispatch ──

def run_case(func, inp):
    ctx = Ctx(inp)
    state = None
    g = inp.get
    if func == "console":
        r = plc_console(ctx)
    elif func == "table":
        r = plc_table(ctx, g("columns"), g("kwargs"))
    elif func == "is_tty":
        r = plc_is_tty(ctx)
    elif func == "fail":
        r = plc_fail(ctx, g("console"), g("message", ""))
    elif func == "ask_yes":
        r = plc_ask_yes(ctx, g("prompt", ""))
    elif func == "config_value":
        r = plc_config_value(ctx, g("keys"), g("default"))
    elif func == "clone_timeout_seconds":
        r = plc_clone_timeout_seconds(ctx)
    elif func == "config_name_set":
        r = plc_config_name_set(ctx, g("keys"))
    elif func == "config_str":
        r = plc_config_str(ctx, g("keys"), g("default"))
    elif func == "write_config_value":
        r = plc_write_config_value(ctx, g("section", ""), g("key", ""), g("value"))
    elif func == "native_manifest_file":
        r = plc_native_manifest_file(ctx, g("plugin_dir", ""))
    elif func == "has_portable_manifest":
        r = plc_has_portable_manifest(ctx, g("plugin_dir", ""))
    elif func == "load_yaml_manifest":
        r = plc_load_yaml_manifest(ctx, g("manifest_file", ""))
    elif func == "looks_like_plugin_dir":
        r = plc_looks_like_plugin_dir(ctx, g("target", ""))
    elif func == "read_manifest_info":
        r = plc_read_manifest_info(ctx, g("d", ""), g("prefix", ""))
    elif func == "missing_env_specs":
        r = plc_missing_env_specs(ctx, g("manifest"))
    elif func == "clone_failure_message":
        r = plc_clone_failure_message(ctx, g("git_url", ""), g("git_error", ""))
    elif func == "unknown_plugin_message":
        r = plc_unknown_plugin_message(ctx, g("name", ""), g("downloaded_only", False))
    elif func == "preserved_files_note":
        r = plc_preserved_files_note(g("exc"), g("merged"))
    elif func == "scan_merged_tree":
        r = plc_scan_merged_tree(ctx, g("plugin_dir", ""), g("identifier", ""), g("merged"), g("kwargs"))
    elif func == "discard_key_and_leaf":
        res = plc_discard_key_and_leaf(g("names"), g("key", ""))
        state = {"names": res}
        r = None
    elif func == "plugin_aliases":
        r = plc_plugin_aliases(ctx, g("key", ""), g("entries"))
    elif func == "disabled_by_update":
        r = plc_disabled_by_update(ctx, g("key", ""), g("aliases"))
    elif func == "activate_key":
        r = plc_activate_key(ctx, g("key", ""), g("enable", False), g("console"))
    elif func == "apply_activation":
        state = plc_apply_activation(g("enabled"), g("disabled"), g("key", ""), g("aliases"), g("enable", False))
        r = None
    elif func == "forget_plugin_config":
        r = plc_forget_plugin_config(ctx, g("aliases"))
    elif func == "plugin_selection_version":
        r = plc_plugin_selection_version(ctx)
    elif func == "refuse_recalled_entry":
        r = plc_refuse_recalled_entry(ctx, g("entry"))
    elif func == "tri_state_flag":
        r = plc_tri_state_flag(g("args"), g("yes_attr", ""), g("no_attr", ""))
    elif func == "category_active_names":
        r = plc_category_active_names(ctx)
    elif func == "saved_list_toolset_key":
        r = plc_saved_list_toolset_key(ctx, g("name", ""))
    elif func == "scan_level":
        seen = copy.deepcopy(g("seen") or {})
        r = plc_scan_level(ctx, g("base", ""), g("source", ""), g("skip_names"), g("prefix", ""), g("depth", 0), seen)
        if not is_raise(r):
            state = {"seen": seen}
            r = None
    elif func == "discover_all_plugins":
        r = plc_discover_all_plugins(ctx)
    elif func == "default_on":
        r = plc_default_on(ctx, g("dir_path", ""), g("source", ""))
    else:
        r = mkraise("RuntimeError", "unknown func")
    out = {"ret": r, "trace": ctx.trace}
    if state is not None:
        out["state"] = state
    return out


def main():
    n = 0
    with open(FIX, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line.startswith("{"):
                continue
            c = json.loads(line)
            out = run_case(c["func"], c.get("in") or {})
            print(json.dumps({"func": c["func"], "in": c.get("in"), "out": out}, ensure_ascii=False))
            n += 1
    sys.stderr.write("mirror emitted %d envelopes\n" % n)


if __name__ == "__main__":
    main()