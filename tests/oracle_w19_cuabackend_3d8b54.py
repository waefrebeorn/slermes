#!/usr/bin/env python3
"""Differential oracle for tools/computer_use/cua_backend_input.py (gate w19_cuabackend_3d8b54)."""
import ast, json, re, sys, types, typing, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "tools" / "computer_use" / "cua_backend_input.py"
PARSE_SRC = REPO.parent / "hermes-agent" / "tools" / "computer_use" / "cua_backend_parse.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_cuabackend_3d8b54"


class ActionResult:
    """Stub for tools.computer_use.backend.ActionResult - the fields these methods set."""

    def __init__(self, ok=True, action="", message="", meta=None, **fields):
        self.ok = bool(ok)
        self.action = action
        self.message = message
        self.meta = dict(meta or {})
        self.fields = fields

    def dump(self):
        d = {"ok": self.ok, "action": self.action, "message": self.message, "meta": self.meta}
        d.update(self.fields)
        return d


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    mod = types.ModuleType("cua_input_live")
    mod.__dict__["__builtins__"] = __builtins__
    sys.modules["cua_input_live"] = mod
    g = mod.__dict__
    g.update(dict(ActionResult=ActionResult, Any=typing.Any, Callable=typing.Callable, Dict=dict,
                  List=list, Optional=typing.Optional, Sequence=typing.Sequence, Tuple=tuple,
                  Union=typing.Union))
    for node in tree.body:
        if isinstance(node, ast.Assign):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name == "_InputMixin":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == "_refuse":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    # the key-combo parser this module imports from cua_backend_parse
    ptree = ast.parse(PARSE_SRC.read_text(encoding="utf-8"))
    g["re"] = re
    for node in ptree.body:
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if any(n in {"_MODIFIER_NAMES", "_KEY_ALIASES"} for n in names):
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(PARSE_SRC), "exec",
                             flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.FunctionDef) and node.name == "_parse_key_combo":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(PARSE_SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
M = G["_InputMixin"]


class FakeSession:
    def __init__(self, r):
        self._r = r

    def supports_input_property(self, action, prop):
        return bool(self._r.get("supports_delivery_mode"))

    def supports_capability(self, cap, tool=None):
        return bool(self._r.get("supports_scroll_coords"))

    def _has_tool(self, name):
        return bool(self._r.get("has_btf_tool"))


def _host_init(self, r):
    self._r = r
    self._active_pid = r.get("pid")
    self._active_window_id = r.get("window_id")
    self._session = FakeSession(r)


def _host_action(self, action, args):
    ok = bool(self._r.get("action_ok", True))
    return ActionResult(ok=ok, action=action, message="" if ok else "action failed",
                        sent=json.loads(json.dumps(args)))


def _host_btf(self, *, pid, window_id):
    if not self._r.get("btf_ok", True):
        return ActionResult(ok=False, action="bring_to_front", message="focus failed")
    return ActionResult(ok=True, action="bring_to_front", message="",
                        sent={"pid": pid, "window_id": window_id})


# A real _InputMixin subclass, so the methods' own self._target_args / self._apply_delivery /
# self._pointer_args calls run the live helper implementations.
Host = type("FakeHost", (M,), {"__init__": _host_init, "_action": _host_action,
                               "bring_to_front": _host_btf})


def kw(r, *names):
    return {n: r[n] for n in names if n in r}


def run_case(r):
    f = r.get("func")
    fs = Host(r)
    if f == "refuse":
        fields = {}
        if "code" in r:
            fields["code"] = r["code"]
        if "delivery_mode" in r:
            fields["delivery_mode"] = r["delivery_mode"]
        return G["_refuse"](r.get("action", ""), r.get("message", ""), **fields).dump()
    if f == "target_args":
        refusal, args = fs._target_args(r.get("action", ""), need_window=bool(r.get("need_window", False)))
        return {"refusal": refusal.dump() if refusal is not None else None, "args": args}
    if f == "apply_delivery":
        args = {}
        refusal = fs._apply_delivery(r.get("action", ""), args, r.get("delivery_mode"))
        return {"refusal": refusal.dump() if refusal is not None else None, "args": args}
    if f == "parse_key_combo":
        key, modifiers = G["_parse_key_combo"](r.get("keys", ""))
        return {"key": key, "modifiers": modifiers}
    if f == "run_input_action":
        return fs._run_input_action(r.get("action", ""), dict(r.get("args") or {}),
                                    r.get("delivery_mode"), bool(r.get("bring_to_front", False))).dump()
    if f == "click":
        return fs.click(**kw(r, "element", "x", "y", "button", "click_count", "modifiers",
                             "delivery_mode", "bring_to_front")).dump()
    if f == "drag":
        return fs.drag(**kw(r, "from_element", "to_element", "from_xy", "to_xy", "button",
                            "modifiers", "delivery_mode", "bring_to_front")).dump()
    if f == "scroll":
        return fs.scroll(direction=r.get("direction", "down"),
                         **kw(r, "amount", "element", "x", "y", "modifiers", "delivery_mode",
                              "bring_to_front")).dump()
    if f == "type_text":
        return fs.type_text(r.get("text", ""), **kw(r, "delivery_mode", "bring_to_front")).dump()
    if f == "key":
        return fs.key(r.get("keys", ""), **kw(r, "delivery_mode", "bring_to_front")).dump()
    if f == "set_value":
        return fs.set_value(r.get("value", ""), r.get("element")).dump()
    return None


FUNCS = {"refuse", "target_args", "apply_delivery", "parse_key_combo", "run_input_action",
         "click", "drag", "scroll", "type_text", "key", "set_value"}


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
        if f not in FUNCS:
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
            print("MISMATCH %s args=%r py=%r c=%r" % (f, r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or not cases) else 0)


if __name__ == "__main__":
    main()
