#!/usr/bin/env python3
"""Oracle for the hermes_cli/cli_tui_mixin.py navigation/editor/stash FLOW port
(src/cli/cli_tui_mixin_flow.c).

Drives the LIVE CLITuiMixin navigation/editor/stash methods + module leaves
with SCRIPTED I/O: every terminal / skin / i18n / stash dependency is a
recording stub fed the same scripts the C harness feeds its seams. Compares
the DECISION SEQUENCE (ordered {ev, args, ret} seam events in "trace"), the
return ("ret") and the mutated CLI-instance mirror ("state"). The claimed
sibling _tui_connection_move is SEAMED (ported in port_tui_dispatch_flow.c);
the composed helpers _tui_recall_without_recollapse / _tui_clarify_batch_step
(ported here) stay LIVE-bound so nested decision logic is exercised, matching
the C port's inline composition. The real staticmethod _tui_set_filter stays
LIVE (C inlines it).

Live defs are AST-extracted (module Assign/AnnAssign constants + class
FunctionDef exec with the future-annotations flag); the heavy quarry import
chain is never imported — in-function imports resolve against stub modules
whose __getattr__ models a scripted import failure (records nothing, raises).
Scripted raises round-trip BARE via an identity map (a scripted KeyError keeps
its raw message; an unscripted live raise is reconstructed as {type, str(e)}).
BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError)
are REAL builtins and PASS THROUGH every except-Exception site. Fail-closed:
0 cases => exit 1. Summary: 'ORACLE cli_tui_mixin_flow: N cases, M mismatches'.

Representation contract (both sides):
- tuples normalize to lists; integral floats to ints; sets to sorted lists.
- _clarify_state.selected_indices is a sorted unique int list (live set).
- an uncaught exception rides as {"__raise__": {type, message}}.
- scripted numbers are integral; restore_stash_payload payload is a 2-list or
  null; image ids are strings; _audio_level_bar current_rms >= 0.
- import scripts ("import:<module>" keys) must be SINGLE entries, not queues.
"""
import ast
import copy
import json
import os
import string
import sys
import types
import __future__
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
_HOME = _HERE.parents[2]
os.environ.setdefault("HOME", str(_HOME))
os.environ.setdefault("USERPROFILE", str(_HOME))
os.environ.setdefault("LOCALAPPDATA", str(_HOME / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
QUARRY = _HOME / "hermes-agent"
if not (QUARRY / "hermes_cli" / "cli_tui_mixin.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "cli_tui_mixin.py")

PY = {
    "approval_choice_label": "_approval_choice_label",
    "agent_name": "_agent_name",
    "tn": "_tn",
    "num_prefix": "_num_prefix",
    "term_rows": "_term_rows",
    "set_filter": "_tui_set_filter",
    "audio_level_bar": "_audio_level_bar",
    "connection_up": "_tui_connection_up",
    "connection_down": "_tui_connection_down",
    "connection_side": "_tui_connection_side",
    "model_picker_down": "_tui_model_picker_down",
    "model_picker_up": "_tui_model_picker_up",
    "model_picker_filter_backspace": "_tui_model_picker_filter_backspace",
    "clarify_toggle": "_tui_clarify_toggle",
    "clarify_down": "_tui_clarify_down",
    "clarify_up": "_tui_clarify_up",
    "clarify_batch_step": "_tui_clarify_batch_step",
    "clarify_batch_tab": "_tui_clarify_batch_tab",
    "clarify_batch_backtab": "_tui_clarify_batch_backtab",
    "command_palette_backspace": "_tui_command_palette_backspace",
    "command_palette_down": "_tui_command_palette_down",
    "command_palette_up": "_tui_command_palette_up",
    "command_palette_enter": "_tui_command_palette_enter",
    "command_palette_escape": "_tui_command_palette_escape",
    "slash_confirm_down": "_tui_slash_confirm_down",
    "slash_confirm_up": "_tui_slash_confirm_up",
    "approval_down": "_tui_approval_down",
    "approval_up": "_tui_approval_up",
    "handle_double_escape": "_tui_handle_double_escape",
    "handle_alt_v": "_tui_handle_alt_v",
    "handle_ctrl_v": "_tui_handle_ctrl_v",
    "handle_ctrl_l": "_tui_handle_ctrl_l",
    "insert_newline": "_tui_insert_newline",
    "handle_open_in_editor": "_tui_handle_open_in_editor",
    "recall_without_recollapse": "_tui_recall_without_recollapse",
    "history_up": "_tui_history_up",
    "history_down": "_tui_history_down",
    "wake_startup": "_tui_wake_startup",
    "handle_ignored_terminal_sequence": "_tui_handle_ignored_terminal_sequence",
    "stash_panel_up": "_tui_handle_stash_panel_up",
    "stash_panel_down": "_tui_handle_stash_panel_down",
    "stash_panel_delete": "_tui_handle_stash_panel_delete",
    "stash_panel_close": "_tui_handle_stash_panel_close",
    "restore_stash_payload": "_tui_restore_stash_payload",
}

# module functions (no self) vs methods
MODULE_FNS = {"approval_choice_label", "agent_name", "tn", "num_prefix", "term_rows"}

SELF_FIELDS = [
    "_connection_state", "_model_picker_state", "_clarify_state", "_clarify_freetext",
    "_command_palette_state", "_slash_confirm_state", "_approval_state",
    "_attached_images", "_skip_paste_collapse", "_voice_recorder", "_prompt_stash",
]
# fields live only READS or calls seamed methods on (no rebind) -> keep raw state
NO_COLLECT = {"_voice_recorder", "_prompt_stash"}

OSERROR_FAMILY = {
    "OSError", "PermissionError", "FileNotFoundError", "FileExistsError",
    "BlockingIOError", "ChildProcessError", "ConnectionError", "BrokenPipeError",
    "ConnectionAbortedError", "ConnectionRefusedError", "ConnectionResetError",
    "InterruptedError", "IsADirectoryError", "NotADirectoryError",
    "ProcessLookupError", "TimeoutError",
}
BASE_EXC = {"SystemExit", "KeyboardInterrupt", "GeneratorExit", "CancelledError"}


def make_exc(spec):
    spec = spec or {}
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if t == "SystemExit":
        e = SystemExit(msg)
    elif t == "KeyboardInterrupt":
        e = KeyboardInterrupt(msg)
    elif t == "GeneratorExit":
        e = GeneratorExit(msg)
    elif t == "CancelledError":
        from asyncio import CancelledError
        e = CancelledError(msg)
    elif t == "KeyError":
        e = KeyError(msg)
    elif t == "RuntimeError":
        e = RuntimeError(msg)
    elif t == "RecursionError":
        e = RecursionError(msg)
    elif t == "ValueError":
        e = ValueError(msg)
    elif t == "TypeError":
        e = TypeError(msg)
    elif t == "AttributeError":
        e = AttributeError(msg)
    elif t == "ZeroDivisionError":
        e = ZeroDivisionError(msg)
    elif t in OSERROR_FAMILY:
        cls = {"OSError": OSError, "PermissionError": PermissionError,
               "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
               "TimeoutError": TimeoutError}.get(t, OSError)
        e = cls(msg)
    else:
        e = type(t, (Exception,), {})(msg)
    if spec.get("errno") is not None:
        try:
            e.errno = spec["errno"]
        except Exception:
            pass
    return e


def exc_spec(e):
    return {"type": type(e).__name__, "message": str(e),
            "errno": getattr(e, "errno", None)}


# ── AST extraction + namespace seeding ───────────────────────────────────
_src_text = Path(SRC).read_text(encoding="utf-8")
_tree = ast.parse(_src_text)
FLAGS = __future__.annotations.compiler_flag
ns = {
    "json": json, "string": string, "os": os, "sys": sys,
    "shutil": types.SimpleNamespace(), "Optional": object,
}


def _exec_node(node):
    exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=FLAGS), ns)


for _node in _tree.body:
    if isinstance(_node, ast.Assign) and all(isinstance(t, ast.Name) for t in _node.targets):
        _exec_node(_node)
    elif isinstance(_node, ast.AnnAssign) and isinstance(_node.target, ast.Name) and _node.value is not None:
        _exec_node(_node)

_wanted = set(PY.values())
_found = set()
_cls = next(n for n in _tree.body if isinstance(n, ast.ClassDef) and n.name == "CLITuiMixin")
for _node in _cls.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in _wanted:
        _exec_node(_node)
        _found.add(_node.name)
for _node in _tree.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in _wanted:
        _exec_node(_node)
        _found.add(_node.name)
_missing = _wanted - _found
if _missing:
    print(f"ORACLE cli_tui_mixin_flow: FATAL missing live defs: {sorted(_missing)}")
    sys.exit(1)


# ── doubles ──────────────────────────────────────────────────────────────
class BufDouble:
    def __init__(self, b, rec):
        self._b = b
        self._rec = rec

    @property
    def text(self):
        return self._b.get("text", "")

    @text.setter
    def text(self, v):
        self._b["text"] = v

    @property
    def cursor_position(self):
        return self._b.get("cursor_position", 0)

    @cursor_position.setter
    def cursor_position(self, v):
        self._b["cursor_position"] = v

    def reset(self, append_to_history=False):
        self._rec("buffer.reset", {"append_to_history": append_to_history}, None)
        self._b["text"] = ""
        self._b["cursor_position"] = 0

    def insert_text(self, s):
        self._rec("buffer.insert_text", {"text": s}, None)
        t = self._b.get("text", "")
        ci = self._b.get("cursor_position", 0)
        ci = max(0, min(ci, len(t)))
        self._b["text"] = t[:ci] + s + t[ci:]
        self._b["cursor_position"] = ci + len(s)

    def auto_up(self, count=1):
        return self._stub("buffer.auto_up", {"count": count})

    def auto_down(self, count=1):
        return self._stub("buffer.auto_down", {"count": count})


class AppDouble:
    def __init__(self, buf, rec):
        self.current_buffer = buf
        self._rec = rec

    def invalidate(self):
        self._rec("app.invalidate", {}, None)


def norm(o):
    if isinstance(o, Exception):
        return exc_spec(o)
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in vars(o).items() if not callable(v)}
    if isinstance(o, (set, frozenset)):
        return sorted(norm(x) for x in o)
    if isinstance(o, tuple):
        return [norm(x) for x in o]
    if isinstance(o, list):
        return [norm(x) for x in o]
    if isinstance(o, dict):
        return {k: norm(v) for k, v in o.items()}
    if isinstance(o, float) and o.is_integer():
        return int(o)
    return o


class ScriptCtx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}

    def next(self, name):
        e = self.io.get(name)
        if e is None:
            return None
        if isinstance(e, list):
            i = self.cursors.get(name, 0)
            if i >= len(e):
                return None
            self.cursors[name] = i + 1
            return e[i]
        return e


def run_case(func, inp):
    state = copy.deepcopy(inp.get("state") or {})
    trace = []
    sc = ScriptCtx(inp.get("io"))
    exc_map = {}          # id(exception) -> scripted spec (bare round-trip)
    holder = {}

    def rec(name, args, ret):
        trace.append({"ev": name, "args": norm(args), "ret": norm(copy.deepcopy(ret))})

    def raise_exc(spec):
        e = make_exc(spec)
        exc_map[id(e)] = spec
        raise e

    def apply_effects(eff):
        for k, v in (eff or {}).items():
            v = copy.deepcopy(v)
            if k == "buf" and isinstance(v, dict) and isinstance(state.get("buf"), dict):
                state["buf"].update(v)        # in place: BufDouble stays attached
            else:
                state[k] = v

    def stub(name, args):
        e = sc.next(name)
        if e is None:
            rec(name, args, None)
            return None
        if isinstance(e, dict):
            apply_effects(e.get("state"))
            if isinstance(e.get("import_raise"), dict):
                raise_exc(e["import_raise"])
            if isinstance(e.get("raise"), dict):
                rec(name, args, {"__raise__": e["raise"]})
                raise_exc(e["raise"])
            ret = copy.deepcopy(e.get("ret"))
            rec(name, args, ret)
            return ret
        rec(name, args, e)
        return copy.deepcopy(e)

    # materialize selected_indices list -> set (live set semantics)
    cs = state.get("_clarify_state")
    if isinstance(cs, dict) and isinstance(cs.get("selected_indices"), list):
        cs["selected_indices"] = set(cs["selected_indices"])

    buf = BufDouble(state.setdefault("buf", {"text": "", "cursor_position": 0}), rec)
    buf._stub = stub
    app = AppDouble(buf, rec)

    # event double
    ks = inp.get("key_sequence")
    if isinstance(ks, list):
        ks = [types.SimpleNamespace(**p) if isinstance(p, dict) else p for p in ks]
    event = types.SimpleNamespace(app=app, current_buffer=buf,
                                  arg=inp.get("arg", 1), key_sequence=ks)

    # self double
    sd = types.SimpleNamespace()
    for k in SELF_FIELDS:
        v = state.get(k)
        if k == "_voice_recorder" and isinstance(v, dict):
            v = types.SimpleNamespace(**v)
        setattr(sd, k, v)
    holder["sd"] = sd

    # seamed self methods (claimed sibling + unported collaborators)
    sd._tui_connection_move = lambda ev, delta: stub("self._tui_connection_move", {"delta": delta})
    sd._clarify_batch_set_active = lambda st, active: stub(
        "self._clarify_batch_set_active", {"state": st, "active": active})
    sd._paint_now = lambda: stub("self._paint_now", {})
    sd._try_attach_clipboard_image = lambda: stub("self._try_attach_clipboard_image", {})
    sd._force_full_redraw = lambda: stub("self._force_full_redraw", {})
    sd._open_external_editor = lambda b: stub("self._open_external_editor", {"text": b.text})
    sd._maybe_start_wake_word = lambda: stub("self._maybe_start_wake_word", {})
    sd._schedule_focus_regain_redraw = lambda: stub("self._schedule_focus_regain_redraw", {})
    sd._handle_command_palette_selection = lambda: stub("self._handle_command_palette_selection", {})
    sd._close_command_palette = lambda: stub("self._close_command_palette", {})
    sd._command_palette_visible_entries = lambda: stub("self._command_palette_visible_entries", {})
    # LIVE-bound composed helpers (ported here) + real staticmethod
    sd._tui_recall_without_recollapse = types.MethodType(ns["_tui_recall_without_recollapse"], sd)
    sd._tui_clarify_batch_step = types.MethodType(ns["_tui_clarify_batch_step"], sd)
    sd._tui_set_filter = ns["_tui_set_filter"]
    # stash object double (methods seamed); absent -> AttributeError on access
    if state.get("_prompt_stash") is not None:
        sd._prompt_stash = types.SimpleNamespace(
            move_cursor=lambda delta: stub("prompt_stash.move_cursor", {"delta": delta}),
            delete_at_cursor=lambda: stub("prompt_stash.delete_at_cursor", {}),
            close_panel=lambda: stub("prompt_stash.close_panel", {}))

    # stub modules for in-function imports (import failure = untraced raise)
    def import_check(key):
        e = sc.next(key)
        if isinstance(e, dict) and isinstance(e.get("import_raise"), dict):
            raise_exc(e["import_raise"])

    skin_mod = types.ModuleType("hermes_cli.skin_engine")

    def skin_getattr(name):
        import_check("import:hermes_cli.skin_engine")
        if name == "get_active_skin":
            def get_active_skin():
                r = stub("get_active_skin", {})
                if isinstance(r, dict):
                    return types.SimpleNamespace(
                        get_branding=lambda key, default: stub("get_branding", {"key": key, "default": default}))
                return r
            return get_active_skin
        raise AttributeError(name)

    skin_mod.__getattr__ = skin_getattr

    msm_mod = types.ModuleType("hermes_cli.cli_model_switch_mixin")

    def msm_getattr(name):
        import_check("import:hermes_cli.cli_model_switch_mixin")
        if name == "_picker_reasoning_rows":
            return lambda: stub("_picker_reasoning_rows", {})
        raise AttributeError(name)

    msm_mod.__getattr__ = msm_getattr

    cli_mod = types.ModuleType("cli")

    def cli_getattr(name):
        import_check("import:cli")
        if name == "logger":
            return types.SimpleNamespace(
                debug=lambda msg, *args: stub("logger.debug", {"msg": msg, "args": list(args)}))
        raise AttributeError(name)

    cli_mod.__getattr__ = cli_getattr

    hc_pkg = types.ModuleType("hermes_cli")
    hc_pkg.__path__ = []
    sys.modules["hermes_cli"] = hc_pkg
    sys.modules["hermes_cli.skin_engine"] = skin_mod
    sys.modules["hermes_cli.cli_model_switch_mixin"] = msm_mod
    sys.modules["cli"] = cli_mod

    # module-level names the extracted defs read (rebound per case)
    ns["t"] = lambda key, **kw: stub("t", {"key": key, **kw})
    ns["shutil"].get_terminal_size = lambda fallback: (
        lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r
    )(stub("shutil.get_terminal_size", {"fallback": list(fallback)}))

    f = ns[PY[func]]
    try:
        if func == "set_filter":
            st = copy.deepcopy(inp.get("st") or {})
            f(st, inp.get("value", ""))
            ret = None
            trace_out = []
            state_out = norm(st)
            return {"ret": ret, "trace": trace_out, "state": state_out}
        elif func == "approval_choice_label":
            ret = f(inp.get("choice", ""))
        elif func == "agent_name":
            ret = f()
        elif func == "tn":
            ret = f(inp.get("key", ""), inp.get("count", 0), **(inp.get("kwargs") or {}))
        elif func == "num_prefix":
            ret = f(inp.get("i", 0))
        elif func == "term_rows":
            ret = f()
        elif func == "clarify_batch_step":
            ret = f(sd, event, inp.get("delta", 0))
        elif func == "audio_level_bar":
            ret = f(sd)
        elif func == "wake_startup":
            ret = f(sd)
        elif func == "recall_without_recollapse":
            ret = f(sd, buf, lambda: stub("move", {}))
        elif func == "restore_stash_payload":
            ret = f(sd, event, copy.deepcopy(inp.get("payload")))
        elif func in MODULE_FNS:
            ret = f()
        else:
            ret = f(sd, event)
        ret = norm(ret)
    except BaseException as e:
        if id(e) in exc_map:
            ret = {"__raise__": exc_map[id(e)]}
        else:
            ret = {"__raise__": {"type": type(e).__name__, "message": str(e)}}

    for k in SELF_FIELDS:
        if k not in NO_COLLECT:
            state[k] = getattr(sd, k)
    return {"ret": ret, "trace": trace, "state": norm(state)}


def diff_detail(got, want):
    parts = []
    for k in ("ret", "trace", "state"):
        g, w = got.get(k), want.get(k)
        if g == w:
            continue
        if k == "trace" and isinstance(g, list) and isinstance(w, list):
            i = next((j for j in range(max(len(g), len(w)))
                      if (g[j] if j < len(g) else None) != (w[j] if j < len(w) else None)), 0)
            gj = json.dumps(g[i] if i < len(g) else None, ensure_ascii=False)[:280]
            wj = json.dumps(w[i] if i < len(w) else None, ensure_ascii=False)[:280]
            parts.append(f"trace[{i}] C={gj} PY={wj}")
        else:
            parts.append(f"{k} C={json.dumps(g, ensure_ascii=False)[:280]} "
                         f"PY={json.dumps(w, ensure_ascii=False)[:280]}")
    return "; ".join(parts) or "(unknown)"


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        n += 1
        try:
            want = run_case(func, inp)
        except Exception as e:
            want = {"error": f"{type(e).__name__}: {e}"}
        if got != want:
            mism += 1
            if mism <= 5:
                print(f"MISMATCH {func} case#{n}: {diff_detail(got or {}, want)}")
    if n == 0:
        print("ORACLE cli_tui_mixin_flow: 0 cases (fail-closed)")
        sys.exit(1)
    print(f"ORACLE cli_tui_mixin_flow: {n} cases, {mism} mismatches")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()