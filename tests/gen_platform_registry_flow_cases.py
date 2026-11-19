#!/usr/bin/env python3
"""Generate tests/fixtures/platform_registry_flow.jsonl for the FLOW port of
gateway/platform_registry.py. >=5 cases per ported function (22 functions).

Case shape: {"func": <live_name>, "in": {...}} where in.io carries universal-seam
scripts (constant or per-call queue; list RESULTS wrapped {"__script__":true,
"ret":[...]}), in.state carries the flat registry carrier, and the remaining keys
are the call arguments / import-gate flags.

Domain (ASCII): load keys encode as compact JSON [scope,name]; entry records carry
__id__ first plus the fields the flows read; loader markers {"__loader__":id};
identity refs {"__ref__":id}; callable markers {"__callable__":seam}. The
configured flows' multi-name unresolved fixtures script wanted FALSY for every
name (any() exhausts the set; trailing wanted events sorted on both sides);
truthy/raise wanted cases use single-name unresolved. hermes_home_key scripts are
strings (a null scope would diverge _scope_maps None-vs-"" semantics).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "platform_registry_flow.jsonl"


def entry(eid, name, **kw):
    d = {
        "__id__": eid,
        "name": name,
        "label": kw.get("label", name.upper()),
        "adapter_factory": {"__callable__": kw.get("af", "af_" + eid)},
        "check_fn": kw.get("check_fn", {"__callable__": kw.get("cf", "cf_" + eid)}),
        "source": kw.get("source", "plugin"),
        "required_env": kw.get("required_env", []),
        "install_hint": kw.get("install_hint", ""),
    }
    if "ensure_deps_fn" in kw:
        d["ensure_deps_fn"] = kw["ensure_deps_fn"]
    if "validate_config" in kw:
        d["validate_config"] = kw["validate_config"]
    return d


def loader(lid):
    return {"__loader__": lid}


def key(scope, name):
    return json.dumps([scope, name], separators=(",", ":"))


def ref(rid):
    return {"__ref__": rid}


def cb(seam):
    return {"__callable__": seam}


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_spec(kind, msg):
    return {"__raise__": kind, "message": msg}


def state(**kw):
    st = {
        "_entries": {}, "_scoped_entries": {}, "_deferred": {}, "_scoped_deferred": {},
        "_inflight": {}, "_inflight_loaders": {}, "_cancelled_inflight": [],
        "_consumed_loaders": {}, "_failed_loads": [], "_activation_env": {}, "_loading": [],
    }
    st.update(kw)
    return st


CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


E1 = entry("e1", "irc")
E2 = entry("e2", "tg")
E3 = entry("e3", "wx", source="builtin")
L1, L2, L3 = loader("L1"), loader("L2"), loader("L3")

# ---- _plugin_scope_from_callable ----
case("_plugin_scope_from_callable", callback={"__attr__": {"__module__": "plugins.foo"}},
     io={"plugin_scope_for_callable": "scopeA"})
case("_plugin_scope_from_callable", callback={"__attr__": {}},
     io={"plugin_scope_for_callable": None})
case("_plugin_scope_from_callable", callback={"__attr__": {"__module__": "x"}}, import_fails=True)
case("_plugin_scope_from_callable", callback={"__attr__": {"__module__": "x"}}, attr_missing=True)
case("_plugin_scope_from_callable", callback={"__attr__": {"__module__": "x"}},
     io={"plugin_scope_for_callable": raise_spec("ImportError", "no mod")})
case("_plugin_scope_from_callable", callback={"__attr__": {"__module__": "x"}},
     io={"plugin_scope_for_callable": raise_spec("AttributeError", "no attr")})
case("_plugin_scope_from_callable", callback={"__attr__": {"__module__": "x"}},
     io={"plugin_scope_for_callable": raise_spec("RuntimeError", "boom")})
case("_plugin_scope_from_callable", callback={"__attr__": {"__module__": "x"}},
     io={"plugin_scope_for_callable": raise_spec("SystemExit", "1")})
case("_plugin_scope_from_callable", callback={"__attr__": {"__module__": "x"}})

# ---- _caller_plugin_scope ----
case("_caller_plugin_scope", io={"sys._getframe": {"f_globals": {"__name__": "plugins.bar"}},
                                 "plugin_scope_for_callable": "scopeB"})
case("_caller_plugin_scope", io={"sys._getframe": {"f_globals": {"__name__": ""}},
                                 "plugin_scope_for_callable": None})
case("_caller_plugin_scope", io={"sys._getframe": {"f_globals": {}},
                                 "plugin_scope_for_callable": None})
case("_caller_plugin_scope", io={"sys._getframe": raise_spec("ValueError", "call stack is not deep enough")})
case("_caller_plugin_scope", io={"sys._getframe": raise_spec("SystemExit", "1")})
case("_caller_plugin_scope", io={"sys._getframe": None})
case("_caller_plugin_scope", io={"sys._getframe": {"f_globals": None}})
case("_caller_plugin_scope", import_fails=True,
     io={"sys._getframe": {"f_globals": {"__name__": "plugins.bar"}}})
case("_caller_plugin_scope")

# ---- core_ships_platform ----
PLAT = {"member_names": ["telegram", "discord"], "values": {"telegram": "telegram", "discord": "discord"}}
case("core_ships_platform", name="Telegram", platform=PLAT)
case("core_ships_platform", name=" discord ", platform=PLAT)
case("core_ships_platform", name="wechat", platform=PLAT,
     io={"platform_scan_bundled": script(["wechat", "qq"])})
case("core_ships_platform", name="nope", platform=PLAT, io={"platform_scan_bundled": script(["wechat"])})
case("core_ships_platform", name=None, platform=PLAT, io={"platform_scan_bundled": script([])})
case("core_ships_platform", name=5, platform=PLAT, io={"platform_scan_bundled": script(["5"])})
case("core_ships_platform", name="x", platform_import_fails=True)
case("core_ships_platform", name="x", platform=PLAT, io={"platform_scan_bundled": raise_spec("RuntimeError", "scan fail")})
case("core_ships_platform", name="x", platform=PLAT, io={"platform_scan_bundled": None})
case("core_ships_platform", name="x", platform={"member_names": ["ghost"], "values": {}})
case("core_ships_platform", name="x", platform=PLAT)

# ---- __init__ ----
case("__init__")
case("__init__", state={"_entries": {"x": E1}})
case("__init__", state=state(_deferred={"qq": L1}))
case("__init__", io={"hermes_home_key": "/h"})
case("__init__", state={"off_loop_deps": True})

# ---- current_scope_key ----
case("current_scope_key", io={"hermes_home_key": "/h/p1"})
case("current_scope_key", io={"hermes_home_key": ""})
case("current_scope_key", io={"hermes_home_key": None})
case("current_scope_key", io={"hermes_home_key": raise_spec("RuntimeError", "no home")})
case("current_scope_key", io={"hermes_home_key": raise_spec("KeyboardInterrupt", "")})
case("current_scope_key")

# ---- _scope_maps ----
SM = state(_entries={"irc": E1}, _deferred={"qq": L1},
           _scoped_entries={"s1": {"tg": E2}}, _scoped_deferred={"s1": {"wx": L2}})
case("_scope_maps", scope=None, create=False, state=SM)
case("_scope_maps", scope="s1", create=False, state=SM)
case("_scope_maps", scope="s2", create=False, state=SM)
case("_scope_maps", scope="s2", create=True, state=SM)
case("_scope_maps", scope="s1", create=True, state=SM)
case("_scope_maps", scope=None, create=True, state=SM)
case("_scope_maps", scope="", create=True, state=SM)

# ---- _registration_state ----
RS = state(_entries={"irc": E1}, _deferred={"qq": L1},
           _inflight_loaders={key("s1", "tg"): L2}, _consumed_loaders={key(None, "wx"): L3})
case("_registration_state", scope=None, name="irc", state=RS)
case("_registration_state", scope=None, name="qq", state=RS)
case("_registration_state", scope=None, name="wx", state=RS)
case("_registration_state", scope="s1", name="tg", state=RS)
case("_registration_state", scope="s1", name="absent", create=True, state=RS)
case("_registration_state", scope=None, name="irc",
     state=state(_entries={"irc": E1}, _deferred={"irc": L1}))
case("_registration_state", scope="s2", name="x", create=False, state=RS)

# ---- _prune_scope ----
PS = state(_scoped_entries={"s1": {}, "s2": {"x": E1}}, _scoped_deferred={"s1": {}, "s2": {}})
case("_prune_scope", scope="s1", state=PS)
case("_prune_scope", scope="s2", state=PS)
case("_prune_scope", scope=None, state=PS)
case("_prune_scope", scope="absent", state=PS)
case("_prune_scope", scope="s1",
     state=state(_scoped_entries={"s1": {"x": E1}}, _scoped_deferred={"s1": {}}))
case("_prune_scope", scope="s1",
     state=state(_scoped_entries={"s1": {"x": E1}}, _scoped_deferred={"s1": {"y": L1}}))

# ---- note_activation_env ----
AE = state(_activation_env={"irc": ["A", "B"]})
case("note_activation_env", name="irc", env_names=["C", " B "], state=AE)
case("note_activation_env", name="irc", env_names=["", "  "], state=AE)
case("note_activation_env", name="new", env_names=["X"], state=AE)
case("note_activation_env", name="irc", env_names=None, state=AE)
case("note_activation_env", name="irc", env_names=[5, True, None], state=AE)
case("note_activation_env", name="irc", env_names=["A", "A"], state=AE)
case("note_activation_env", name="irc", env_names=[], state=AE)

# ---- activation_env ----
case("activation_env", name="irc", state=state(_activation_env={"irc": ["B", "A"]}))
case("activation_env", name="absent", state=state(_activation_env={"irc": ["B", "A"]}))
case("activation_env", name="irc", state=state(_activation_env={"irc": []}))
case("activation_env", name="irc", state=state())
case("activation_env", name="x", state=state(_activation_env={"x": ["Z", "Y", "X"]}))

# ---- snapshot_registration ----
case("snapshot_registration", name="irc", scope=None, state=RS)
case("snapshot_registration", name="qq", scope=None, state=RS)
case("snapshot_registration", name="tg", scope="s1", state=RS)
case("snapshot_registration", name="wx", scope=None, state=RS)
case("snapshot_registration", name="x", scope="s2", state=RS)
case("snapshot_registration", name="absent", scope=None, state=RS)

# ---- restore_registration ----
RR = state(_entries={"irc": E1, "tg": E2}, _deferred={"irc": L1},
           _inflight={key(None, "irc"): {"__event__": "ev1"}},
           _consumed_loaders={key(None, "irc"): L3})
case("restore_registration", name="irc", scope=None, current=[ref("e1"), ref("L1")],
     previous=[None, None], state=RR)
case("restore_registration", name="irc", scope=None, current=[ref("e2"), ref("L1")],
     previous=[None, None], state=RR)
case("restore_registration", name="irc", scope=None, current=[ref("e1"), ref("L2")],
     previous=[None, None], state=state(_entries={"irc": E1}, _deferred={"irc": L1, "tg": L2}))
case("restore_registration", name="irc", scope=None, current=[ref("e1"), ref("L1")],
     previous=[ref("e2"), None], state=RR)
case("restore_registration", name="tg", scope="s1", current=[ref("e2"), ref("L2")],
     previous=[None, ref("L1")],
     state=state(_deferred={"irc": L1}, _scoped_entries={"s1": {"tg": E2}},
                 _scoped_deferred={"s1": {"tg": L2}},
                 _inflight={key("s1", "tg"): {"__event__": "ev2"}}))
case("restore_registration", name="absent", scope=None, current=[None, None],
     previous=[None, None], state=state())
case("restore_registration", name="irc", scope=None, current=[ref("e1"), ref("L1")],
     previous=[E3, L3], state=RR)
case("restore_registration", name="x", scope="s2", current=[ref("e1"), None],
     previous=[None, None], state=state(_entries={"other": E1}))

# ---- _pending_names ----
PN = state(_deferred={"qq": L1, "irc": L2}, _scoped_deferred={"s1": {"tg": L3, "wx": L1}},
           _inflight={key("s1", "aa"): {"__event__": "e"}, key(None, "bb"): {"__event__": "e"},
                      key("s2", "cc"): {"__event__": "e"}})
case("_pending_names", active_scope="s1", state=PN)
case("_pending_names", active_scope="s2", state=PN)
case("_pending_names", active_scope="s3", state=PN)
case("_pending_names", active_scope="s1",
     state=state(_deferred={"tg": L1, "qq": L2}, _scoped_deferred={"s1": {"tg": L3, "wx": L1}},
                 _inflight={key("s1", "aa"): {"__event__": "e"}, key(None, "bb"): {"__event__": "e"}}))
case("_pending_names", active_scope="s1", state=state())
case("_pending_names", active_scope="s1",
     state=state(_inflight={key("s1", "only"): {"__event__": "e"}}))

# ---- pending_names ----
case("pending_names", io={"hermes_home_key": "s1"}, state=PN)
case("pending_names", io={"hermes_home_key": "s2"}, state=PN)
case("pending_names", io={"hermes_home_key": ""}, state=PN)
case("pending_names", io={"hermes_home_key": raise_spec("RuntimeError", "no home")}, state=PN)
case("pending_names", io={"hermes_home_key": "s3"}, state=state())
case("pending_names", state=PN)

# ---- configured_entries / configured_load ----
CE = state(_entries={"irc": E1}, _scoped_entries={"s1": {"tg": E2}},
           _consumed_loaders={key(None, "wx"): L3}, _failed_loads=[key("s1", "qq")])
CE_IO = {"hermes_home_key": "s1", "self._resolve_all": None, "wanted": False}
case("configured_entries", wanted=cb("wanted"), io=CE_IO, state=CE)
case("configured_entries", wanted=cb("wanted"),
     io={"hermes_home_key": "s1", "self._resolve_all": None, "wanted": True},
     state=state(_entries={"irc": E1}, _scoped_entries={"s1": {"tg": E2}},
                 _consumed_loaders={key(None, "wx"): L3}))
case("configured_entries", wanted=cb("wanted"),
     io={"hermes_home_key": "s1", "self._resolve_all": None, "wanted": False},
     state=state(_entries={"irc": E1}, _scoped_entries={"s1": {"tg": E2}}))
case("configured_entries", wanted=cb("wanted"),
     io={"hermes_home_key": "s1", "self._resolve_all": raise_spec("RuntimeError", "resolve fail"), "wanted": False},
     state=CE)
case("configured_entries", wanted=cb("wanted"),
     io={"hermes_home_key": raise_spec("SystemExit", "1"), "self._resolve_all": None, "wanted": False},
     state=CE)
case("configured_entries", wanted=cb("wanted"),
     io={"hermes_home_key": "s1", "self._resolve_all": None, "wanted": raise_spec("RuntimeError", "wanted fail")},
     state=state(_entries={"irc": E1}, _consumed_loaders={key(None, "wx"): L3}))
case("configured_entries", wanted=cb("wanted"), io={"hermes_home_key": "s1"}, state=CE)

CL_IO = {"hermes_home_key": "s1", "self._resolve_all": None, "wanted": False}
case("configured_load", wanted=cb("wanted"), io=CL_IO, state=CE)
case("configured_load", wanted=cb("wanted"),
     io={"hermes_home_key": "s1", "self._resolve_all": None, "wanted": True},
     state=state(_entries={"irc": E1}, _scoped_entries={"s1": {"tg": E2}},
                 _consumed_loaders={key(None, "wx"): L3}))
case("configured_load", wanted=cb("wanted"),
     io={"hermes_home_key": "s1", "self._resolve_all": None, "wanted": False},
     state=state(_entries={"irc": E1}, _scoped_entries={"s1": {"tg": E2}}))
case("configured_load", wanted=cb("wanted"),
     io={"hermes_home_key": "s1", "self._resolve_all": None, "wanted": False},
     state=state(_entries={"irc": E1, "wx": E3}, _consumed_loaders={key(None, "wx"): L3}))
case("configured_load", wanted=cb("wanted"),
     io={"hermes_home_key": "s1", "self._resolve_all": raise_spec("ValueError", "x"), "wanted": False},
     state=CE)
case("configured_load", wanted=cb("wanted"),
     io={"hermes_home_key": "s1", "self._resolve_all": None, "wanted": raise_spec("KeyboardInterrupt", "")},
     state=state(_entries={"irc": E1}, _consumed_loaders={key(None, "wx"): L3}))
case("configured_load", wanted=cb("wanted"),
     io={"hermes_home_key": "s1", "self._resolve_all": None, "wanted": False},
     state=state(_entries={"irc": E1}, _consumed_loaders={key("s2", "xx"): L3}))

# ---- loaded_entries ----
LE = state(_entries={"irc": E1, "qq": E2}, _scoped_entries={"s1": {"irc": E3, "tg": E2}})
case("loaded_entries", io={"hermes_home_key": "s1"}, state=LE)
case("loaded_entries", io={"hermes_home_key": "s2"}, state=LE)
case("loaded_entries", io={"hermes_home_key": "s1"}, state=state())
case("loaded_entries", io={"hermes_home_key": raise_spec("RuntimeError", "x")}, state=LE)
case("loaded_entries", io={"hermes_home_key": ""}, state=LE)
case("loaded_entries", state=LE)

# ---- forget_failed_loads ----
FF = state(_failed_loads=[key(None, "a"), key("s1", "b"), key("s2", "c"), key("s1", "d")])
case("forget_failed_loads", scope="s1", state=FF)
case("forget_failed_loads", scope=None, state=FF)
case("forget_failed_loads", scope="s3", state=FF)
case("forget_failed_loads", scope="s2", state=FF)
case("forget_failed_loads", scope="s1", state=state())
case("forget_failed_loads", scope="", state=FF)

# ---- _load_pending ----
LP = state(_entries={"irc": E1}, _deferred={"qq": L1}, _scoped_deferred={"s1": {"tg": L2}},
           _inflight={key(None, "wx"): {"__event__": "e"}, key("s1", "aa"): {"__event__": "e"}})
case("_load_pending", scope=None, name="qq", state=LP)
case("_load_pending", scope=None, name="irc", state=LP)
case("_load_pending", scope=None, name="wx", state=LP)
case("_load_pending", scope="s1", name="tg", state=LP)
case("_load_pending", scope="s1", name="aa", state=LP)
case("_load_pending", scope="s1", name="qq", state=LP)
case("_load_pending", scope="s2", name="zz", state=LP)

# ---- required_env_names ----
RE = state(_entries={"irc": entry("e1", "irc", required_env=["A", "B", 5])},
           _scoped_entries={"s1": {"tg": entry("e2", "tg", required_env=["B", "C", None])}})
case("required_env_names", io={"hermes_home_key": "s1"}, state=RE)
case("required_env_names", io={"hermes_home_key": "s2"}, state=RE)
case("required_env_names", io={"hermes_home_key": "s1"}, state=state())
case("required_env_names", io={"hermes_home_key": raise_spec("RuntimeError", "x")}, state=RE)
case("required_env_names", io={"hermes_home_key": "s1"},
     state=state(_entries={"irc": entry("e1", "irc", required_env=[1, True, None])}))
case("required_env_names", io={"hermes_home_key": "s1"},
     state=state(_entries={"irc": entry("e1", "irc", required_env=["A"])},
                 _scoped_entries={"s1": {"tg": entry("e2", "tg", required_env=["A", "B"])}}))

# ---- deps_resolved_off_loop ----
case("deps_resolved_off_loop", deps_ok=True, state={"off_loop_deps": None})
case("deps_resolved_off_loop", deps_ok=False, state={"off_loop_deps": True})
case("deps_resolved_off_loop", deps_ok=None, state={"off_loop_deps": None})
case("deps_resolved_off_loop", deps_ok=True, state={"off_loop_deps": None},
     body_raise=raise_spec("RuntimeError", "boom"))
case("deps_resolved_off_loop", deps_ok=True, state={"off_loop_deps": False},
     body_raise=raise_spec("SystemExit", "1"))
case("deps_resolved_off_loop", deps_ok=True, state={"off_loop_deps": "x"})

# ---- ensure_deps ----
ED_ENTRY = entry("e1", "irc", label="IRC", cf="cf1", ensure_deps_fn=cb("ed1"),
                 install_hint="pip install x")
ED_NOINSTALL = entry("e1", "irc", label="IRC", cf="cf1", install_hint="")
GET_ED = {"self.get": ED_ENTRY}
case("ensure_deps", name="irc", io={"self.get": ED_ENTRY, "cf1": True})
case("ensure_deps", name="irc", io={"self.get": ED_ENTRY, "cf1": False, "ed1": True,
                                    "logger.info": None})
case("ensure_deps", name="irc", io={"self.get": ED_ENTRY, "cf1": False, "ed1": False,
                                    "logger.info": None, "logger.warning": None})
case("ensure_deps", name="irc", io={"self.get": ED_NOINSTALL, "cf1": False, "logger.warning": None})
case("ensure_deps", name="irc", io={"self.get": ED_NOINSTALL, "cf1": raise_spec("RuntimeError", "boom"),
                                    "logger.warning": None})
case("ensure_deps", name="irc", io={"self.get": ED_ENTRY, "cf1": raise_spec("SystemExit", "1")})
case("ensure_deps", name="irc", io={"self.get": None})
case("ensure_deps", name="irc", off_loop_deps=True, io=GET_ED)
case("ensure_deps", name="irc", off_loop_deps=False, io=GET_ED)
case("ensure_deps", name="irc", io={"self.get": raise_spec("RuntimeError", "get fail")})
case("ensure_deps", name="irc",
     io={"self.get": ED_ENTRY, "cf1": False, "ed1": raise_spec("ValueError", "install boom"),
         "logger.info": None, "logger.warning": None})
case("ensure_deps", name="irc",
     io={"self.get": entry("e1", "irc", label="IRC", check_fn=None, install_hint=""),
         "logger.warning": None})
case("ensure_deps", name="irc")
case("ensure_deps", name="irc",
     io={"self.get": ED_NOINSTALL, "cf1": raise_spec("RuntimeError", "boom"),
         "logger.warning": raise_spec("RuntimeError", "log fail")})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  (<5!)"
        print("  %-32s %d%s" % (fn, counts[fn], flag))
    short = [fn for fn, n in counts.items() if n < 5]
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)


if __name__ == "__main__":
    main()
