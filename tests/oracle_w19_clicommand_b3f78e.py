#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_commands_handoff.py (gate w19_clicommand_b3f78e).

The live module is a CLI mixin whose imports reach cli.py, so the oracle execs the
module-level line helper and the CLIHandoffMixin ClassDef against stub globals: the
console writer (_cp), the i18n lookup (_t, identity), a virtual clock, and injected
store/session-DB/gateway-config seams. Every printed line is recorded and compared.
"""
import ast, contextlib, json, sys, types, __future__
from pathlib import Path

for _s in (sys.stdin, sys.stdout):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
GATE = "w19_clicommand_b3f78e"
SRC = Path("C:/Users/eman5/hermes-agent/hermes_cli/cli_commands_handoff.py")
_ANN = __future__.annotations.compiler_flag
LINES = []


def _cp(*lines):
    LINES.extend(str(x) for x in lines)


def _t(key, **kw):
    return key if isinstance(key, str) else str(key)


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "handoff_live", "suppress": contextlib.suppress,
         "time": types.SimpleNamespace(time=lambda: 1000.0, sleep=lambda s: None),
         "_cp": _cp, "_t": _t, "_command_arg": lambda s: s.strip(), "Optional": object}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_db_unavailable_line":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name == "CLIHandoffMixin":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
MIX = G["CLIHandoffMixin"]

# storage_user_copy.format_session_db_unavailable is a text seam; identity on both sides.
_m = types.ModuleType("storage_user_copy")
_m.format_session_db_unavailable = lambda details=False: "DB_UNAVAILABLE"
sys.modules["storage_user_copy"] = _m
_cli = types.ModuleType("cli")
_cli._handed_off_session_ids = set()
sys.modules["cli"] = _cli
_gw = types.ModuleType("gateway")
_gw.__path__ = []
sys.modules["gateway"] = _gw


def _self(r):
    obj = MIX.__new__(MIX)
    obj.session_id = r.get("session_id", "abcdef1234567890")
    obj._agent_running = bool(r.get("agent_running", False))
    obj._session_db = r.get("_db_stub")
    obj._should_exit = False
    return obj


def _session_store(r):
    state = {"title": r.get("title_value", ""), "exists": bool(r.get("session_exists", False))}

    class Sessions:
        def get(self, sid):
            return {"title": state["title"]} if state["exists"] else None

        def set_title(self, sid, title):
            if r.get("set_title_raises"):
                raise RuntimeError("db down")
            state["exists"] = True
            state["title"] = title

        def get_title(self, sid):
            if r.get("get_title_raises"):
                raise RuntimeError("db down")
            return state["title"]

    class Store:
        def __init__(self):
            self.sessions = Sessions()

    return Store()


def _session_db(r):
    state = {"title": r.get("title_value", ""), "exists": bool(r.get("session_exists", False))}

    class DB:
        def get_session(self, sid):
            return {"title": state["title"]} if state["exists"] else None

        def set_session_title(self, sid, title):
            if r.get("set_title_raises"):
                raise RuntimeError("db down")
            state["exists"] = True
            state["title"] = title

    return DB()


def run_prepare(r):
    obj = _self(r)
    store = _session_store(r) if r.get("store_present") else None
    obj._session_db = _session_db(r) if r.get("session_db_present") else None
    res = MIX._handoff_prepare_session(obj, store)
    return {"result": res, "lines": list(LINES)}


def run_validate(r):
    outcome = r.get("outcome", "ok")

    class HandoffTargetError(Exception):
        def __init__(self, reason):
            super().__init__(reason)
            self.reason = reason

    def resolve_handoff_home(platform):
        if outcome == "ok":
            return types.SimpleNamespace(name=r.get("home_name", "HomeChan"))
        if outcome == "config_error":
            raise ValueError("config unreadable")
        raise HandoffTargetError(outcome)

    m = types.ModuleType("gateway.handoff_target")
    m.HandoffTargetError = HandoffTargetError
    m.resolve_handoff_home = resolve_handoff_home
    sys.modules["gateway.handoff_target"] = m
    home = MIX._handoff_validate_target(_self(r), r.get("platform_name", "telegram"))
    name = getattr(home, "name", None) if home is not None else None
    return {"result": name, "lines": list(LINES)}


def run_wait(r):
    states = r.get("states") or []
    clock = [1000.0]
    pos = [0]
    used = [0]
    reads = [0]
    sleeps = [0]
    fails = []

    G["time"] = types.SimpleNamespace(time=lambda: clock[0],
                                      sleep=lambda s: (clock.__setitem__(0, clock[0] + s),
                                                       sleeps.__setitem__(0, sleeps[0] + 1)))

    def read_state():
        reads[0] += 1
        n = len(states)
        if not n:
            return None
        while pos[0] < n:
            entry = states[pos[0]]
            count = int(entry.get("n", 1)) if isinstance(entry, dict) else 1
            if used[0] >= count:
                pos[0] += 1
                used[0] = 0
                continue
            used[0] += 1
            if isinstance(entry, dict) and entry.get("raise"):
                raise RuntimeError("state unreadable")
            return entry
        return states[n - 1]

    def fail_pending(reason):
        if r.get("fail_pending_raises"):
            raise RuntimeError("db down")
        fails.append(reason)

    obj = _self(r)
    res = MIX._handoff_wait(obj, r.get("platform_name", "telegram"),
                            r.get("session_title", "handoff-abcdef12"),
                            read_state=read_state, fail_pending=fail_pending)
    return {"result": bool(res), "lines": list(LINES), "should_exit": bool(obj._should_exit),
            "reads": reads[0], "fail_calls": fails,
            "sleeps": sleeps[0]}


def run_case(r):
    del LINES[:]
    f = r.get("func")
    if f == "keep":
        res = MIX._handoff_keep(*list(r.get("lines") or []))
        return {"result": bool(res), "lines": list(LINES)}
    if f == "prepare_session":
        return run_prepare(r)
    if f == "validate_target":
        return run_validate(r)
    if f == "handoff_wait":
        return run_wait(r)
    return None


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
        f = r.get("func")
        if f not in {"keep", "prepare_session", "validate_target", "handoff_wait"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
