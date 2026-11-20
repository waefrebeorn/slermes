#!/usr/bin/env python3
"""Shadow mirror of src/cli/plugins_cmd_flow2.c — a LITERAL Python transcription of
the C algorithms (value-or-raise-spec model, universal seam script semantics, ordered
trace, state windows). Produces {func,in,out} envelopes simulating the C harness, piped
into tests/oracle_plugins_cmd_flow2.py (the live drive) for the shadow-diff.

Code-only verification: no compiling/linking. The mirror models C json_t* as Python
values (NULL pointer == None; a raise spec == {"__raise__":kind,"msg":...} returned as a
VALUE, propagated/swallowed by each function exactly as the C does). The seam dispatcher
mirrors the harness script_call (presence-test fail-closed, per-name queue cursor with
last-element repeat, scripted null -> None, args drop-null, ordered trace).
"""
import copy
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FIX = os.path.join(HERE, "fixtures", "plugins_cmd_flow2.jsonl")

EXC_PARENTS = {
    "SystemExit": "BaseException", "KeyboardInterrupt": "BaseException", "GeneratorExit": "BaseException",
    "Exception": "BaseException", "BaseExceptionGroup": "BaseException", "ExceptionGroup": "Exception",
    "StopIteration": "Exception", "StopAsyncIteration": "Exception", "ArithmeticError": "Exception",
    "FloatingPointError": "ArithmeticError", "OverflowError": "ArithmeticError", "ZeroDivisionError": "ArithmeticError",
    "AssertionError": "Exception", "AttributeError": "Exception", "BufferError": "Exception", "EOFError": "Exception",
    "ImportError": "Exception", "ModuleNotFoundError": "ImportError", "LookupError": "Exception",
    "IndexError": "LookupError", "KeyError": "LookupError", "MemoryError": "Exception", "NameError": "Exception",
    "UnboundLocalError": "NameError", "OSError": "Exception", "IOError": "OSError", "EnvironmentError": "OSError",
    "WindowsError": "OSError", "BlockingIOError": "OSError", "ChildProcessError": "OSError",
    "ConnectionError": "OSError", "BrokenPipeError": "ConnectionError", "ConnectionAbortedError": "ConnectionError",
    "ConnectionRefusedError": "ConnectionError", "ConnectionResetError": "ConnectionError",
    "FileExistsError": "OSError", "FileNotFoundError": "OSError", "InterruptedError": "OSError",
    "IsADirectoryError": "OSError", "NotADirectoryError": "OSError", "PermissionError": "OSError",
    "ProcessLookupError": "OSError", "TimeoutError": "OSError", "ReferenceError": "Exception",
    "RuntimeError": "Exception", "NotImplementedError": "RuntimeError", "RecursionError": "RuntimeError",
    "PythonFinalizationError": "RuntimeError", "SyntaxError": "Exception", "IndentationError": "SyntaxError",
    "TabError": "IndentationError", "_IncompleteInputError": "SyntaxError", "SystemError": "Exception",
    "TypeError": "Exception", "ValueError": "Exception", "UnicodeError": "ValueError",
    "UnicodeDecodeError": "UnicodeError", "UnicodeEncodeError": "UnicodeError", "UnicodeTranslateError": "UnicodeError",
    "Warning": "Exception", "DeprecationWarning": "Warning", "PendingDeprecationWarning": "Warning",
    "RuntimeWarning": "Warning", "SyntaxWarning": "Warning", "UserWarning": "Warning", "FutureWarning": "Warning",
    "ImportWarning": "Warning", "UnicodeWarning": "Warning", "BytesWarning": "Warning", "ResourceWarning": "Warning",
    "EncodingWarning": "Warning", "PluginOperationError": "Exception", "PluginSourceMoved": "PluginOperationError",
    "PluginScanBlocked": "PluginOperationError", "AdmissionRefused": "RuntimeError", "DependencyConflict": "AdmissionRefused",
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


def is_plugin_operation_error(k):
    return kind_is_a(k, "PluginOperationError")


def is_admission_refused(k):
    return kind_is_a(k, "AdmissionRefused")


def is_dependency_conflict(k):
    return kind_is_a(k, "DependencyConflict")


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


def type_name_of(v):
    if v is None:
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, int):
        return "int"
    if isinstance(v, float):
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
        d = float(v)
        if d == math.floor(d) and abs(d) < 1e15:
            return "%.0f" % d
        return "%.17g" % d
    if isinstance(v, str):
        return v
    return ""


def format_g(v):
    return "%g" % v


def subscript_error(v):
    if isinstance(v, str):
        return mkraise("TypeError", "string indices must be integers, not 'str'")
    if isinstance(v, list):
        return mkraise("TypeError", "list indices must be integers or slices, not str")
    return mkraise("TypeError", "'%s' object is not subscriptable" % type_name_of(v))


# ── lexical PurePosixPath component model ──

def path_parse(s):
    s = s or ""
    n = len(s)
    lead = 0
    while lead < n and s[lead] == "/":
        lead += 1
    root = "//" if lead == 2 else ("/" if lead >= 1 else "")
    parts = []
    i = lead
    while i < n:
        j = i
        while j < n and s[j] != "/":
            j += 1
        seg = s[i:j]
        if seg == ".":
            pass
        elif seg:
            parts.append(seg)
        i = j
        while i < n and s[i] == "/":
            i += 1
    return root, parts


def path_str(root, parts):
    if not parts and root == "":
        return "."
    out = root
    for p in parts:
        if out and out[-1] != "/":
            out += "/"
        out += p
    return out


def posix_join(a, b):
    ra, pa = path_parse(a)
    rb, pb = path_parse(b)
    if rb != "":
        return path_str(rb, pb)
    return path_str(ra, pa + pb)


def path_eq(a, b):
    ra, pa = path_parse(a)
    rb, pb = path_parse(b)
    return ra == rb and pa == pb


def leaf(key):
    key = key or ""
    idx = key.rfind("/")
    return key[idx + 1:] if idx >= 0 else key


def ss_from_array(arr):
    s = set()
    if isinstance(arr, list):
        for v in arr:
            if isinstance(v, str):
                s.add(v)
    return s


def ss_to_array(s):
    return sorted(s)


def sorted_set_array(arr):
    return ss_to_array(ss_from_array(arr))


# ── universal seam dispatcher (mirrors harness script_call) ──

class Ctx:
    def __init__(self, inp):
        self.inp = inp
        self.scripts = inp.get("seams", {})
        self.idx = {}
        self.trace = []

    def call(self, name, args):
        args = {k: v for k, v in args.items() if v is not None}
        if name not in self.scripts:
            result = mkraise("RuntimeError", "no script for " + name)
        else:
            sc = self.scripts[name]
            if isinstance(sc, list):
                if not sc:
                    result = mkraise("RuntimeError", "no script for " + name)
                else:
                    i = self.idx.get(name, 0)
                    self.idx[name] = i + 1
                    result = copy.deepcopy(sc[min(i, len(sc) - 1)])
            else:
                result = copy.deepcopy(sc)
        if result is None:
            pass   # scripted null -> None (NULL pointer)
        self.trace.append({"ev": name, "args": args, "ret": result})
        return result


def console_print(ctx, console, message):
    if not present(console):
        return mkraise("AttributeError", "'NoneType' object has no attribute 'print'")
    return ctx.call("console_print", {"args": [message]})


def fail_seam(ctx, console, message):
    a = {}
    if present(console):
        a["console"] = console
    a["message"] = message
    return ctx.call("fail", a)


# ── admission transaction ──

def pc2_admit_and_save_plugin_sets(ctx, enabled, disabled, extra_dirs, console, action,
                                   expected_config, plugin, settled):
    pdir = ctx.call("_plugins_dir", {})
    if is_raise(pdir):
        return pdir
    a = {"enabled": sorted_set_array(enabled), "disabled": sorted_set_array(disabled),
         "active_plugins_dir": jstr(pdir),
         "extra_dirs": copy.deepcopy(extra_dirs) if isinstance(extra_dirs, list) else [],
         "settled": copy.deepcopy(settled) if isinstance(settled, list) else []}
    if present(expected_config):
        a["expected_config"] = expected_config
    if present(plugin):
        a["plugin"] = plugin
    r = ctx.call("admit_plugin_set_change", a)
    if not is_raise(r):
        return None
    k = rkind(r)
    if is_dependency_conflict(k):
        if present(console):
            esc = ctx.call("markup_escape", {"text": rmsg(r)})
            if is_raise(esc):
                return esc
            p1 = console_print(ctx, console, "[red]\u2717[/red] %s" % jstr(esc))
            if is_raise(p1):
                return p1
            p2 = console_print(ctx, console, "[dim]config.yaml and the active environment are unchanged.[/dim]")
            if is_raise(p2):
                return p2
        return r
    if is_admission_refused(k):
        if present(console):
            p1 = console_print(ctx, console, "[red]\u2717[/red] %s refused: %s" % (action or "enable", rmsg(r)))
            if is_raise(p1):
                return p1
            p2 = console_print(ctx, console,
                "[dim]config.yaml and the active environment are unchanged. "
                "Run `hermes pm install` to resolve dependencies, then retry.[/dim]")
            if is_raise(p2):
                return p2
        return r
    return r


def pc2_selection_lock(ctx, locks):
    key = ctx.call("hermes_home_key", {})
    if is_raise(key):
        return key
    lock = ctx.call("threading_lock", {})
    if is_raise(lock):
        return lock
    ks = jstr(key)
    if ks in locks:
        return copy.deepcopy(locks[ks])
    locks[ks] = lock
    return copy.deepcopy(lock)


def pc2_set_plugin_enabled(ctx, name, enable, aliases, console, locks, work):
    if enable:
        entry = pc2_find_plugin_entry(ctx, name)
        if is_raise(entry):
            return entry
        a = {}
        if present(entry):
            a["entry"] = entry
        rr = ctx.call("refuse_recalled_entry", a)
        if is_raise(rr):
            return rr
    lk = pc2_selection_lock(ctx, locks)
    if is_raise(lk):
        return lk
    ver = ctx.call("plugin_selection_version", {})
    if is_raise(ver):
        return ver
    home = ctx.call("get_hermes_home", {})
    if is_raise(home):
        return home
    config = ctx.call("read_home_selection", {"home": jstr(home)})
    if is_raise(config):
        return config
    cfg = config if truthy(config) else None
    if cfg is not None and not isinstance(cfg, dict):
        return mkraise("AttributeError", "'%s' object has no attribute 'get'" % type_name_of(cfg))
    plugins = cfg.get("plugins") if cfg else None
    pcfg = plugins if truthy(plugins) else None
    if pcfg is not None and not isinstance(pcfg, dict):
        return mkraise("AttributeError", "'%s' object has no attribute 'get'" % type_name_of(pcfg))
    en_v = pcfg.get("enabled") if pcfg else None
    dis_v = pcfg.get("disabled") if pcfg else None
    en = ss_from_array(en_v) if truthy(en_v) and isinstance(en_v, list) else set()
    dis = ss_from_array(dis_v) if truthy(dis_v) and isinstance(dis_v, list) else set()
    work["enabled"] = ss_to_array(en)
    work["disabled"] = ss_to_array(dis)
    aa = {"enabled": ss_to_array(en), "disabled": ss_to_array(dis), "name": name,
          "aliases": copy.deepcopy(aliases) if isinstance(aliases, list) else [], "enable": enable}
    ar = ctx.call("apply_activation", aa)
    if is_raise(ar):
        return ar
    if isinstance(ar, dict) and isinstance(ar.get("state"), dict):
        st = ar["state"]
        if isinstance(st.get("enabled"), list):
            en = ss_from_array(st["enabled"])
        if isinstance(st.get("disabled"), list):
            dis = ss_from_array(st["disabled"])
    work["enabled"] = ss_to_array(en)
    work["disabled"] = ss_to_array(dis)
    action = "%s '%s'" % ("Enable" if enable else "Disable", name)
    settled = [name] + (copy.deepcopy(aliases) if isinstance(aliases, list) else [])
    plugin_arg = name if enable else None
    r = pc2_admit_and_save_plugin_sets(ctx, ss_to_array(en), ss_to_array(dis), None, console,
                                       action, ver, plugin_arg, settled)
    if is_raise(r):
        return r
    return None


def pc2_catalog_installed_dir(ctx, name):
    md = ctx.call("read_install_metadata", {})
    if is_raise(md):
        if is_plugin_operation_error(rkind(md)):
            return None
        return md
    if md is None:
        return mkraise("TypeError", "'NoneType' object is not iterable")
    if isinstance(md, dict):
        keys = list(md.keys())
    elif isinstance(md, list):
        keys = [jstr(v) for v in md]
    elif isinstance(md, str):
        keys = list(md)
    else:
        return mkraise("TypeError", "'%s' object is not iterable" % type_name_of(md))
    hits = []
    for key in keys:
        root = ctx.call("_plugins_dir", {})
        if is_raise(root):
            return root
        target = posix_join(jstr(root), key)
        isd = ctx.call("is_dir", {"path": target})
        if is_raise(isd):
            return isd
        if not truthy(isd):
            continue
        rec = ctx.call("catalog_install_record", {"plugin_dir": target})
        if is_raise(rec):
            return rec
        if not truthy(rec):
            continue
        if not isinstance(rec, dict):
            return subscript_error(rec)
        if "catalog_name" not in rec:
            return mkraise("KeyError", "'catalog_name'")
        cn = rec["catalog_name"]
        if isinstance(cn, str) and cn == name:
            hits.append(target)
    if len(hits) == 1:
        return hits[0]
    return None


def pc2_find_plugin_entry(ctx, name):
    entries = ctx.call("discover_all_plugins", {})
    if is_raise(entries):
        return entries
    alias = ctx.call("legacy_plugin_alias", {"name": name})
    if is_raise(alias):
        return alias
    target = jstr(alias) if truthy(alias) else name
    if isinstance(entries, list):
        for e in entries:
            if isinstance(e, list) and len(e) >= 6:
                n0, n5 = e[0], e[5]
                if (isinstance(n0, str) and n0 == target) or (isinstance(n5, str) and n5 == target):
                    return copy.deepcopy(e)
    leaf_first = None
    leaf_n = 0
    if isinstance(entries, list):
        for e in entries:
            if isinstance(e, list) and len(e) >= 6:
                k = e[5]
                if not isinstance(k, str):
                    return mkraise("AttributeError", "'%s' object has no attribute 'split'" % type_name_of(k))
                if leaf(k) == name:
                    leaf_n += 1
                    if leaf_first is None:
                        leaf_first = copy.deepcopy(e)
    if leaf_n > 0:
        return leaf_first if leaf_n == 1 else None
    cat = pc2_catalog_installed_dir(ctx, name)
    if is_raise(cat):
        return cat
    if present(cat) and isinstance(entries, list):
        for e in entries:
            if isinstance(e, list) and len(e) >= 6:
                dp = e[4]
                if truthy(dp):
                    if path_eq(py_str(dp), jstr(cat)):
                        return copy.deepcopy(e)
    return None


def pc2_resolved_user_plugin_dir(ctx, name):
    entry = pc2_find_plugin_entry(ctx, name)
    if is_raise(entry):
        return entry
    found = None
    if truthy(entry) and isinstance(entry, list) and len(entry) >= 6:
        s = jstr(entry[3])
        dp = entry[4]
        if s in ("user", "git") and truthy(dp):
            found = py_str(dp)
    if found is None:
        return None
    isd = ctx.call("is_dir", {"path": found})
    if is_raise(isd):
        return isd
    if not truthy(isd):
        return None
    return found


def pc2_enable_consent(ctx, console, key, allow_tool_override):
    caps = ctx.call("declared_capabilities_for_key", {"key": key})
    if is_raise(caps):
        return caps
    if truthy(caps):
        a = {"key": key, "declared_caps": caps, "context": "enable"}
        if present(console):
            a["console"] = console
        r = ctx.call("run_capability_consent", a)
        if is_raise(r):
            return r
    if present(allow_tool_override):
        a = {"key": key, "allow_tool_override": allow_tool_override}
        if present(console):
            a["console"] = console
        r = ctx.call("resolve_tool_override_grant", a)
        if is_raise(r):
            return r
    return None


def pc2_catalog(ctx):
    return ctx.call("catalog_module", {})


def pc2_action_update(ctx, args):
    args = args or {}
    if "name" not in args:
        return mkraise("AttributeError", "'types.SimpleNamespace' object has no attribute 'name'")
    name = args["name"]
    if truthy(name):
        if truthy(args.get("yes")):
            con = ctx.call("Console", {})
            if is_raise(con):
                return con
            f = fail_seam(ctx, con, "[red]Error:[/red] --yes only applies to `hermes plugins update` with no name.")
            if is_raise(f):
                return f
        a = {"name": name, "force": bool(truthy(args.get("force")))}
        r = ctx.call("cmd_update", a)
        if is_raise(r):
            return r
        return None
    if truthy(args.get("force")):
        con = ctx.call("Console", {})
        if is_raise(con):
            return con
        f = fail_seam(ctx, con, "[red]Error:[/red] --force applies to one named plugin: `hermes plugins update <name> --force`.")
        if is_raise(f):
            return f
    a = {"yes": bool(truthy(args.get("yes")))}
    r = ctx.call("cmd_update_all", a)
    if is_raise(r):
        return r
    return None


def pc2_action_auto_update(ctx, args):
    args = args or {}
    console = ctx.call("Console", {})
    if is_raise(console):
        return console
    state = args.get("state")
    if not present(state):
        cur = ctx.call("auto_update_state", {})
    else:
        on = isinstance(state, str) and state == "on"
        cur = ctx.call("set_auto_update", {"on": on})
    if is_raise(cur):
        if is_plugin_operation_error(rkind(cur)):
            f = fail_seam(ctx, console, "[red]Error:[/red] %s" % rmsg(cur))
            if is_raise(f):
                return f
            return mkraise("UnboundLocalError",
                "cannot access local variable 'current' where it is not associated with a value")
        return cur
    if not isinstance(cur, dict):
        return subscript_error(cur)
    if "auto_apply" not in cur:
        return mkraise("KeyError", "'auto_apply'")
    word = "[green]on[/green]" if truthy(cur["auto_apply"]) else "[dim]off[/dim]"
    if "check_hours" not in cur:
        return mkraise("KeyError", "'check_hours'")
    ch = cur["check_hours"]
    if isinstance(ch, bool):
        chv = 1.0 if ch else 0.0
    elif isinstance(ch, (int, float)):
        chv = float(ch)
    else:
        return mkraise("TypeError", "'>' not supported between instances of '%s' and 'int'" % type_name_of(ch))
    if chv > 0:
        cadence = "checked every %sh by the gateway" % format_g(chv)
    else:
        cadence = "the gateway's check is off: plugins.auto_update_check_hours is 0"
    msg = "Plugin auto-update: %s (%s; updates load in new sessions)" % (word, cadence)
    p = console_print(ctx, console, msg)
    if is_raise(p):
        return p
    return None


def pc2_action_pack(ctx, args):
    r = ctx.call("pack_command", {"args": copy.deepcopy(args) if args is not None else {}})
    if is_raise(r):
        return r
    return None


def pc2_action_idle_tasks(ctx, args):
    args = args or {}
    for attr in ("name", "pause", "resume"):
        if attr not in args:
            return mkraise("AttributeError", "'types.SimpleNamespace' object has no attribute '%s'" % attr)
    a = {"name": args["name"], "pause": args["pause"], "resume": args["resume"]}
    r = ctx.call("cmd_idle_tasks", a)
    if is_raise(r):
        return r
    return None


def pc2_refuse_legacy_relay(ctx, console, plugin, legacy_keys, env):
    member = isinstance(legacy_keys, list) and plugin in [jstr(v) for v in legacy_keys]
    if not member:
        return None
    m = ("[red]Plugin '%s' was removed.[/red] Relay lifecycle is owned "
         "by Hermes core; configure a standard user or system Relay plugins.toml, or use "
         "%s for an explicit user-file override.") % (plugin, env)
    f = fail_seam(ctx, console, m)
    if is_raise(f):
        return f
    return None


def pc2_first_toolset(ctx, tool_names):
    if isinstance(tool_names, list):
        for t in tool_names:
            e = ctx.call("registry_get_entry", {"name": jstr(t)})
            if is_raise(e):
                return e
            if not truthy(e):
                continue
            if not isinstance(e, dict):
                return mkraise("AttributeError", "'%s' object has no attribute 'toolset'" % type_name_of(e))
            if "toolset" not in e:
                return mkraise("AttributeError", "'types.SimpleNamespace' object has no attribute 'toolset'")
            if truthy(e["toolset"]):
                return e["toolset"]
    return None


def pc2_from_loaded_plugin(ctx, name):
    dp = ctx.call("discover_plugins", {})
    if is_raise(dp):
        return dp
    mgr = ctx.call("get_plugin_manager", {})
    if is_raise(mgr):
        return mgr
    if mgr is None:
        return mkraise("AttributeError", "'NoneType' object has no attribute '_plugins'")
    if not isinstance(mgr, dict):
        return mkraise("AttributeError", "'%s' object has no attribute '_plugins'" % type_name_of(mgr))
    if "_plugins" not in mgr:
        return mkraise("AttributeError", "'types.SimpleNamespace' object has no attribute '_plugins'")
    pl = mgr["_plugins"]
    if not isinstance(pl, dict):
        return mkraise("AttributeError", "'%s' object has no attribute 'items'" % type_name_of(pl))
    for key, loaded in pl.items():
        if loaded is None:
            return mkraise("AttributeError", "'NoneType' object has no attribute 'manifest'")
        if not isinstance(loaded, dict):
            return mkraise("AttributeError", "'%s' object has no attribute 'manifest'" % type_name_of(loaded))
        if "manifest" not in loaded:
            return mkraise("AttributeError", "'types.SimpleNamespace' object has no attribute 'manifest'")
        manifest = loaded["manifest"]
        if manifest is None:
            return mkraise("AttributeError", "'NoneType' object has no attribute 'name'")
        if not isinstance(manifest, dict):
            return mkraise("AttributeError", "'%s' object has no attribute 'name'" % type_name_of(manifest))
        if "name" not in manifest:
            return mkraise("AttributeError", "'types.SimpleNamespace' object has no attribute 'name'")
        nm = manifest["name"]
        match = (isinstance(nm, str) and nm == name) or key == name
        if match:
            if "tools_registered" not in loaded:
                return mkraise("AttributeError", "'types.SimpleNamespace' object has no attribute 'tools_registered'")
            return pc2_first_toolset(ctx, loaded["tools_registered"])
    return None


def pc2_from_manifest_on_disk(ctx, name):
    b1 = ctx.call("get_bundled_plugins_dir", {})
    if is_raise(b1):
        return b1
    b2 = ctx.call("_plugins_dir", {})
    if is_raise(b2):
        return b2
    for base in (b1, b2):
        if not present(base):
            return mkraise("AttributeError", "'NoneType' object has no attribute 'is_dir'")
        if not isinstance(base, str):
            return mkraise("AttributeError", "'%s' object has no attribute 'is_dir'" % type_name_of(base))
        isd = ctx.call("is_dir", {"path": base})
        if is_raise(isd):
            return isd
        if not truthy(isd):
            continue
        joined = posix_join(base, name)
        isd2 = ctx.call("is_dir", {"path": joined})
        if is_raise(isd2):
            return isd2
        if not truthy(isd2):
            continue
        rm = ctx.call("read_manifest", {"plugin_dir": joined})
        if is_raise(rm):
            return rm
        if not isinstance(rm, dict):
            return mkraise("AttributeError", "'%s' object has no attribute 'get'" % type_name_of(rm))
        pt = rm.get("provides_tools")
        lst = pt if truthy(pt) else None
        ts = pc2_first_toolset(ctx, lst)
        if is_raise(ts):
            return ts
        if truthy(ts):
            return ts
    return None


# ── dispatch ──

def run_case(func, inp):
    ctx = Ctx(inp)
    state = None
    g = inp.get
    if func == "admit_and_save_plugin_sets":
        r = pc2_admit_and_save_plugin_sets(ctx, g("enabled"), g("disabled"), g("extra_dirs"),
                                           g("console"), g("action", "enable"),
                                           g("expected_config"), g("plugin"), g("settled"))
    elif func == "selection_lock":
        locks = copy.deepcopy(g("locks") or {})
        r = pc2_selection_lock(ctx, locks)
        state = {"locks": locks}
    elif func == "set_plugin_enabled":
        locks = copy.deepcopy(g("locks") or {})
        work = {"enabled": [], "disabled": []}
        r = pc2_set_plugin_enabled(ctx, g("name", ""), g("enable", False), g("aliases"),
                                   g("console"), locks, work)
        state = {"locks": locks, "enabled": work["enabled"], "disabled": work["disabled"]}
    elif func == "catalog_installed_dir":
        r = pc2_catalog_installed_dir(ctx, g("name", ""))
    elif func == "find_plugin_entry":
        r = pc2_find_plugin_entry(ctx, g("name", ""))
    elif func == "enable_consent":
        r = pc2_enable_consent(ctx, g("console"), g("key", ""), g("allow_tool_override"))
    elif func == "resolved_user_plugin_dir":
        r = pc2_resolved_user_plugin_dir(ctx, g("name", ""))
    elif func == "catalog":
        r = pc2_catalog(ctx)
    elif func == "action_update":
        r = pc2_action_update(ctx, g("args"))
    elif func == "action_auto_update":
        r = pc2_action_auto_update(ctx, g("args"))
    elif func == "action_pack":
        r = pc2_action_pack(ctx, g("args"))
    elif func == "action_idle_tasks":
        r = pc2_action_idle_tasks(ctx, g("args"))
    elif func == "refuse_legacy_relay":
        r = pc2_refuse_legacy_relay(ctx, g("console"), g("plugin", ""), g("legacy_keys"), g("env", ""))
    elif func == "first_toolset":
        r = pc2_first_toolset(ctx, g("tool_names"))
    elif func == "from_loaded_plugin":
        r = pc2_from_loaded_plugin(ctx, g("name", ""))
    elif func == "from_manifest_on_disk":
        r = pc2_from_manifest_on_disk(ctx, g("name", ""))
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