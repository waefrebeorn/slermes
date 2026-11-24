#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_store.py (gate w25_apiservers_bf168e).

The live module is loaded from the quarry by path; its collaborators -- the
authority registry, the Persistence handle class, ``StoreUnavailable``, the home
constant, the session-retention hook and the media root -- are registered as
stub modules, so no runtime is started and no store is opened.  What is
compared is what the module decides from those values:

  * ``adapter_persistence`` over the adapter's state (runtime bound, cache
    closed, override present and of which kind, home store cached, authority
    carrying a handle);
  * ``selected_session_db`` over authority presence and the cache flag;
  * ``authority_store`` over an authority with and without a handle;
  * ``delete_session``'s ``bool(removed)`` answer.
"""
import contextlib
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_apiservers_bf168e"
MODULE = QUARRY / "gateway" / "platforms" / "api_server_store.py"


class StoreUnavailable(Exception):
    """Stands in for persistence.StoreUnavailable."""


class Persistence:
    """Stands in for persistence_assembly.Persistence (the isinstance test)."""


AUTH_STORE = Persistence()
CACHE_STORE = Persistence()
OVERRIDE_STORE = Persistence()
OTHER = object()

STATE = {"authority": None}


def authority_for_home(runner, home):
    return STATE["authority"]


def stub(name, **attrs):
    module = types.ModuleType(name)
    for key, value in attrs.items():
        setattr(module, key, value)
    sys.modules[name] = module
    return module


gateway_pkg = types.ModuleType("gateway")
gateway_pkg.__path__ = []
sys.modules.setdefault("gateway", gateway_pkg)
stub("gateway.session_authorities", authority_for_home=authority_for_home)
stub("gateway.session_persistence", authority_persistence=lambda a: getattr(a, "store", None))
stub("gateway.session_ingress_media", native_inputs_root=lambda: "/media/root")
stub("persistence_assembly", Persistence=Persistence)
stub("persistence", StoreUnavailable=StoreUnavailable)
stub("hermes_constants", get_hermes_home=lambda: "profile-home")
stub("session_retention", after_sessions_removed=lambda *a, **k: None)


def load_live():
    spec = importlib.util.spec_from_file_location("live_api_server_store", MODULE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


live = load_live()

WANTED = {"adapter_persistence", "selected_session_db", "authority_store", "delete_removed"}
LABELS = {id(AUTH_STORE): "authority_store", id(CACHE_STORE): "cache_store", id(OVERRIDE_STORE): "override_store"}


def set_authority(r):
    STATE["authority"] = None
    if r.get("authority_present"):
        STATE["authority"] = types.SimpleNamespace(
            persistence=AUTH_STORE if r.get("authority_persistence") else None)


def adapter_for(r):
    override = r.get("override")
    if override is None:
        override_obj = None
    elif override == "handle":
        override_obj = OVERRIDE_STORE
    else:
        override_obj = OTHER
    return types.SimpleNamespace(
        gateway_runner=object() if r.get("runtime") else None,
        _session_db_cache_lock=contextlib.nullcontext(),
        _session_db_cache_closed=bool(r.get("cache_closed")),
        _session_db=override_obj,
        _home_stores={"profile-home": CACHE_STORE} if r.get("home_store") else {},
    )


def adapter_persistence(r):
    set_authority(r)
    result = live.adapter_persistence(adapter_for(r), "profile-home")
    if result is None:
        return "none"
    return LABELS.get(id(result), "other")


def selected_session_db(r):
    # The authority always carries a handle here: the decision under test is
    # which of the two early returns fires, not the attribute's presence.
    STATE["authority"] = (types.SimpleNamespace(persistence=AUTH_STORE)
                          if r.get("authority_present") else None)
    result = live.selected_session_db(adapter_for(r), "profile-home")
    if result is None:
        return "none"
    return LABELS.get(id(result), "other")


def authority_store(r):
    authority = types.SimpleNamespace(store=AUTH_STORE if r.get("has_store") else None)
    try:
        result = live.authority_store(authority)
    except StoreUnavailable as exc:
        return {"ok": False, "kind": type(exc).__name__, "error": str(exc)}
    return {"ok": result is AUTH_STORE}


def delete_removed(r):
    removed = r.get("removed")

    class _Sessions:
        def delete(self, session_id, **kwargs):
            return removed

    store = types.SimpleNamespace(sessions=_Sessions(), scope=types.SimpleNamespace(profile_dir="/profile"))
    return live.delete_session(store, "session-id", "/sessions")


def expected(r):
    f = r.get("func")
    if f == "adapter_persistence":
        return adapter_persistence(r)
    if f == "selected_session_db":
        return selected_session_db(r)
    if f == "authority_store":
        return authority_store(r)
    if f == "delete_removed":
        return delete_removed(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
