#!/usr/bin/env python3
"""Oracle for the hermes_cli/cli_tui_mixin.py unclaimed-remainder FLOW port
(src/cli/cli_tui_mixin_flow2.c).

Drives the LIVE CLITuiMixin prompt/placeholder/hint display methods, the small
fragment delegators, the Tab gesture, palette open, stash-panel restore, the
five key-handler factories and Ctrl+Z with SCRIPTED I/O: every terminal /
skin / profile / i18n / stash / app dependency is a recording stub fed the
same scripts the C harness feeds its seams. Compares the DECISION SEQUENCE
(ordered {ev, args, ret} seam events in "trace"), the return ("ret") and the
mutated CLI-instance mirror ("state"). Claimed siblings are SEAMED
(self._audio_level_bar, self._tui_restore_stash_payload — ported in
cli_tui_mixin_flow.c); the composed helpers _get_tui_prompt_symbols /
_build_tui_style_dict (ported HERE) stay LIVE-bound so nested decision logic
is exercised, matching the C port's inline composition. The real
staticmethod _tui_set_filter (claimed in flow1) stays LIVE (C inlines it).

Live defs are AST-extracted (module Assign constants + class Assign tables +
class FunctionDef exec with the future-annotations flag); the heavy quarry
import chain is never imported — in-function imports resolve against stub
modules whose __getattr__ models a scripted import failure (records nothing,
raises). The bare `import signal` is gated by a meta_path blocker when
scripted (a bare import accesses no attribute). Scripted raises round-trip
BARE via an identity map (a scripted KeyError keeps its raw message; an
unscripted live raise is reconstructed as {type, str(e)}). BaseException
kinds (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError) are REAL
builtins and PASS THROUGH every except-Exception site. Fail-closed: 0 cases
=> exit 1. Summary: 'ORACLE cli_tui_mixin_flow2: N cases, M mismatches'.

Representation contract (both sides):
- tuples normalize to lists; integral floats to ints; sets to sorted lists.
- _clarify_state.selected_indices is a sorted unique int list (live set).
- an uncaught exception rides as {"__raise__": {type, message}}.
- scripted numbers are integral (py type name of a number is "int");
  style-dict values / badges results are str|null|number|bool; filter strings
  are str|null; complete_state is null or a NON-EMPTY object (never {});
  handler idx >= 0; ctrl_z "platform" is explicit per case.
- import scripts ("import:<module>" keys) must be SINGLE entries, not queues.
- cli._DIM = "\\x1b[2;3m" / cli._RST = "\\x1b[0m" are PINNED constants (not
  scripted); run_in_terminal is a pure scripted seam (the _suspend closure
  body os.write/os.kill is process-level, out of the scripted scope).
"""
import ast
import asyncio  # pre-cache so make_exc(CancelledError) never imports mid-case
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
    "get_tui_prompt_symbols": "_get_tui_prompt_symbols",
    "get_tui_prompt_fragments": "_get_tui_prompt_fragments",
    "build_tui_style_dict": "_build_tui_style_dict",
    "apply_tui_skin_style": "_apply_tui_skin_style",
    "get_extra_tui_widgets": "_get_extra_tui_widgets",
    "tui_hint_text": "_tui_hint_text",
    "tui_placeholder_text": "_tui_placeholder_text",
    "get_stash_panel_display_fragments": "_get_stash_panel_display_fragments",
    "tui_image_bar_fragments": "_tui_image_bar_fragments",
    "tui_voice_status_fragments": "_tui_voice_status_fragments",
    "tui_spinner_text": "_tui_spinner_text",
    "tui_spinner_height": "_tui_spinner_height",
    "tui_hint_height": "_tui_hint_height",
    "handle_tab": "_tui_handle_tab",
    "open_command_palette": "_tui_open_command_palette",
    "handle_stash_panel_restore": "_tui_handle_stash_panel_restore",
    "make_clarify_number_handler": "_tui_make_clarify_number_handler",
    "make_model_filter_char_handler": "_tui_make_model_filter_char_handler",
    "make_palette_char_handler": "_tui_make_palette_char_handler",
    "make_approval_number_handler": "_tui_make_approval_number_handler",
    "make_slash_confirm_number_handler": "_tui_make_slash_confirm_number_handler",
    "handle_ctrl_z": "_tui_handle_ctrl_z",
}
EVENT_FNS = {"handle_tab", "open_command_palette", "handle_stash_panel_restore",
             "handle_ctrl_z"}
FACTORY_IDX = {"make_clarify_number_handler", "make_approval_number_handler",
               "make_slash_confirm_number_handler"}
FACTORY_CH = {"make_model_filter_char_handler", "make_palette_char_handler"}

SELF_FIELDS = [
    "_voice_recording", "_voice_processing", "_voice_mode", "_voice_live_text",
    "_sudo_state", "_secret_state", "_connection_state", "_approval_state",
    "_slash_confirm_state", "_clarify_state", "_clarify_freetext",
    "_clarify_deadline", "_sudo_deadline", "_secret_deadline",
    "_approval_deadline", "_slash_confirm_deadline",
    "_command_running", "_command_blocks_input", "_command_status",
    "_agent_running", "_modern_layout", "busy_input_mode",
    "_attached_images", "_image_counter", "_tui_style_base",
    "_app", "_prompt_stash", "_composer_placeholder",
    "_model_picker_state", "_command_palette_state",
]
# fields whose state dict IS the mirror (mutated in place by the doubles)
NO_COLLECT = {"_app", "_prompt_stash"}

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
        e = asyncio.CancelledError(msg)
    elif t == "KeyError":
        e = KeyError(msg)
    elif t == "IndexError":
        e = IndexError(msg)
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
    elif t == "ImportError":
        e = ImportError(msg)
    elif t == "ModuleNotFoundError":
        e = ModuleNotFoundError(msg)
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
    elif isinstance(_node, ast.Assign) and all(isinstance(t, ast.Name) for t in _node.targets):
        # class-level tables referenced through self (_TUI_MODAL_HINTS, ...)
        _exec_node(_node)
for _node in _tree.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in _wanted:
        _exec_node(_node)
        _found.add(_node.name)
_missing = _wanted - _found
if _missing:
    print(f"ORACLE cli_tui_mixin_flow2: FATAL missing live defs: {sorted(_missing)}")
    sys.exit(1)
for _tbl in ("_TUI_MODAL_HINTS", "_TUI_CONNECTION_HINT_KEYS"):
    if _tbl not in ns:
        print(f"ORACLE cli_tui_mixin_flow2: FATAL missing class table {_tbl}")
        sys.exit(1)


# ── doubles ──────────────────────────────────────────────────────────────
class CompleteStateDouble:
    def __init__(self, d):
        self._d = d

    def __bool__(self):
        return bool(self._d)

    @property
    def current_completion(self):
        return self._d.get("current_completion")


class SuggestionDouble:
    def __init__(self, d):
        self._d = d

    def __bool__(self):
        return bool(self._d)

    @property
    def text(self):
        return self._d.get("text", "")


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

    @property
    def complete_state(self):
        cs = self._b.get("complete_state")
        return CompleteStateDouble(cs) if isinstance(cs, dict) else None

    @property
    def suggestion(self):
        s = self._b.get("suggestion")
        return SuggestionDouble(s) if isinstance(s, dict) else None

    def go_to_completion(self, index):
        return self._stub("buffer.go_to_completion", {"index": index})

    def apply_completion(self, completion):
        return self._stub("buffer.apply_completion", {"completion": completion})

    def start_completion(self):
        return self._stub("buffer.start_completion", {})

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


class AppDouble:
    def __init__(self, buf, rec, style_mirror):
        self.current_buffer = buf
        self._rec = rec
        self._style_mirror = style_mirror
        self.style = style_mirror.get("style") if isinstance(style_mirror, dict) else None

    def __bool__(self):
        return bool(self._style_mirror)

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
    app_mirror = state.get("_app") if isinstance(state.get("_app"), dict) else None
    app = AppDouble(buf, rec, app_mirror)
    event = types.SimpleNamespace(app=app, current_buffer=buf,
                                  arg=inp.get("arg", 1), data=inp.get("data"))

    # self double
    sd = types.SimpleNamespace()
    for k in SELF_FIELDS:
        setattr(sd, k, state.get(k))
    sd._app = app if state.get("_app") is not None else None
    # class-level tables referenced through self
    sd._TUI_MODAL_HINTS = ns["_TUI_MODAL_HINTS"]
    sd._TUI_CONNECTION_HINT_KEYS = ns["_TUI_CONNECTION_HINT_KEYS"]

    # seamed self methods (claimed siblings + unported collaborators)
    sd._use_minimal_tui_chrome = lambda width: stub("self._use_minimal_tui_chrome", {"width": width})
    sd._get_tui_terminal_width = lambda: stub("self._get_tui_terminal_width", {})
    sd._audio_level_bar = lambda: stub("self._audio_level_bar", {})
    sd._command_spinner_frame = lambda: stub("self._command_spinner_frame", {})
    sd._invalidate = lambda min_interval: stub("self._invalidate", {"min_interval": min_interval})
    sd._voice_record_key_label = lambda: stub("self._voice_record_key_label", {})
    sd._render_stash_panel = lambda rows, cursor, width: stub(
        "self._render_stash_panel", {"rows": rows, "cursor": cursor, "width": width})
    sd._get_voice_status_fragments = lambda: stub("self._get_voice_status_fragments", {})
    sd._render_spinner_text = lambda: stub("self._render_spinner_text", {})
    sd._spinner_widget_height = lambda: stub("self._spinner_widget_height", {})
    sd._agent_spacer_height = lambda: stub("self._agent_spacer_height", {})
    sd._open_command_palette = lambda: stub("self._open_command_palette", {})
    sd._tui_restore_stash_payload = lambda ev, payload: stub(
        "self._tui_restore_stash_payload", {"payload": payload})
    sd._clarify_batch_lock = lambda st, choice: stub(
        "self._clarify_batch_lock", {"state": st, "choice": choice})
    sd._handle_approval_selection = lambda: stub("self._handle_approval_selection", {})
    sd._submit_slash_confirm_response = lambda value: stub(
        "self._submit_slash_confirm_response", {"value": value})
    # LIVE-bound composed helpers (ported here) + real staticmethod (flow1)
    sd._get_tui_prompt_symbols = types.MethodType(ns["_get_tui_prompt_symbols"], sd)
    sd._build_tui_style_dict = types.MethodType(ns["_build_tui_style_dict"], sd)
    sd._tui_set_filter = ns["_tui_set_filter"]
    # stash object double (methods seamed; panel_cursor attribute read)
    if state.get("_prompt_stash") is not None:
        d = state["_prompt_stash"]
        kw = dict(panel_rows=lambda: stub("prompt_stash.panel_rows", {}),
                  placeholder_hint=lambda: stub("prompt_stash.placeholder_hint", {}),
                  restore_at_cursor=lambda: stub("prompt_stash.restore_at_cursor", {}))
        if isinstance(d, dict) and "panel_cursor" in d:
            kw["panel_cursor"] = d["panel_cursor"]
        sd._prompt_stash = types.SimpleNamespace(**kw)

    # stub modules for in-function imports (import failure = untraced raise)
    def import_check(key):
        e = sc.next(key)
        if isinstance(e, dict) and isinstance(e.get("import_raise"), dict):
            raise_exc(e["import_raise"])

    cli_mod = types.ModuleType("cli")

    def cli_getattr(name):
        import_check("import:cli")
        if name == "_DIM":
            return "\x1b[2;3m"
        if name == "_RST":
            return "\x1b[0m"
        if name == "_cprint":
            return lambda text: stub("_cprint", {"text": text})
        if name == "_detect_light_mode":
            return lambda: stub("_detect_light_mode", {})
        if name == "_maybe_remap_for_light_mode":
            return lambda tok: stub("_maybe_remap_for_light_mode", {"token": tok})
        if name == "_format_image_attachment_badges":
            return lambda images, counter: stub(
                "_format_image_attachment_badges", {"images": images, "image_counter": counter})
        raise AttributeError(name)

    cli_mod.__getattr__ = cli_getattr

    skin_mod = types.ModuleType("hermes_cli.skin_engine")

    def skin_getattr(name):
        import_check("import:hermes_cli.skin_engine")
        if name == "get_active_prompt_symbol":
            return lambda default: stub("get_active_prompt_symbol", {"default": default})
        if name == "get_prompt_toolkit_style_overrides":
            return lambda: stub("get_prompt_toolkit_style_overrides", {})
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

    profiles_mod = types.ModuleType("hermes_cli.profiles")

    def profiles_getattr(name):
        import_check("import:hermes_cli.profiles")
        if name == "get_active_profile_name":
            return lambda: stub("get_active_profile_name", {})
        raise AttributeError(name)

    profiles_mod.__getattr__ = profiles_getattr

    pt_pkg = types.ModuleType("prompt_toolkit")
    pt_pkg.__path__ = []
    pt_app_mod = types.ModuleType("prompt_toolkit.application")

    def pt_app_getattr(name):
        import_check("import:prompt_toolkit.application")
        if name == "run_in_terminal":
            # pure scripted seam: the _suspend closure body is out of scope
            return lambda func: stub("run_in_terminal", {"closure": "_suspend"})
        raise AttributeError(name)

    pt_app_mod.__getattr__ = pt_app_getattr

    hc_pkg = types.ModuleType("hermes_cli")
    hc_pkg.__path__ = []
    sys.modules["hermes_cli"] = hc_pkg
    sys.modules["hermes_cli.skin_engine"] = skin_mod
    sys.modules["hermes_cli.profiles"] = profiles_mod
    sys.modules["cli"] = cli_mod
    sys.modules["prompt_toolkit"] = pt_pkg
    sys.modules["prompt_toolkit.application"] = pt_app_mod

    # bare `import signal as _sig`: no attribute access -> meta_path blocker
    blocker = None
    sig_entry = sc.next("import:signal")
    if isinstance(sig_entry, dict) and isinstance(sig_entry.get("import_raise"), dict):
        sig_spec = sig_entry["import_raise"]

        class _Blocker:
            def find_spec(self, fullname, path=None, target=None):
                if fullname == "signal":
                    raise_exc(sig_spec)
                return None

        blocker = _Blocker()
        sys.meta_path.insert(0, blocker)
        sys.modules.pop("signal", None)
    else:
        sys.modules["signal"] = types.ModuleType("signal")

    # module-level names the extracted defs read (rebound per case)
    ns["t"] = lambda key, **kw: stub("t", {"key": key, **kw})
    ns["time"] = types.SimpleNamespace(
        time=lambda: stub("time.time", {}),
        monotonic=lambda: stub("time.monotonic", {}))
    ns["PTStyle"] = types.SimpleNamespace(
        from_dict=lambda d: stub("PTStyle.from_dict", {"style_dict": d}))
    ns["sys"] = types.SimpleNamespace(platform=inp.get("platform", "win32"))

    f = ns[PY[func]]
    try:
        if func in FACTORY_IDX:
            handler = f(sd, inp.get("idx", 0))
            ret = handler(event)
        elif func in FACTORY_CH:
            handler = f(sd, inp.get("ch", ""))
            ret = handler(event)
        elif func in EVENT_FNS:
            ret = f(sd, event)
        else:
            ret = f(sd)
        ret = norm(ret)
    except BaseException as e:
        if id(e) in exc_map:
            ret = {"__raise__": exc_map[id(e)]}
        else:
            ret = {"__raise__": {"type": type(e).__name__, "message": str(e)}}
    finally:
        if blocker is not None:
            try:
                sys.meta_path.remove(blocker)
            except ValueError:
                pass

    for k in SELF_FIELDS:
        if k not in NO_COLLECT:
            state[k] = getattr(sd, k)
    if isinstance(state.get("_app"), dict):
        state["_app"]["style"] = app.style
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
        print("ORACLE cli_tui_mixin_flow2: 0 cases (fail-closed)")
        sys.exit(1)
    print(f"ORACLE cli_tui_mixin_flow2: {n} cases, {mism} mismatches")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()