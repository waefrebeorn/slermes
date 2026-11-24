#!/usr/bin/env python3
"""Oracle for gate w17_sessionhis_4326ac (gateway/session_history_mount.py).

Drives the LIVE functions (AST-extracted) against scripted persistence/stub
collaborators.  Compares `out` = {"ret":..., "state":...}.
"""
import __future__
import ast
import builtins
import json
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "gateway" / "session_history_mount.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "gateway" / "session_history_mount.py")
NAME = "w17_sessionhis_4326ac"
PORTED = ["_reader", "mount_history", "is_history_mount", "require_history_leaf",
          "read_only", "require_history_access"]

class AuthorityRefusal(Exception):
    def __init__(self, reason=None, field="", detail="", **kw):
        super().__init__(reason or "")
        self.reason, self.field, self.detail = reason, field, detail

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

for _pn in ("gateway", "persistence", "persistence.core", "persistence.authority", "session_rules"):
    if _pn not in sys.modules:
        _m = types.ModuleType(_pn)
        _m.__path__ = []
        sys.modules[_pn] = _m
sys.modules["persistence.core"].errors = _mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
sys.modules["persistence.authority"].vocabulary = _mod(
    "persistence.authority.vocabulary",
    LEGACY_LOCAL_SOURCES=frozenset({"cli", "tui", "desktop", "webui"}))
_mod("session_rules.legacy_local", binding_names=lambda *a, **k: False)
_mod("persistence_assembly", store_owner_name=lambda p: None)
_mod("gateway.session_authority", LiveSession=lambda source, route: types.SimpleNamespace(source=source, route=route))
_sp = _mod("gateway.session_persistence", authority_persistence=lambda a: None, owner_rule=lambda: "owner")

_STATE = {"io": {}, "state": {}}

def authority_persistence(authority):
    return _STATE["store"]

def store_owner_name(profile_dir):
    return _STATE["io"].get("store_owner_name")

def binding_names(binding, subject, rule):
    return bool(_STATE["io"].get("binding_names", False))

_sp.authority_persistence = authority_persistence
_sp.owner_rule = lambda: "owner"
sys.modules["persistence_assembly"].store_owner_name = store_owner_name
sys.modules["session_rules.legacy_local"].binding_names = binding_names

_ns = {
    "__name__": "w17_sessionhis_live", "__builtins__": builtins,
    "AuthorityRefusal": AuthorityRefusal,
    "authority_persistence": authority_persistence,
    "owner_rule": lambda: "owner",
    "LEGACY_LOCAL_SOURCES": sys.modules["persistence.authority.vocabulary"].LEGACY_LOCAL_SOURCES,
    "store_owner_name": store_owner_name,
    "_ROUTE_PREFIX": "history:",
}
_live = types.ModuleType("w17_sessionhis_live")
_live.__dict__.update(_ns)
sys.modules["w17_sessionhis_live"] = _live
_ns = _live.__dict__

_tree = ast.parse(Path(SRC).read_text(encoding="utf-8"), SRC)
_flags = __future__.annotations.compiler_flag
_seen = set()
for _node in _tree.body:
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in PORTED:
        exec(compile(ast.Module(body=[_node], type_ignores=[]), SRC, "exec",
                     flags=_flags, dont_inherit=True), _ns)
        _seen.add(_node.name)
missing = set(PORTED) - _seen
if missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(missing)}", file=sys.stderr)
    sys.exit(1)

# ---- doubles ---------------------------------------------------------------
def make_store(io):
    row = io.get("row") or {}
    class Bindings:
        def history_binding(self, sid):
            return io.get("history_binding")
        def api_binding(self, sid):
            ab = io.get("api_binding")
            if isinstance(ab, dict) and "$raise" in ab:
                raise ValueError(ab.get("msg", "bad binding"))
            return ab
    class Sessions:
        def get(self, sid):
            return types.SimpleNamespace(source=row.get("source"), profile_name=row.get("profile_name"))
    class Lineage:
        def lineage(self, sid):
            _STATE["state"].setdefault("leaf_calls", []).append(
                {"op": "lineage.lineage", "session_id": sid})
            return list(io.get("chain") or [])
    fp = io.get("footprint") or {}
    class Runtime:
        def delete_footprint(self, sid, owner=None):
            _STATE["state"].setdefault("leaf_calls", []).append(
                {"op": "runtime.delete_footprint", "session_id": sid, "owner": "owner"})
            return types.SimpleNamespace(targets=fp.get("targets") or [],
                                         children=fp.get("children") or [])
    return types.SimpleNamespace(bindings=Bindings(), sessions=Sessions(),
        lineage=Lineage(), runtime=Runtime(),
        scope=types.SimpleNamespace(profile_dir=io.get("profile_dir")))

def norm(v):
    if v is None or isinstance(v, (bool, str, int, float)):
        return v
    if isinstance(v, (list, tuple, set, frozenset)):
        if isinstance(v, (set, frozenset)):
            return sorted(norm(x) for x in v)
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, AuthorityRefusal):
        return {"__raise__": "AuthorityRefusal", "reason": v.reason,
                "field": v.field or "", "detail": v.detail or ""}
    return v

def drive(r):
    io = r.get("io") or {}
    state = {}
    _STATE["io"] = io
    _STATE["state"] = state
    _STATE["store"] = make_store(io)

    sessions = {sid: types.SimpleNamespace(**v) for sid, v in (io.get("sessions") or {}).items()}
    class RecordingDict(dict):
        def setdefault(self, key, default=None):
            _STATE["state"].setdefault("mount_calls", []).append(
                {"op": "sessions.setdefault", "session_id": key,
                 "route": getattr(default, "route", None)})
            return super().setdefault(key, default)
    authority = types.SimpleNamespace(
        sessions=RecordingDict(sessions),
        _native_legacy_transports=io.get("_native_legacy_transports") or {})
    def require_open():
        state.setdefault("mount_calls", []).append({"op": "_require_admission_open"})
    authority._require_admission_open = require_open

    actor_json = r.get("actor") or {}
    actor = types.SimpleNamespace(
        subject=actor_json.get("subject"), capabilities=actor_json.get("capabilities") or [],
        transport_id=actor_json.get("transport_id"), id=actor_json.get("id"))
    # _native_legacy_transports values equal to the actor id denote the actor itself
    src_nt = (r.get("authority") or {}).get("_native_legacy_transports") or io.get("_native_legacy_transports") or {}
    nt = {k: (actor if v == actor_json.get("id") else v) for k, v in src_nt.items()}
    authority._native_legacy_transports = nt

    sid = r.get("session_id", "")
    func = r.get("func")
    ret = None
    try:
        if func == "reader":
            ret = _ns["_reader"](authority, actor, sid)
        elif func == "mount_history":
            ret = _ns["mount_history"](authority, actor, sid)
        elif func == "is_history_mount":
            if r.get("live") is None:
                ret = False
            else:
                ret = _ns["is_history_mount"](types.SimpleNamespace(**r.get("live")))
        elif func == "require_history_leaf":
            ret = _ns["require_history_leaf"](authority, sid)
        elif func == "read_only":
            ret = _ns["read_only"]()
        elif func == "require_history_access":
            ret = _ns["require_history_access"](authority, actor, sid, r.get("capability", ""))
        else:
            raise KeyError(func)
        ret = norm(ret)
    except AuthorityRefusal as e:
        ret = norm(e)
    except BaseException as e:
        ret = {"__raise__": type(e).__name__}
    return {"ret": ret, "state": norm(state)}

def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        n += 1
        try:
            py = drive(r)
        except BaseException as e:
            py = {"ret": {"__raise__": type(e).__name__}, "state": {}}
        got = r.get("out")
        if got != py:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {r.get('func')} case {n}:", file=sys.stderr)
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}", file=sys.stderr)
                print(f"  P={json.dumps(py, ensure_ascii=False)[:900]}", file=sys.stderr)
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)

if __name__ == "__main__":
    main()
