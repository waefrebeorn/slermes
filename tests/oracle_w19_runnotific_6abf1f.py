#!/usr/bin/env python3
"""Differential oracle for gateway/run_notifications_update.py (gate w19_runnotific_6abf1f)."""
import ast, contextlib, asyncio, logging, dataclasses, datetime as _dt, json, sys, types, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = Path("C:/Users/eman5/hermes-agent")
if str(Q) not in sys.path:
    sys.path.insert(0, str(Q))
GATE = "w19_runnotific_6abf1f"
SRC = Q / "gateway" / "run_notifications_update.py"
_ANN = __future__.annotations.compiler_flag

# gate.run is stubbed (only _hermes_home/_non_conversational_metadata are touched); the real
# gateway.session (profile_from_session_key_namespace) is imported lazily by the marker helper.
if "gateway" not in sys.modules:
    _pkg = types.ModuleType("gateway")
    _pkg.__path__ = []
    sys.modules["gateway"] = _pkg
_gw_run = types.ModuleType("gateway.run")
_gw_run._hermes_home = Path("C:/hermes-home")
_gw_run._non_conversational_metadata = lambda md, **kw: md
sys.modules["gateway.run"] = _gw_run

# gateway.session only re-exports this from persistence.core.keys; installing the module keeps
# the marker helper's lazy import off the heavy gateway package graph.
from persistence.core.keys import profile_from_session_key_namespace as _pfskn
_gs = types.ModuleType("gateway.session")
_gs.profile_from_session_key_namespace = _pfskn
sys.modules["gateway.session"] = _gs

NOW = [""]  # injected clock for datetime.now(...)


class FakeDateTime:
    @staticmethod
    def fromisoformat(s):
        return _dt.datetime.fromisoformat(s)

    @staticmethod
    def now(tz=None):
        base = _dt.datetime.fromisoformat(NOW[0])
        if tz is None:
            return base
        if base.tzinfo is None:
            return base.replace(tzinfo=tz)
        return base.astimezone(tz)


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    _mod = types.ModuleType("upd_notify_live")
    _mod.__path__ = []
    sys.modules["upd_notify_live"] = _mod
    g = _mod.__dict__
    g.update({"__name__": "upd_notify_live", "dataclasses": dataclasses, "dataclass": dataclasses.dataclass,
              "json": json, "asyncio": asyncio, "logging": logging,
              "datetime": FakeDateTime, "Path": Path, "Any": object, "Optional": object,
              "suppress": contextlib.suppress,
              "logger": logging.getLogger("oracle"),
              "t": lambda key, **kw: key,
              "_log_suppressed": lambda *a, **k: contextlib.suppress()})
    wanted_fn = {"_update_failed_notice", "_update_output_tail"}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted_fn:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name == "GatewayUpdateNotificationsMixin":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             flags=_ANN, dont_inherit=True), g)
            except Exception:
                continue
    return g


G = extract()
MIXIN = G["GatewayUpdateNotificationsMixin"]


def run_case(r):
    f = r.get("func")
    if f == "output_tail":
        return G["_update_output_tail"](r.get("output") or "", int(r.get("limit") or 0))
    if f == "marker_profile":
        return MIXIN._marker_profile(r.get("data") or {})
    if f == "marker_age":
        NOW[0] = r.get("now_iso") or ""
        return MIXIN._marker_age_seconds(r.get("data") or {})
    if f == "read_since":
        data = (r.get("data") or "").encode("utf-8")
        offset = int(r.get("offset") or 0)
        if r.get("io_error"):
            def boom():
                raise OSError("nope")
            fake = types.SimpleNamespace(read_bytes=boom)
        else:
            fake = types.SimpleNamespace(read_bytes=lambda: data)
        text, new_off = MIXIN._read_update_output_since(fake, offset)
        return {"text": text, "offset": new_off}
    if f == "update_paths":
        G["_hermes_home"] = Path(r.get("hermes_home") or "")
        _gw_run._hermes_home = Path(r.get("hermes_home") or "")
        paths = MIXIN._update_paths()
        return {"pending": str(paths.pending), "claimed": str(paths.claimed),
                "output": str(paths.output), "exit_code": str(paths.exit_code),
                "prompt": str(paths.prompt), "response": str(paths.response)}
    if f == "update_exit_code":
        fake = types.SimpleNamespace(
            exit_code=types.SimpleNamespace(read_text=lambda encoding=None: r.get("text") or ""))
        try:
            return MIXIN._update_exit_code(fake)
        except ValueError:
            return None  # int() would raise; the C port answers null on the same input
    return None


def main():
    cases = mism = 0
    known = {"output_tail", "marker_profile", "marker_age", "read_since", "update_paths",
             "update_exit_code"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in known:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        same = py == got
        if not same and isinstance(py, float) and isinstance(got, (int, float)):
            same = abs(py - got) <= 1e-6
        if not same:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
